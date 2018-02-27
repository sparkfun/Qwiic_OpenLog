/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example shows how to:
    Create a directory
    Create some files there
    Delete a specific file
    Delete *.TXT
    Remove the directory we created
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
  Serial.println("OpenLog Removing a Directory Test");

  Serial.println(F("Press a key to begin"));

  while(Serial.available()) Serial.read(); //Clear buffer
  while(Serial.available() == 0) delay(10); //Wait for user to press a key

  Serial.println(F("Making a directory and empty files"));
  recordToOpenLognl(F("Making a directory and empty files")); //Record something to the default log

  //Create some directories and files
  changeDirectory(".."); //Return to root
  makeDirectory("MONDAY");
  changeDirectory("MONDAY"); //Move into this sub directory
  appendFile("TheNewMe.txt");
  appendFile("Test1.txt");
  appendFile("Test2.txt");
  appendFile("Test3.txt");

  recordToOpenLognl(F("This is recorded to the last appended file in MONDAY"));

  if(removeFile("Test1.txt") == 1) //Delete a specific file
    Serial.println(F("We have deleted Test1.txt! You can remove the SD card to see."));
  else
    Serial.println(F("Failed to delete Test1.txt"));

  Serial.println(F("Press a key to delete all the files in the MONDAY directory."));

  while(Serial.available()) Serial.read(); //Clear buffer
  while(Serial.available() == 0) delay(10); //Wait for user to press a key

  //If user reinserted the SD card it will cause OpenLog to reset so we need to re-navigate out to MONDAY
  changeDirectory(".."); //Return to root
  changeDirectory("MONDAY"); //Move into this sub directory

  long thingsDeleted = removeFile("*.TXT"); //This is not case sensitive
  Serial.print("Files deleted: ");
  Serial.println(thingsDeleted);

  Serial.println(F("Press a key to remove the MONDAY directory."));
  while(Serial.available()) Serial.read(); //Clear buffer
  while(Serial.available() == 0) delay(10); //Wait for user to press a key

  changeDirectory(".."); //Return to root

  thingsDeleted = remove("MONDAY", true); //Remove MONDAY and everything in it
  Serial.print("Things deleted: ");
  Serial.println(thingsDeleted);

  //Note: We cannot recordToOpenLog() here because we have deleted the log we were appending to
  //You must create a new file using appendFile() to continue logging
  
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

//Delete a given file
//Returns true if success
long removeFile(String fileNameToDelete)
{
  long numberOfFilesDeleted = remove(fileNameToDelete, false); //No need for -rf flag

  return(numberOfFilesDeleted);
}

//Remove a file or directory
//OpenLog will respond with the number of files deleted
long remove(String thingToDelete, boolean removeDirectoryContents)
{
  Wire.beginTransmission(OpenLogAddress);
  Wire.write(26); //Send the three ctrl+z escape characters
  Wire.write(26);
  Wire.write(26);
  Wire.print("rm "); //Include space
  if(removeDirectoryContents == true) Wire.print("-rf "); //extra flag to remove contents of directory
  Wire.print(thingToDelete);

  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack");

  //Upon completion Qwiic OpenLog will respond with 4 bytes, the number of files deleted 
  Wire.requestFrom(OpenLogAddress, 4);

  long filesDeleted = 0;
  while (Wire.available())
  {
    byte incoming = Wire.read();
    filesDeleted <<= 8;
    filesDeleted |= incoming;
  }

  return (filesDeleted); //Return the number of files removed

  //Qwiic OpenLog will continue logging whatever it next receives to the current open log
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

