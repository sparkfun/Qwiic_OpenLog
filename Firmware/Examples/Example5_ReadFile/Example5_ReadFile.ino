/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 2nd, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example shows how to:
    Record some strings to a default log
    Check the size of a given file name
    If that given file doesn't exist, create it with random characters
    Read back the contents of the given file (containing random characters)
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
  Serial.println("OpenLog Read File Test");
  
  //Record something to the default log
  recordToOpenLognl("OpenLog Read File Test");

  String myFile = "testFile.txt";

  //Get size of file
  long sizeOfFile = readFileSize(myFile);

  if (sizeOfFile == -1) //File does not exist. Create it.
  {
    Serial.println(F("File not found, creating one..."));
    
    appendFile(myFile); //Create file and begin writing to it

    //Write a random number of random characters to this new file
    recordToOpenLognl("The Beginning");
    randomSeed(analogRead(A0));

    //Write 300 to 500 random characters to the file
    int charsToWrite = random(300, 500);
    for(int x = 0 ; x < charsToWrite ; x++)
    {
      byte myCharacter = random('a', 'z'); //Pick a random letter, a to z
      
      Wire.beginTransmission(OpenLogAddress);
      Wire.write(myCharacter);
      if (Wire.endTransmission() != 0)
        Serial.println("Error: Sensor did not ack");
    }
    recordToOpenLognl("\nThe End");
  }
  else
  {
    Serial.println("File found!");
  }

  //Get size of file
  sizeOfFile = readFileSize(myFile);

  if (sizeOfFile > -1)
  {
    Serial.print(F("Size of file: "));
    Serial.println(sizeOfFile);

    readFile(myFile); //Print the contents of the file to the terminal
  }
  else
  {
    Serial.println(F("Size error: File not found"));
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

//Given a file name, read the contents of it
void readFile(String fileName)
{
  //Use size command to find out how much to read
  long leftToRead = readFileSize(fileName);

  Wire.beginTransmission(OpenLogAddress);
  Wire.write(26); //Send the three ctrl+z escape characters
  Wire.write(26);
  Wire.write(26);
  Wire.print("read "); //Include space
  Wire.print(fileName);
  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack");

  //Upon completion Qwiic OpenLog will respond with the file contents. Master can request up to 32 bytes at a time.
  //Qwiic OpenLog will respond until it reaches the end of file then it will report zeros.

  Serial.println(F("Contents of file: "));
  while (leftToRead > 0)
  {
    byte toGet = 32; //Request up to a 32 byte block
    if(leftToRead < toGet) toGet = leftToRead; //Go smaller if that's all we have left

    Wire.requestFrom(OpenLogAddress, toGet); 
    while (Wire.available())
    {
      char incoming = Wire.read();
      Serial.write(incoming);
    }

    leftToRead -= toGet;
  }
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

