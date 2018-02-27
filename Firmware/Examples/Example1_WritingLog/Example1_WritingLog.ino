/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example shows how to record various text and variables to Qwiic OpenLog

  To Use:
    Insert a formatted SD card into Qwiic OpenLog
    Attach Qwiic OpenLog to a RedBoard or Uno with a Qwiic cable
    Load this sketch onto the RedBoard
    Open a terminal window to see the Serial.print statements
    Then insert the SD card into a computer view the log file contents
*/

#include <Wire.h>

int ledPin = 13; //Status LED connected to digital pin 13

const byte OpenLogAddress = 42; //Default Qwiic OpenLog I2C address

void setup()
{
  pinMode(ledPin, OUTPUT);

  Wire.begin();

  Serial.begin(9600); //9600bps is used for debug statements
  Serial.println("OpenLog Read File Test");
  
  //Record something to the default log
  recordToOpenLog("This goes to the log file\n\r");
  Serial.println("This goes to the terminal");

  float batteryVoltage = 3.4;
  recordToOpenLog("Batt voltage: " + String(batteryVoltage) + "\n\r");

  batteryVoltage = batteryVoltage + 0.71;

  recordToOpenLog("Batt voltage: " + String(batteryVoltage) + "\n\r");

  Serial.println(F("Done!"));
}

void loop()
{
  //Blink the Status LED because we're done!
  digitalWrite(ledPin, HIGH);
  delay(100);
  digitalWrite(ledPin, LOW);
  delay(1000);
}

//Write a string to Qwiic OpenLong
//Arduino has a limit of 32 bytes per write
//This splits writes up into 32 byte chunks
void recordToOpenLog(String myString)
{
  while (myString.length() > 0)
  {
    //Pick the smaller of 32 or the length of the string to send
    byte toSend = 32;
    if (myString.length() < 32) toSend = myString.length();

    Wire.beginTransmission(OpenLogAddress);
    Wire.print(myString.substring(0, toSend));
    if (Wire.endTransmission() != 0)
      Serial.println("Error: Sensor did not ack");

    //Remove what we just sent from the big string
    myString = myString.substring(toSend, myString.length());
  }
}
