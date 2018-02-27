/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example shows how to:
    Record some strings to a default log
    Check the size of a given file name
    If the given file doesn't exist, say so
*/

#include <Wire.h>

int ledPin = 13; //Status LED connected to digital pin 13

const byte OpenLogAddress = 42; //Default Qwiic OpenLog I2C address

void setup()
{
  pinMode(ledPin, OUTPUT);

  Wire.begin();
  Wire.setClock(400000); //Go super fast 400kHz I2C

  Serial.begin(9600); //9600bps is used for debug statements
  Serial.println();
  Serial.println("OpenLog Read File Test");
  
  //Record something to the default log
  recordToOpenLognl("OpenLog Read File Test");

  String myFile = "testFile.txt";

  //Get size of file
  long sizeOfFile = readFileSize(myFile);

  if (sizeOfFile == -1)
  {
    Serial.println(F("File not found."));
  }
  else
  {
    Serial.println(F("File found!"));
    Serial.print(F("Size of file: "));
    Serial.println(sizeOfFile);
  }

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

//Given a file name, read the size of the file
long readFileSize(String fileName)
{
  Wire.beginTransmission(OpenLogAddress);
  Wire.write(26); //Send the three ctrl+z escape characters
  Wire.write(26);
  Wire.write(26);
  Wire.print("size "); //Include space
  Wire.print(fileName);
  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack");

  //Upon completion Qwiic OpenLog will have 4 bytes ready to be read
  Wire.requestFrom(OpenLogAddress, 4);

  long fileSize = 0;
  while (Wire.available())
  {
    byte incoming = Wire.read();
    fileSize <<= 8;
    fileSize |= incoming;
  }

  return (fileSize);
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

