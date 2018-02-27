/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example shows how to:
    Create a directory
    Move into that directory called MONDAY
    Create a sub directory within MONDAY called LOGS
    Create and write to a file inside MONDAY
*/

#include <Wire.h>

int ledPin = 13; //Status LED connected to digital pin 13

const byte OpenLogAddress = 42; //Default Qwiic OpenLog I2C address

void setup()
{
  pinMode(ledPin, OUTPUT);

  Wire.begin();
  Wire.setClock(400000); //Go super fast

  Serial.begin(9600); //9600bps is used for debug statements
  Serial.println("OpenLog Directory Test");

  //Record something to the default log
  recordToOpenLognl("Time to make a directory");

  changeDirectory(".."); //Make sure we're in the root directory

  makeDirectory("MONDAY");

  changeDirectory("MONDAY"); //Go into this new directory

  makeDirectory("LOGS"); //Create a sub directory within this new directory

  recordToOpenLognl(F("But we are still recording to the original log file in root"));

  appendFile("TheNewMe.txt");

  recordToOpenLognl(F("This is in the new file located in the MONDAY directory!"));
  
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

//Given a directory name, create it in whatever directory we are currently in
void makeDirectory(String directoryName)
{
  Wire.beginTransmission(OpenLogAddress);
  Wire.write(26); //Send the three ctrl+z escape characters
  Wire.write(26);
  Wire.write(26);
  Wire.print("md "); //Include space
  Wire.print(directoryName);
  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack");

  //Upon completion Qwiic OpenLog will respond with its status
  //Qwiic OpenLog will continue logging whatever it next receives to the current open log
}

//Given a directory name, change to that directory
void changeDirectory(String directoryName)
{
  Wire.beginTransmission(OpenLogAddress);
  Wire.write(26); //Send the three ctrl+z escape characters
  Wire.write(26);
  Wire.write(26);
  Wire.print("cd "); //Include space
  Wire.print(directoryName);
  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack");

  //Upon completion Qwiic OpenLog will respond with its status
  //Qwiic OpenLog will continue logging whatever it next receives to the current open log
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

//Write a string to Qwiic OpenLong with new line terminator
void recordToOpenLognl(String myString)
{
  recordToOpenLog(myString + "\n\r");
}

