/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example shows how to change the I2C address of the Qwiic OpenLog

  Note: It's easy to change the I2C address. If you forget what the address is you can use the I2CScan Example to re-discover it
  You can also close the ADR jumper on the board. This will force the I2C address to 0x29 regardless of any other setting or command.

  Valid I2C addresses are 0x08 to 0x77 (inclusive): https://www.totalphase.com/support/articles/200349176-7-bit-8-bit-and-10-bit-I2C-Slave-Addressing
*/

#include <Wire.h>

int ledPin = 13; //Status LED connected to digital pin 13

byte OpenLogAddress = 42; //Address of the Qwiic OpenLog

#define STATUS_SD_INIT_GOOD 0
#define STATUS_LAST_COMMAND_SUCCESS 1
#define STATUS_LAST_COMMAND_KNOWN 2
#define STATUS_FILE_OPEN 3
#define STATUS_IN_ROOT_DIRECTORY 4

void setup()
{
  pinMode(ledPin, OUTPUT);

  Wire.begin();
  Wire.setClock(400000); //Go super fast

  Serial.begin(9600); //9600bps is used for debug statements
  Serial.println("OpenLog Address Change Example");
  Serial.println(F("Press a key to begin"));
  while (Serial.available()) Serial.read(); //Clear buffer
  while (Serial.available() == 0) delay(10); //Wait for user to press a key

  byte status = getStatus(); //Check the status byte to see if we're good to go

  Serial.print("Status byte: 0x");
  if (status < 0x10) Serial.print("0");
  Serial.println(status, HEX);

  if (status == 0xFF)
  {
    Serial.println("OpenLog failed to respond. Check wiring. Freezing.");
    while (1);
  }

  if (status & 1 << STATUS_SD_INIT_GOOD)
    Serial.println("SD card online");

  status = getStatus(); //Check the status byte to see if we're good to go
  Serial.print("Status byte: 0x");
  if (status < 0x10) Serial.print("0");
  Serial.println(status, HEX);

  setAddress(30); //Set the I2C address
  OpenLogAddress = 30;

  status = getStatus(); //Check the status byte, using new address, to see if command worked (and if we can talk to OpenLog in general)

  Serial.print("Status byte: 0x");
  if (status < 0x10) Serial.print("0");
  Serial.println(status, HEX);

  if (status & 1 << STATUS_LAST_COMMAND_SUCCESS)
    Serial.println("Last command was successful");
  else
    Serial.println("Last command failed.");

  recordToOpenLog("This is recorded to the last open log file but using the new I2C address");

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

//Request the current status
long getStatus()
{
  //During normal operation OpenLog will respond with its status byte
  Wire.requestFrom(OpenLogAddress, 1);

  return (Wire.read());
}

//Set the I2C Address
void setAddress(byte newAddress)
{
  Wire.beginTransmission(OpenLogAddress);
  Wire.write(26); //Send the three ctrl+z escape characters
  Wire.write(26);
  Wire.write(26);
  Wire.print("adr "); //Include space
  Wire.print(newAddress, DEC); //Print the value
  
  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack");

  //Upon completion any new communication must be with this new I2C address
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
