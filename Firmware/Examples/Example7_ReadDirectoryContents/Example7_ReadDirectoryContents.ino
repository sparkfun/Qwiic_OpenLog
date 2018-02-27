/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example shows how to read the files in a given directory.
  You can use wildcards if desired. This is handy for listing a certain type of file such
  as *.LOG or LOG01*.TXT

  If you need to check if a file exists, use the readFileSize command (there's an
  example sketch showing how).
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
  recordToOpenLognl(F("Let's read the current directory!"));

  Serial.println("List of things in this directory:");
  //printListOfFiles(); //Give me everything, no limit to the size of the list
  //printListOfFiles("*/"); //Get just directories
  //printListOfFiles("*.*", 50); //Get just files, limit of 50
  //printListOfFiles("LOG000*.TXT"); //Get just the logs LOG00000 to LOG00099 if they exist.
  printListOfFiles("LOG*.TXT", 25); //Give me a list, up to 25 files
  
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

//Read the list of files in the current directory
//No options
String printListOfFiles()
{
  return(printListOfFiles("", 1000)); //Set limit of list to 1000 items
}

//Use options but limit list size to 25
String printListOfFiles(String options)
{
  return(printListOfFiles(options, 1000)); //Limit to a list of 25 items
}

//Read the list of files in the current directory
//With options
String printListOfFiles(String options, int maxList)
{
  Wire.beginTransmission(OpenLogAddress);
  Wire.write(26); //Send the three ctrl+z escape characters
  Wire.write(26);
  Wire.write(26);
  Wire.print("ls");
  if (options.length() > 0)
    Wire.print(" " + options); //Space between ls and options is needed
  
  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack");

  //Upon completion Qwiic OpenLog will have a file name or directory name ready to respond with, terminated with a \0
  //It will continue to respond with a file name or directory until it responds with all 0xFFs (end of list)

  //Read the list until we hit the end or the maxList limit
  for(int x = 0 ; x < maxList ; x++)
  {
    String fileName = "";
    Wire.requestFrom(OpenLogAddress, 32); 
    
    byte charsReceived = 0;
    while (Wire.available())
    {
      byte incoming = Wire.read();

      if(incoming == '\0')
      {
        //This is the end of the file name. Now print it.
        Serial.println(fileName);
        break; //Go request the next file. We don't need to read any more of the 32 bytes
      }
      else if(charsReceived == 0 && incoming == 0xFF)
      {
        return; //End of the file listing
      }
      else
      {
        fileName += (char)incoming; //Add this byte to the file name
      }

      charsReceived++;
    }
  }

  return; //We've read the max number of files
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

