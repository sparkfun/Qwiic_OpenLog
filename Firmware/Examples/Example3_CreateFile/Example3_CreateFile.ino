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
*/

#include <Wire.h>

int ledPin = 13; //Status LED connected to digital pin 13

const byte OpenLogAddress = 42; //Default Qwiic OpenLog I2C address

void setup()
{
  pinMode(ledPin, OUTPUT);

  Wire.begin();

  Serial.begin(9600); //9600bps is used for debug statements
  Serial.println("Run OpenLog New File Test"); //Goes to terminal
  recordToOpenLog("Run OpenLog New File Test\r\n"); //Goes to the default LOG00312.txt file

  recordToOpenLog("This is recorded to the default log file\r\n");
  newFile("NewFile.txt");
  recordToOpenLog("This is also recorded to the default log file. But a new file has been created\r\n");
  recordToOpenLog("If you want to write to a file use appendFile in example 2\r\n");

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
    if (myString.length() < 32) toSend = myString.length();

    Wire.beginTransmission(OpenLogAddress);
    Wire.print(myString.substring(0, toSend));
    if (Wire.endTransmission() != 0)
      Serial.println("Error: Sensor did not ack");

    //Remove what we just sent from the big string
    myString = myString.substring(toSend, myString.length());
  }
}

//Given a file name, create it
void newFile(String fileName)
{
  Wire.beginTransmission(OpenLogAddress);
  Wire.write(26); //Send the three ctrl+z escape characters
  Wire.write(26);
  Wire.write(26);
  Wire.print("new "); //Include space
  Wire.print(fileName);
  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack");

  //Upon completion a new file is created but OpenLog is still recording to original file
}

