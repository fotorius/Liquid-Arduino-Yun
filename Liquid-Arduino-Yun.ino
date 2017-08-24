/*
  Liquid: Automating watering plants and fish feeding using an Arduno Yun

  Alfredo Rius
  alfredo.rius@gmail.com

  v1.4   2017-08-21
  Removed servo enable pin
  Detach servo after feeding

  v1.3   2017-08-18
  Added servo feeder
  Working with TimerThree interrupts
  Added feeder button
  Added servo enable pin

  v1.2   2017-08-03
  Added water level sensor
  Display shows:
    - Time remining for next pump
    - Pump time
    - Water level OK/ERR
    - Temperature Sensor

  v1.1   
  Added Air Pump
  Added Temperature Sensor
  Display shows:
    - Time remining for next pump
    - Pump time
    - Temperature Sensor

  v1.0 
  Removed feeder
  Working with TimerOne interrupts

  v0.1   2017-01-15
  It Works
  System activates feeder every 12 hrs
  System activates the pump every 1 hr
  Pump time can be manually adjusted
  Display shows:
    - Time remining for next pump
    - Pump time
    - Time remining for next feed
 */

#define FEED_TIMES 4
#define PUMP_PIN 13
#define AIR_PUMP_PIN A2
#define POT_PIN A0
#define LEVEL_PIN A1
#define PUMP_BUTTON_PIN 2
#define CLEAR_DISP_BUTTON_PIN 3
#define FEEDER_BUTTON_PIN 7
#define SERVO_PIN 5
#define ONE_WIRE_BUS 12
#define MAX_PUMP_ON 180 // seconds
#define TEMP_DELAY 20 // Refresh temperature every X seconds

#define SERVO_MAX 100
#define SERVO_MIN 30

#define PUMP_PERIOD 3600 // Every hour
#define FEEDER_PERIOD (long)8 // Every 8 hours
#define AIR_PUMP_THR 55*60 // 55 min every hour

#define DEBOUNCE_FILTER 500
#define FREQ_DIV 10


#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include <LiquidCrystal.h>
#include <TimerOne.h>
#include <TimerThree.h> 
#include <DallasTemperature.h>
#include <Servo.h>

BridgeServer server;
LiquidCrystal lcd(A3, A4, A5, 8, 9, 10);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Servo servo;

