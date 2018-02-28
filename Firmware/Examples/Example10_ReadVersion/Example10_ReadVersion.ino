/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example shows how to read the firmware version of Qwiic OpenLog.
  
  This is helpful when getting support. We will be adding features as we go and having the firmware
  version will let us know what features your board supports.
*/

#include <Wire.h>

int ledPin = 13; //Status LED connected to digital pin 13

const byte OpenLogAddress = 42; //Default Qwiic OpenLog I2C address

void setup()
{
  pinMode(ledPin, OUTPUT);

  Wire.begin();

  Serial.begin(9600); //9600bps is used for debug statements
  Serial.println("OpenLog Version Reader");

  printOpenLogVersion();
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

//Request the firmware version
long printOpenLogVersion()
{
  Wire.beginTransmission(OpenLogAddress);
  Wire.write(26); //Send the three ctrl+z escape characters
  Wire.write(26);
  Wire.write(26);
  Wire.print("ver"); //Include space
  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack");

  //Upon completion Qwiic OpenLog will have 2 bytes ready to be read
  Wire.requestFrom(OpenLogAddress, 2);

  byte versionMajor = Wire.read();
  byte versionMinor = Wire.read();

  Serial.print("Qwiic OpenLog Version ");
  Serial.print(versionMajor, DEC);
  Serial.print(".");
  Serial.println(versionMinor, DEC);
}

