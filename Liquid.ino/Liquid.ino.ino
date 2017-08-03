/*
  Liquid: Automating watering plants using an Arduno Yun v1.1

  2017/08/03
  Alfredo Rius
  alfredo.rius@gmail.com
 */


#define PUMP_PIN 13
#define SERVO_PIN 11
#define POT_PIN A0
#define LEVEL_PIN A1
#define PUMP_BUTTON_PIN 2
#define MAX_PUMP_ON 45 // seconds
#define LEVEL_ERROR 800

#define PUMP_PERIOD 3600

#define DEBOUNCE_FILTER 500
#define FREQ_DIV 10


#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include <LiquidCrystal.h>
#include <TimerOne.h>

BridgeServer server;
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
unsigned long nextPump = PUMP_PERIOD;
unsigned long pumpOff = 0;
unsigned long pumpOnTime = 0;
boolean pumpState;
boolean firstPumpRun = true;
boolean firstFeedRun = true;
unsigned long debounceFilter = 0;
unsigned long freqDiv = FREQ_DIV;
unsigned long lastProximity = 0;
unsigned long proximity = 0;

void printData(void){
  // Print on LCD
  String buff;
  boolean seconds = false;

  // Print next pump
  lcd.setCursor(0, 0);
  if(pumpState){
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
  if(analogRead(LEVEL_PIN)<LEVEL_ERROR){
    lcd.print("Level: OK   ");
  }else{
    lcd.print("Level: ERROR");
  }
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
    digitalWrite(PUMP_PIN,analogRead(LEVEL_PIN)<LEVEL_ERROR);
  }
}

void pumpButtonISR(void){
  if(debounceFilter<millis() && !firstPumpRun){
    togglePump();
    debounceFilter = millis()+DEBOUNCE_FILTER;
  }
  firstPumpRun = false;
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
      }
      digitalWrite(PUMP_PIN,analogRead(LEVEL_PIN)<LEVEL_ERROR);
    }else{
      nextPump -= 1;
      digitalWrite(PUMP_PIN,LOW);
    }
    printData();
    freqDiv = FREQ_DIV;
  }
  
  lcd.setCursor(13, 0);
  pumpOnTime = map(analogRead(POT_PIN),0, 1023, 0, MAX_PUMP_ON);
  printPumpOnTime();
  
  freqDiv--;
}

void setup(void){

  //Pin Setup
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pinMode(PUMP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LEVEL_PIN, INPUT_PULLUP);

  // LCD Setup
  // Define characters for LCD
  lcd.createChar(0, block0Char);
  lcd.createChar(1, block1Char);
  lcd.createChar(2, block2Char);
  lcd.createChar(3, block3Char);
  lcd.createChar(4, block4Char);
  lcd.createChar(5, block5Char);
  lcd.begin(16, 2);

  // Screen says hello!
  lcd.setCursor(0, 0);
  lcd.print("Liquid      v1.1");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  delay(3000);
  lcd.setCursor(0, 0);
  lcd.print("                ");

  
  // Interrupt Setup
  attachInterrupt(digitalPinToInterrupt(PUMP_BUTTON_PIN), pumpButtonISR, FALLING);

  Timer1.initialize(100000); // 100 ms
  Timer1.attachInterrupt(timerCount);

  // Bridge
  Bridge.begin();
  // Start Server
  server.listenOnLocalhost();
  server.begin();
}

void loop(void){

  delay(50);
  
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
      buff = String(analogRead(LEVEL_PIN)<LEVEL_ERROR);
      client.println(" \"level\":"+buff+"");
      client.print("}");
    }
    
    // Close connection and free resources.
    client.stop();
  }
}

