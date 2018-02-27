/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This is a simple test sketch for Qwiic OpenLog. It sends a large strings of text quickly over I2C.
  We can move 1,100,000 bytes in 53 seconds or 20,754 bytes per second. This is about half the max speed
  possible at 400kHz. Clock stretching allows Qwiic OpenLog to tell master to wait while it completes a write.

  At 115200bps with OpenLog Serial, it logged 11,520 bytes per second with a few buffer overruns
  At 100kHz I2C that's 10,000 bytes per second (10-bits per byte in I2C). Should be ok.
  At 400kHz, that's 40,000 bytes per second. We're going to need clock stretching.

  To use this sketch, attach Qwiic OpenLog to an Arduino.
  After power up, OpenLog will start flashing (indicating it's receiving characters). It takes about 1 minute for
  the sketch to run to completion. This will create a file that looks like this:

  ...
  6:abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-!#
  7:abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-!#
  8:abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-!#
  9:abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-!#
  #:abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-!#
  1:abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-!#
  2:abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-!#
  ...

  The reason for creating these character blocks is it allows for a reader to very quickly scan the visible characters and
  indentify any byte errors or glitches along the way. Every 9 lines we print a 10th line that has a leading character
  such as # or !. These allow us to quickly see the different blocks of 10 lines.

*/

#include <Wire.h>

int ledPin = 13; //Status LED connected to digital pin 13

const byte OpenLogAddress = 42; //Default Qwiic OpenLog I2C address

void setup()
{
  pinMode(ledPin, OUTPUT);

  Serial.begin(9600); //9600bps is used for debug statements

  Wire.begin();
  Wire.setClock(400000); //Set I2C bus speed to fast 400kHz

  Serial.println();
  Serial.println("Run OpenLog Test");
  recordToOpenLog("Run OpenLog Test\n");

  int testAmt = 10;
  //At 100kHz, testAmt of 10 takes about 13 seconds
  //At 400kHz, testAmt of 10 takes about 5 seconds
  //testAmt of 10 will push 110,000 characters/bytes.

  //Each test is 100 lines. 10 tests is 1000 lines (11,000 characters)
  for (int numofTests = 0 ; numofTests < testAmt ; numofTests++)
  {
    //This loop will print 100 lines of 110 characters each
    for (int k = 33; k < 43 ; k++)
    {
      //Print one line of 110 characters with marker in the front (markers go from '!' to '*')
      recordToOpenLog(String((char)k)); //Print the ASCII value directly: ! then " then #, etc
      recordToOpenLog(":abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-!#\n");
      //delay(50);

      //Then print 9 lines of 110 characters with new line at the end of the line
      for (int i = 1 ; i < 10 ; i++)
      {
        recordToOpenLog(String(i, DEC));
        recordToOpenLog(":abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-!#\n");
        //delay(50);
      }

      if (digitalRead(ledPin) == 0) //Turn the status LED on/off as we go
        digitalWrite(ledPin, HIGH);
      else
        digitalWrite(ledPin, LOW);
    }
  } //End numofTests loop

  recordToOpenLog("Done!\n");

  unsigned long totalCharacters = (long)testAmt * 100 * 110;
  Serial.print("Characters pushed: ");
  Serial.println(totalCharacters);
  Serial.print("Time taken (s): ");
  Serial.println(millis() / 1000);
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


