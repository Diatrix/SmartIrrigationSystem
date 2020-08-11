
/*
  Automatic irrigation system and data logger. 

  This is the code for an automatic irrigation system that turns on an electrovalve when soil moisture gets too low, and only at certain 
  times of the day (Evenings and nights ideally). Also has a datalogger to log humidity data, and temperature to a microSD card. 
  

  The circuit:
   Microcontroller: STM32F103 (blue pill)
   
   SD card reader attached to SPI bus as follows:
 ** MOSI - pin PA7
 ** MISO - pin PA6
 ** CLK - pin PA5
 ** CS - pin PA4

   i2c RTC: DS3231. Attached to microcontroller as follows:
 ** Data: PB7
 ** Clock: PB6
 *  Make sure to put pull up resistors on both lines. 
 * 
   Capacitative soil moisture sensor v1.2 connected as follows:
 ** Analog out: PA2

   Relay Module for electrovalve connected as follows:
 ** Pin out 1: PA15
 ** Pin Out 2: PB3
  * Make sure to put pull up resistor to this line. 

 
  created  9 March 2020

  by Diego Llamas

  This example code is in the public domain.

*/
const int  moistureValue  = 20; //Threshold value where if the moisture is below this, the valve will open (given the time of day is between irrigationTimeStart and irrigationTimeStop.)

const int sleepTime = 1800000; //30 min.
const int irrigationTimeStart = 19;
const int irrigationTimeStop = 7;

const int valveOnTimer = 30000;


#include "STM32LowPower.h"

//SD card setup
#include <SPI.h>
#include <SD.h>
//SPI pinouts. 
const int MOSIpin = PA7;
const int MISOpin = PA6;
const int SCLKpin = PA5;
const int chipSelect = PA4;


//RTC
#include <Wire.h>
#include "RTClib.h"
//#define Wire2 Wire
//TwoWire Wire2 = TwoWire(SDA, SCL) //this is in case you want to use other pins for the i2c
RTC_DS3231 rtc;

const char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//Capacitative Moisture Sensor input
const int moistureSensorPin = PA2;

//Relay signal output pin: 
const int relayOutputPin1 = PA15;
const int relayOutputPin2 = PB3;
//loop counter
int count = 0;

void setup() {
  
 // Sets up the pin output for the relay module. Since it's active low (when the output is low, the circuit activates), I initialize it with a HIGH value. 
  pinMode(relayOutputPin1, OUTPUT);  
  digitalWrite(relayOutputPin1, LOW); 

  pinMode(relayOutputPin2, OUTPUT);  
  digitalWrite(relayOutputPin2, LOW); 

// Low power config
  LowPower.begin();
  
  // Sets the SPI pins for the SD card communication
  pinMode(chipSelect, OUTPUT);
  SPI.setMOSI(MOSIpin);
  SPI.setMISO(MISOpin);
  SPI.setSCLK(SCLKpin);
  SPI.setSSEL(chipSelect);


  Serial.begin(38400);

// Waits 4 seconds just to make sure everything is initialized, and so I can open the serial port in time. 
  delay(1000);

// Initializes SD card. 
  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
  }
  else {
  Serial.println("card initialized.");
  }
  
//initlaizes log file
  File logFile = SD.open("log.csv", FILE_WRITE);
//  Initializes RTC. If it's not found, log will be uploaded to the SD card and system will shut down.
  Wire.begin();
 
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");

    if(logFile){
      logFile.println("Could not find RTC. Shutting down.");
    }
 
    else{
      Serial.println("Could not open log file");
    }

    delay(500);
    while(1){
      LowPower.shutdown(3000000);
    }
  }

  
// Sets LED for error communication. 
  pinMode(LED_BUILTIN, OUTPUT);
// Sets up the pin for the moisture sensor. 
  pinMode(moistureSensorPin, INPUT); 

  logFile.println(" Setup complete! " + get_time_stamp(rtc.now()));
  logFile.close();
  
}

void loop() {
  //initializes and checks if SD card is available. 
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
  }

 
  
  digitalWrite(LED_BUILTIN, LOW);
  String fileLine = "";
  DateTime now = rtc.now();
  String timeStamp = get_time_stamp(now); //creates a timestamp stringfollowing the ISO 8601 format. 

  
  //Shuts down if there's an error with the RTC. 
  if (now.year() < 2010 || now.year() > 2100){
    Serial.println("There was an error with the RTC. Reset device to retry. Shutting down...");
    File logFile = SD.open("log.csv", FILE_WRITE);  
    logFile.println("There was an error with the RTC. Reset device to retry. Shutting down...");
    digitalWrite(LED_BUILTIN, HIGH);
    logFile.close();
    delay(1000);
    while(1){
      LowPower.shutdown(3000000);
    }
  }
  
  Serial.println(timeStamp + " Woke up");

// takes moisture reading
  int sensorData = analogRead(moistureSensorPin);
  int moisture = map(sensorData, 1024, 467, 0, 100);
  
// take temperature reading
  int temp = rtc.getTemperature();
  fileLine = timeStamp + ", " + String(moisture) + "(" + String(sensorData) + ")" + ", " + String(temp);
  
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.csv", FILE_WRITE);
  
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(fileLine);
    Serial.println(fileLine);
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.csv");
        Serial.println(fileLine);

  }

  
  
  //checks moisture and time of day.
  if (moisture < moistureValue && (now.hour() >= irrigationTimeStart || now.hour() <= irrigationTimeStop)) {
    File logFile = SD.open("log.csv", FILE_WRITE);
    
    //writes log.
    if(logFile){
      logFile.println(timeStamp + " Valve on");
    }
    Serial.println(timeStamp + " Valve on");
    digitalWrite(relayOutputPin1, HIGH); 
    digitalWrite(relayOutputPin2, HIGH); 

    delay(valveOnTimer);
    digitalWrite(relayOutputPin1, LOW); 
    digitalWrite(relayOutputPin2, LOW); 

    Serial.println(get_time_stamp(rtc.now()) + " Valve off");
    //writes log
    if(logFile){
      logFile.println(get_time_stamp(rtc.now()) + " Valve off");
      logFile.close();
    }  
  }
  Serial.println("going to sleep");
  Serial.println();
  delay(500);
  LowPower.deepSleep(sleepTime);
  delay(500);
}
//Takes a DateTime object and outputs a string timestamp with ISO 8601 format (without timezone).
String get_time_stamp(DateTime time){
  //creates the timestamp string properly formatted (with leading zeroes)
    char tempString [18]= "";
    sprintf(tempString, "%04d-%02d-%02dT%02d:%02d:%02d", time.year(), time.month(), time.day(), time.hour(), time.minute(), time.second());
    return tempString;
}
