/*
  Liquid: Automating fish feeding and watering plants using an Arduno Yun

  2017/01/08
  Alfredo Rius
  alfredo.rius@gmail.com
 */


#define PUMP_PIN 13
#define SERVO_PIN 11
#define FEED_BUTTON_PIN 12
#define PUMP_BUTTON_PIN 2
#define MAX_PUMP_ON 45 // seconds
//#define SPRINKLER_PIN 3
//#define PIEZO_PIN 7

#define PUMP_PERIOD 3600

#define FEED_PERIOD 12 //Divides PUMP_PERIOD, 12 means every 12 hours

#define SERVO_MAX 85
#define SERVO_MIN 65

#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include <Servo.h>
#include <LiquidCrystal.h>

// Listen on default port 5555, the webserver on the YÃºn
// will forward there all the HTTP requests for us.
BridgeServer server;
Servo servo;
LiquidCrystal lcd(4, 5, 6, 8, 9, 10);

// Define characters for LCD
byte block5Char[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
byte block4Char[8] = {0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE};
byte block3Char[8] = {0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC};
byte block2Char[8] = {0xF8,0xF8,0xF8,0xF8,0xF8,0xF8,0xF8,0xF8};
byte block1Char[8] = {0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0};
byte block0Char[8] = {0xE0,0xE0,0xE0,0xE0,0xE0,0xE0,0xE0,0xE0};


String startString;
long hits = 0;
unsigned long int pumpOff = 0;
unsigned long int pumpOn = 0;
unsigned long int currentTime = 0;
int delayCounter = 0;
int feedCounter = 2;
int pumpOnTime = 0;
boolean pumpStatus;
//uint8_t sprinkler = 0; //How many times the sprinkler was activated


void setup() {
  // Bridge startup
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  //pinMode(SPRINKLER_PIN, OUTPUT);
  //digitalWrite(SPRINKLER_PIN, LOW);
  Bridge.begin();

  // Start Server
  server.listenOnLocalhost();
  server.begin();

  // Setup LCD
  // Define characters for LCD
  lcd.createChar(0, block0Char);
  lcd.createChar(1, block1Char);
  lcd.createChar(2, block2Char);
  lcd.createChar(3, block3Char);
  lcd.createChar(4, block4Char);
  lcd.createChar(5, block5Char);
  lcd.begin(16, 2);

  // Start Servo
  servo.attach(SERVO_PIN);
  // Set servo in a defined location
  servo.write(SERVO_MIN);


  // get the time that this sketch started:
  Process startTime;
  startTime.runShellCommand("date");
  while (startTime.available()) {
    char c = startTime.read();
    if(c != '\n'){
      startString += c;
    }
  }

  // Buttons and interrupts
  pinMode(FEED_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PUMP_BUTTON_PIN, INPUT_PULLUP);
  //pinMode(PIEZO_PIN, INPUT);
  
  //attachInterrupt(digitalPinToInterrupt(PIEZO_PIN), piezoISR, RISING);
  //attachInterrupt(digitalPinToInterrupt(PUMP_BUTTON_PIN), pumpButtonISR, FALLING);
}

void loop() {
  
  // Get clients coming from server
  BridgeClient client = server.accept();

  // There is a new client?
  if (client) {
    hits++;
    // read the command
    String command = client.readString();
    command.trim();        //kill whitespace

    // Toggle Command
    if (command == "toggle") {
      pumpStatus = checkPump();
      if(pumpStatus){
        command = "off";
      }else{
        command = "on";
      }
    }
    // Turn on pump command
    if (command == "on") {
      // Turn on pump for X seconds:
      setPumpOn(pumpOnTime);
      futurePumpOn(PUMP_PERIOD);
      command = "status";
    }
    
    // Turn off pump command
    if (command == "off") {
      pumpOff = 0;
      futurePumpOn(PUMP_PERIOD);
      command = "status";
    }

    // Stop system
    if (command == "stop") {
      pumpOff = 0;
      pumpOn = 0-1;
      command = "status";
    }

    // Feed
    if (command == "feed") {
      feedCounter = FEED_PERIOD;
      feed(1,10);
      command = "status";
    }

    // Get status
    if (command == "status") {
      
      pumpStatus = checkPump();

      String buff;
      client.println("{");
      client.println(" \"start_time\":\""+startString+"\",");

      buff = String(currentTime);
      client.println(" \"current_time\":"+buff+",");

      if(pumpStatus){
        buff = String(pumpOff-currentTime);
        client.println(" \"pump\":"+buff+",");
        client.println(" \"status\":1,");
      }else{
        client.println(" \"pump\":0,");
        client.println(" \"status\":0,");
      }

      if(currentTime<pumpOn){
        buff = String(pumpOn-currentTime);
        client.println(" \"next_pump\":"+buff+",");
      }else{
        client.println(" \"next_pump\":0,");
      }

      buff = String(feedCounter);
      client.println(" \"next_feed\":"+buff+",");
      
      buff = String(hits);
      client.println(" \"hits\":"+buff+"");
      client.print("}");
    }
    
    // Close connection and free resources.
    client.stop();
  }

   
  if(!delayCounter){
    delayCounter = 20;// 20 * 50ms about 1 second
    pumpStatus = checkPump();
    if(currentTime>pumpOn){
      setPumpOn(pumpOnTime);
      futurePumpOn(PUMP_PERIOD);
      
      if(feedCounter <= 1){
        feedCounter = FEED_PERIOD;
        feed(2,10);
      }else{
        feedCounter--;
      }
    }else{
      digitalWrite(PUMP_PIN, pumpStatus);
    }
  }else{
    delayCounter--;
  }
  // Get pump on time value
  pumpOnTime = map(analogRead(A0),0, 1023, 0, MAX_PUMP_ON);

  // Print data on LCD
  printData();

  if(!digitalRead(FEED_BUTTON_PIN)){
    feedButtonISR();
  }
  if(!digitalRead(PUMP_BUTTON_PIN)){
    pumpButtonISR();
  }

  delay(50); // Poll every 50ms
}


void printPumpOnTime(void){
  const byte squares = 3;  // 3 squares
  const byte lines = 5;    // Lines per square
  const byte seconds = MAX_PUMP_ON/squares; // Each square represents X seconds
  int i;
  int tmp,tmp2,tmp3;
  tmp3 = pumpOnTime;
  for(i=0;i<squares;i++){
    tmp2 = (tmp3)/2;
    tmp = (tmp2 > lines) ? lines:int(tmp2);
    lcd.write(byte(tmp));
    tmp3 -= seconds;
    tmp3 = (tmp3 > 0) ? tmp3:0;
  }
}


void printData(void){
  // Print on LCD
  String buff;
  int next;
  boolean seconds = false;

  // Print next pump
  lcd.setCursor(0, 0);
  if(currentTime<pumpOn){
    if(pumpOn-currentTime>60){
      next = (pumpOn-currentTime)/60; //next pump in minutes
      buff = String(next);
      lcd.print("Pump: "+buff+" min ");
    }else{
      next = pumpOn-currentTime; //next pump in seconds
      buff = String(next);
      lcd.print("Pump: "+buff+" sec ");
      seconds = true;
    }
  }

  // Print the pump on time
  lcd.setCursor(13, 0);
  printPumpOnTime();

  // Print time for the next feed
  lcd.setCursor(0, 1);
  if(feedCounter>1){
    buff = String(feedCounter-1);
    lcd.print("Feed: "+buff+" hr");
    if(feedCounter != 2)
      lcd.print("s ");
    else
      lcd.print(" ");
  }else{
    if(seconds)
      lcd.print("Feed: "+buff+" sec ");
    else
      lcd.print("Feed: "+buff+" min ");
  }

  //Sprinkler
  //lcd.setCursor(13, 1);
  //if(sprinkler < 10){
  //  lcd.print(" ");
  //}
  //if(sprinkler < 100){
  //  lcd.print(" ");
  //}
  //lcd.print(sprinkler);
}

boolean checkPump(void){

  // Checks te time if the pump should be turned on
  
  Process now;
  now.runShellCommand("date +'%s'");
  String nowString = "";
  while (now.available()) {
    char c = now.read();
    if(c != '\n'){
      nowString += c;
    }
  }
  currentTime = nowString.toInt();
  
  return pumpOff>currentTime;
}  

void setPumpOn(int seconds){
  // Sets the timer for the pump to be turned on for
  //   a certian amount of seconds.
  Process then;
  then.runShellCommand("date +'%s'");
  String thenString = "";
  while (then.available()) {
    char c = then.read();
    if(c != '\n'){
      thenString += c;
    }
  }
  pumpOff = thenString.toInt()+seconds;
  digitalWrite(PUMP_PIN, checkPump());
}

void futurePumpOn(int seconds){
  // Sets the time in which the pump will be turned on again
  Process then;
  then.runShellCommand("date +'%s'");
  String thenString = "";
  while (then.available()) {
    char c = then.read();
    if(c != '\n'){
      thenString += c;
    }
  }
  pumpOn = thenString.toInt()+seconds;
}

void feed(int times, int d){
  int pos,i;
  for(i=0;i<times;i++){
    for (pos = SERVO_MIN; pos <= SERVO_MAX; pos += 1) { // goes from 0 degrees to 180 degrees
      // in steps of 1 degree
      servo.write(pos);              // tell servo to go to position in variable 'pos'
      delay(d);                       // waits 15ms for the servo to reach the position
    }
    for (pos = SERVO_MAX; pos >= SERVO_MIN; pos -= 1) { // goes from 180 degrees to 0 degrees
      servo.write(pos);              // tell servo to go to position in variable 'pos'
      delay(d);                       // waits 15ms for the servo to reach the position
    }
  }
}

void feedButtonISR(void){
  feedCounter = FEED_PERIOD;
  feed(1,10);
}

void pumpButtonISR(void){
  pumpStatus = checkPump();
  if(pumpStatus){
    pumpOff = 0;
    futurePumpOn(PUMP_PERIOD);
  }else{
    setPumpOn(pumpOnTime);
    futurePumpOn(PUMP_PERIOD);
  }
}

//void piezoISR(){
//  digitalWrite(SPRINKLER_PIN,!digitalRead(SPRINKLER_PIN));
//  sprinkler++;
//}