// Define characters for LCD
byte block5Char[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
byte block4Char[8] = {0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE};
byte block3Char[8] = {0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC};
byte block2Char[8] = {0xF8,0xF8,0xF8,0xF8,0xF8,0xF8,0xF8,0xF8};
byte block1Char[8] = {0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0};
byte block0Char[8] = {0xE0,0xE0,0xE0,0xE0,0xE0,0xE0,0xE0,0xE0};


String startString;
long hits = 0;
unsigned long nextPump = PUMP_PERIOD;
unsigned long pumpOff = 0;
unsigned long pumpOnTime = 0;
boolean pumpState;
boolean firstPumpRun = true;
boolean firstFeedRun = true;
boolean interrupted = false;
boolean feedState = false;
unsigned long feedCounter = FEEDER_PERIOD;
unsigned int readTemp = TEMP_DELAY;
unsigned long debounceFilter = 0;
unsigned long freqDiv = FREQ_DIV;
unsigned long lastProximity = 0;
unsigned long proximity = 0;
float temp;


void resetLCD(void){
  // Define characters for LCD
  lcd.createChar(0, block0Char);
  lcd.createChar(1, block1Char);
  lcd.createChar(2, block2Char);
  lcd.createChar(3, block3Char);
  lcd.createChar(4, block4Char);
  lcd.createChar(5, block5Char);
  lcd.begin(16, 2);
}

void printData(void){
  // Print on LCD
  String buff;
  boolean seconds = false;

  // Print next pump
  lcd.setCursor(0, 0);
  if(pumpState && pumpOff>60){
    buff = String(pumpOff/60);//next pump in minutes
    lcd.print("Pump: "+buff+" min ");
  }else if(pumpState){
    buff = String(pumpOff);
    lcd.print("Pump: "+buff+" sec ");
  }else if(nextPump>60){
    buff = String(nextPump/60);//next pump in minutes
    lcd.print("Pump: "+buff+" min ");
  }else{
    buff = String(nextPump);
    lcd.print("Pump: "+buff+" sec ");
  }
  
  lcd.setCursor(0, 1);
  //lcd.print("Level:    ");
  //lcd.setCursor(6, 1);
  //lcd.print(analogRead(LEVEL_PIN));
  if(!digitalRead(LEVEL_PIN)){
    lcd.print("Level: OK       ");
  }else{
    lcd.print("Level: ERR      ");
  }

  lcd.setCursor(11, 1);
  lcd.print(temp);
  
}

float getTemp(void){
  sensors.requestTemperatures(); // Send the command to get temperatures
  return sensors.getTempCByIndex(0);
}

void printPumpOnTime(void){
  const byte squares = 3;  // 3 squares
  const byte lines = 5;    // Lines per square
  const int seconds = MAX_PUMP_ON/squares; // Each square represents X seconds
  int i;
  int tmp,tmp2,tmp3;
  lcd.setCursor(13, 0);
  tmp3 = pumpOnTime;
  for(i=0;i<squares;i++){
    tmp2 = (tmp3)/2;
    tmp = (tmp2 > lines) ? lines:int(tmp2);
    lcd.write(byte(tmp));
    tmp3 -= seconds;
    tmp3 = (tmp3 > 0) ? tmp3:0;
  }
}


void togglePump(void){
  if(pumpState){
    // Deactivate Pump
    nextPump = PUMP_PERIOD - pumpOnTime;
    pumpOff = pumpOnTime;
    pumpState = false;
    digitalWrite(PUMP_PIN,LOW);
  }else{
    // Activate Pump
    nextPump = 0;
    pumpOff = pumpOnTime;
    pumpState = true;
    digitalWrite(PUMP_PIN,!digitalRead(LEVEL_PIN));
  }
}

void pumpButtonISR(void){
  if(debounceFilter<millis() && !firstPumpRun){
    togglePump();
    debounceFilter = millis()+DEBOUNCE_FILTER;
  }
  firstPumpRun = false;
}

void feederButtonISR(void){
  if(debounceFilter<millis() && !firstPumpRun){
    feedState = true;
    debounceFilter = millis()+DEBOUNCE_FILTER;
  }
  firstPumpRun = false;
}

void clearDispButtonISR(void){
  if(debounceFilter<millis() && !firstPumpRun){
    resetLCD();
    debounceFilter = millis()+DEBOUNCE_FILTER;
  }
  firstPumpRun = false;
}



void feed(int times, int d){
  int pos,i;
  servo.attach(SERVO_PIN);
  servo.write(SERVO_MIN);
  delay(100);
  // Reset everything
  feedState = false;
  feedCounter = FEEDER_PERIOD;
  
  // Feed fish
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
  delay(1000);
  
  servo.detach();
  digitalWrite(SERVO_PIN, LOW);
}


void timerCount(void){ // Every 100 ms
  if(!freqDiv){ // Every Second
    if(!nextPump){
      if(pumpOff && pumpState){
        pumpOff -= 1;
      }else if(!pumpOff && !pumpState){
        pumpOff = pumpOnTime;
        pumpState = true;
      }else{
        pumpState = false;
        nextPump = PUMP_PERIOD - pumpOnTime;

        // FEEDER
        if(!feedCounter){
          feedCounter = FEEDER_PERIOD;
          feedState = true;
        }else{
          feedCounter--;
        }

        
      }
      digitalWrite(PUMP_PIN,!digitalRead(LEVEL_PIN));
    }else{
      nextPump -= 1;
      digitalWrite(PUMP_PIN,LOW);
    }


    
    if(!interrupted){
      interrupted = true;
      printData();
      readTemp--;
      interrupted = false;
    }

    digitalWrite(AIR_PUMP_PIN,nextPump<AIR_PUMP_THR);
    
    freqDiv = FREQ_DIV;
  }
  
  pumpOnTime = map(analogRead(POT_PIN),0, 1023, 0, MAX_PUMP_ON);
  
  if(!interrupted){
    interrupted = true;
    printPumpOnTime();
    interrupted = false;
  }
  
  freqDiv--;
}

void setup(void){

  //Pin Setup
  pinMode(POT_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(AIR_PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pinMode(PUMP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FEEDER_BUTTON_PIN, INPUT);
  pinMode(CLEAR_DISP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LEVEL_PIN, INPUT_PULLUP);

  // Servo
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN,LOW);
  delay(100);

  // LCD Setup
  resetLCD();

  // Screen says hello!
  lcd.setCursor(0, 0);
  lcd.print("Liquid      v1.4");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  delay(3000);
  lcd.setCursor(0, 0);
  lcd.print("                ");

  
  // Interrupt Setup
  attachInterrupt(digitalPinToInterrupt(PUMP_BUTTON_PIN), pumpButtonISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(CLEAR_DISP_BUTTON_PIN), clearDispButtonISR, FALLING);
  

  Timer3.initialize(100000); // 100 ms
  Timer3.attachInterrupt(timerCount);

  // Bridge
  Bridge.begin();
  // Start Server
  server.listenOnLocalhost();
  server.begin();
}

void loop(void){

  delay(50);

  if(!interrupted && !readTemp){
    interrupted = true;
    temp = getTemp();
    readTemp = TEMP_DELAY;
    interrupted = false;
  }
  
  // Get clients coming from server
  BridgeClient client = server.accept();
  
  // There is a new client?
  if (client) {
    // read the command
    String command = client.readString();
    command.trim();        //kill whitespace

    // Toggle Command
    if (command == "toggle") {
      if(pumpState){
        command = "off";
      }else{
        command = "on";
      }
    }
    // Turn on pump command
    if (command == "on") {
      pumpState = false;
      togglePump();
      command = "status";
    }
    
    // Turn off pump command
    if (command == "off") {
      pumpState = true;
      togglePump();
      command = "status";
    }

    // Stop system
    if (command == "stop") {
      command = "status";
    }

    // Feed
    if (command == "feed") {
      command = "status";
    }

    // Get status
    if (command == "status") {
      String buff;
      client.println("{");
      if(pumpState){
        buff = String(pumpOff);
        client.println(" \"pump\":"+buff+",");
        client.println(" \"status\":1,");
        client.println(" \"next_pump\":0,");
      }else{
        client.println(" \"pump\":0,");
        client.println(" \"status\":0,");
        buff = String(nextPump);
        client.println(" \"next_pump\":"+buff+",");
      }
      buff = String(!digitalRead(LEVEL_PIN));
      client.println(" \"level\":"+buff+",");
      buff = String(feedCounter);
      client.println(" \"feed_counter\":"+buff+",");
      buff = String(digitalRead(AIR_PUMP_PIN));
      client.println(" \"air_pump\":"+buff+",");
      buff = String(temp);
      client.println(" \"temperature\":"+buff);
      client.print("}");
    }
    
    // Close connection and free resources.
    client.stop();
  }
  if(feedState){
    feed(FEED_TIMES,10);
  }
  if(digitalRead(FEEDER_BUTTON_PIN)){
    feed(1,10);
  }
}









