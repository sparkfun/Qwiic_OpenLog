/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example shows how to read the status byte of the OpenLog
  
  Qwiic OpenLog records any incoming byte to the default log file. If quered by the master it will
  normally respond with a status byte with the following structure:

  Bit 0: SD/Init Good
  Bit 1: Last Command Succeeded
  Bit 2: Last Command Known
  Bit 3: File Currently Open
  Bit 4: In Root Directory
  Bit 5: 0 - Future Use
  Bit 6: 0 - Future Use
  Bit 7: 0 - Future Use
  
  Instances where the status byte is not sent is when an active command is running such as read file size,
  read file contents, etc. If you have been running lots of other examples then please unplug/plug OpenLog so as
  to power cycle it and return it to its default logging state.  
*/

#include <Wire.h>

int ledPin = 13; //Status LED connected to digital pin 13

const byte OpenLogAddress = 42; //Default Qwiic OpenLog I2C address

#define STATUS_SD_INIT_GOOD 0
#define STATUS_LAST_COMMAND_SUCCESS 1
#define STATUS_LAST_COMMAND_KNOWN 2
#define STATUS_FILE_OPEN 3
#define STATUS_IN_ROOT_DIRECTORY 4

void setup()
{
  pinMode(ledPin, OUTPUT);

  Wire.begin();

  Serial.begin(9600); //9600bps is used for debug statements
  Serial.println("OpenLog Status Example");

  byte status = getStatus();

  Serial.print("Status byte: 0x");
  if(status < 0x10) Serial.print("0");
  Serial.println(status, HEX);

  if(status == 0xFF)
  {
    Serial.println("OpenLog failed to respond. Freezing.");
    while(1); //Freeze!
  }

  if(status & 1<<STATUS_SD_INIT_GOOD)
    Serial.println("SD card is good");
  else
    Serial.println("SD init failure. Is the SD card present? Formatted?");

  if(status & 1<<STATUS_IN_ROOT_DIRECTORY)
    Serial.println("Root directory open");
  else
    Serial.println("Root failed to open. Is SD card present? Formatted?");

  if(status & 1<<STATUS_FILE_OPEN)
    Serial.println("Log file open and ready for recording");
  else
    Serial.println("No log file open. Use append command to start a new log.");

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

  return(Wire.read());
}

