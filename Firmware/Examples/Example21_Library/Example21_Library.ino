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
  Available:

*/

#include <Wire.h>

#include "SparkFun_Qwiic_OpenLog_Arduino_Library.h"
QOL OpenLog;

void setup()
{
  Serial.begin(9600);
  Serial.println("Qwiic OpenLog Read Example");

  Wire.begin();
  Wire.setClock(400000);

  OpenLog.begin(0x2A); //Default I2C address for Qwiic OpenLog.
  //OpenLog.begin(); //Begin with default I2C address for Qwiic OpenLog
  OpenLog.println("More testing");

  byte me = 10;
  OpenLog.print(me);
  OpenLog.println(" Yep, works. but now we need to test some really big strings to make sure those get parsed correctly...");


  Serial.println("Done!");
}

void loop()
{
}
