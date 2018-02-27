/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example shows how to create new files and write to them

  To Use:
    Insert a formatted SD card into Qwiic OpenLog
    Attach Qwiic OpenLog to a RedBoard or Uno with a Qwiic cable
    Load this sketch onto the RedBoard
    Open a terminal window to see the Serial.print statements
    Then insert the SD card into a computer view the log file contents

  Note: We can use the F("") in sketches to move big print statements into program memory to save RAM.
  See more at: https://forum.arduino.cc/index.php?topic=428015.0
*/

#include <Wire.h>

int ledPin = 13; //Status LED connected to digital pin 13

const byte OpenLogAddress = 42; //Default Qwiic OpenLog I2C address

void setup()
{
  pinMode(ledPin, OUTPUT);

  Wire.begin();

  Serial.begin(9600); //9600bps is used for debug statements
  Serial.println("Run OpenLog Append File Test");
  recordToOpenLog("Run OpenLog Append File Test\n");

  recordToOpenLog("This is recorded to the default log file\n");
  appendFile("appendMe.txt");
  recordToOpenLog("This is recorded to appendMe.txt\n");
  recordToOpenLog("If this file doesn't exist it is created and\n");
  recordToOpenLog("anything sent to OpenLog will be recorded to this file\n");

  recordToOpenLog(F("\nNote: We can use the F(\"\") in sketches to move big print statements into program memory to save RAM\n"));

  Serial.println("Done!");
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
//Arduino has limit of 32 bytes per write
//This splits writes up into 32 byte chunks
void recordToOpenLog(String myString)
{
  while (myString.length() > 0)
  {
    //Pick the smaller of 32 or the length of the string to send
    byte toSend = 32;
    if (myString.length() < toSend) toSend = myString.length();

    Wire.beginTransmission(OpenLogAddress);
    Wire.print(myString.substring(0, toSend));
    if (Wire.endTransmission() != 0)
      Serial.println("Error: Sensor did not ack");

    //Remove what we just sent from the big string
    myString = myString.substring(toSend, myString.length());
  }
}

//Given a file name, start writing to the end of it
//Append will also create a new file if one does not already exist
void appendFile(String fileName)
{
  Wire.beginTransmission(OpenLogAddress);
  Wire.write(26); //Send the three ctrl+z escape characters
  Wire.write(26);
  Wire.write(26);
  Wire.print("append "); //Include space
  Wire.print(fileName);
  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack");

  //Upon completion any new characters sent to OpenLog will be recorded to this file
}

