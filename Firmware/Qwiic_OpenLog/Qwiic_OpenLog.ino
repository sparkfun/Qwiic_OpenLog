/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: January 21st, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14586

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
  
  NewSerial uses approximately 60 bytes less RAM than built-in Serial so we use NewSerial library.

  At 115200bps with OpenLog Serial, it logged 11,520 bytes per second with a few buffer overruns
  At 100kHz I2C that's 10,000 bytes per second (10-bits per byte in I2C). Should be ok.
  At 400kHz, that's 40,000 bytes per second. We're going to need clock stretching.

  Red LED is 1.8V forward voltage. 3.3V - 1.8V = 1.5V across 1k = 1.5mA for the "PWR" LED

  What are the options we could set over I2C?
  Set escape character
  Set number of escape characters
  New file logging vs append logging
  Reset file number

  Need to check status byte from examples to see if OpenLog has SD card and is online

  (done) list files
  (done) make directory
  remove directory
  (done) change dir
  (done) read file
  (done) write to file
  (done) append
  (done) new
  (no) echo
  (done) size
  (no) disk
  reset

  Working! Records great 110k at 400kHz. Clock stretches correctly.
  The clock stretch occurs every 36ms, curious... Probably happening every 512 byte write

  Write example that scans for I2C devices to find lost OpenLog

  Commands look like this:
  "new test.txt" - Create file 'test.txt' in current directory
  "append test.txt" - Create (if not exist) and append to file 'test.txt'
  "md day2" - Make directory day2
  "cd day2" - Change directory to day2
  (future) "set <recording type>" - Set recording type. 0 = Create new log each power up, 1 = Append to SeqLog
  (future) "adr <i2c address>" - Change I2C address. Valid/allowed addresses are 0x08 to 0x77.
  (future) "esc <byte>" - Change escape character. Valid 0x01 to 0xFE.
  (future) "num <byte>" - Change number of escape characters. Valid 0x00 to 0xFF
  If the master issues a read after these commands are sent, QOL responds with systemStatus byte

  "ls" - List files and subdirectories in current directory
  "ls *.*" - List files in current directory
  "ls *<forwardslash>" - List subdirectories in current directory
  If the master issues a read after this command, QOL responds with /0 terminated string of object.
  Each subsequent read gets another object.
  Directory names end in / character.

  "size <file>" - Get size of file. -1 if file does not exist
  If the master issues a read after this command, QOL responds with 4 bytes, signed long of file size

  "read test.txt <start> <length> <type>" - Get contents of file
  If the master issues a read after this command, QOL responds with up to 32 bytes of file contents
  Used size command prior to read to determine the number of bytes to read.

  (future) "get <setting>" - status = 1 if success
  if the master issues a read after this command, QOL responds with the adr, esc, num, or set value

  (future) "default" = 1 if success
  reset to all factory settings

  (future) set time/date via timestamp and dateTimeCallback() function

  6 = OpenLog
  8 = Master

  TODO:
  Power savings
  ADR jumper
  Library
  Check status byte

*/

#include <Wire.h>

#define __PROG_TYPES_COMPAT__ //Needed to get SerialPort.h to work in Arduino 1.6.x

#include <SPI.h>
#include <SdFat.h> //We do not use the built-in SD.h file because it calls Serial.print
#include <SerialPort.h> //This is a new/beta library written by Bill Greiman. You rock Bill!
#include <EEPROM.h>
#include <FreeStack.h> //Allows us to print the available stack/RAM size

SerialPort<0, 32, 32> NewSerial;
//This is a very important buffer declaration. This sets the <port #, rx size, tx size>. We set
//the TX buffer to zero because we will be spending most of our time needing to buffer the incoming (RX) characters.

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/power.h> //Needed for powering down perihperals such as the ADC/TWI and Timers

#define SD_CHIP_SELECT 10 //On OpenLog this is pin 10

//Debug turns on (1) or off (0) a bunch of verbose debug statements. Normally use (0)
//#define DEBUG  1
#define DEBUG  0

#define I2C_BUFFER_SIZE 32 //For ATmega328 based Arduinos, the I2C buffer is limited to 32 bytes

void(* Reset_AVR) (void) = 0; //Way of resetting the ATmega

//The bigger the receive buffer, the less likely we are to drop characters at high speed. However, the ATmega has a limited amount of
//RAM. This debug mode allows us to view available RAM at various stages of the program
#define RAM_TESTING  0 //Off
//#define RAM_TESTING  1 //On

//STAT1 is labeled PWR on QOL
#define STAT1  5 //On PORTD
#define STAT1_PORT  PORTD

//STAT2 is labeled STAT on QOL and sits on the SCK line of the SD interface
//LED illuminates when the SD card is actively writing/reading
#define STAT2  5 //On PORTB
#define STAT2_PORT  PORTB

const byte stat1 = 5;  //Used as a power indicator
const byte stat2 = 13; //This is the SPI LED, indicating SD traffic

//Blinking LED error codes
#define ERROR_SD_INIT   3
#define ERROR_NEW_BAUD    5
#define ERROR_CARD_INIT   6
#define ERROR_VOLUME_INIT 7
#define ERROR_ROOT_INIT   8
#define ERROR_FILE_OPEN   9

#define OFF   0x00
#define ON    0x01

SdFat sd;
SdFile workingFile; //This is the main file we are writing to or reading from

byte setting_i2c_address = 42; //The 7-bit I2C address of this OpenLog
byte setting_system_mode; //This is the mode the system runs in, default is MODE_NEWLOG
byte setting_escape_character; //This is the ASCII character we look for to break logging, default is ctrl+z
byte setting_max_escape_character; //Number of escape chars before break logging, default is 3

const unsigned int MAX_IDLE_TIME_MSEC = 500; //Max idle time before unit syncs buffer and goes to sleep

//Variables used in the I2C interrupt
#define BUFFER_SIZE 256
byte incomingData[BUFFER_SIZE]; //Local buffer to record I2C bytes before committing to file
volatile int incomingDataSpot = 0; //Keeps track of where we are in the incoming buffer
volatile unsigned long lastSyncTime = 0; //Keeps track of the last time the file was synced
volatile byte escapeCharsReceived = 0; //We must receive X number of escape chars before going into command mode

byte commandBuffer[I2C_BUFFER_SIZE]; //Contains the incoming I2C bytes of a user's command. Can't be bigger than 32 bytes, limited by Arduino I2C buffer
volatile byte commandBufferSpot;

//These are the different types of datums the OpenLog can respond with
enum Response {
  RESPONSE_STATUS, //1 byte containing status bits
  RESPONSE_VALUE, //File size, number of files removed, etc. May be 1, 2, or 4 bytes depending on what loads it
  RESPONSE_FILE_READ, //Up to 32 bytes of a given file's contents
  RESPONSE_FILE_LIST //Up to 32 ASCII characters for a file or directory name
};
volatile Response responseType = RESPONSE_STATUS; //State engine that let's us know what the master is asking for
byte responseBuffer[I2C_BUFFER_SIZE]; //Used to pass data back to master
volatile byte responseSize = 1; //Defines how many bytes of relevant data is contained in the responseBuffer

volatile byte systemStatus = 0; //This byte contains the following status bits
#define STATUS_SD_INIT_GOOD 0
#define STATUS_LAST_COMMAND_SUCCESS 1
#define STATUS_LAST_COMMAND_KNOWN 2
#define STATUS_FILE_OPEN 3
#define STATUS_IN_ROOT_DIRECTORY 4

char* fileListArguments; //When reading a list of files, we have to remember what to search for between I2C interrupts

//Testing only
volatile long toPrint1 = 0;

void setup(void)
{
  pinMode(stat1, OUTPUT);

  //Power down various bits of hardware to lower power usage
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();

  //Shut off TWI, Timer2, Timer1, ADC
  ADCSRA &= ~(1 << ADEN); //Disable ADC
  ACSR = (1 << ACD); //Disable the analog comparator
  DIDR0 = 0x3F; //Disable digital input buffers on all ADC0-ADC5 pins
  DIDR1 = (1 << AIN1D) | (1 << AIN0D); //Disable digital input buffer on AIN1/0

  //power_twi_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_adc_disable();

  readSystemSettings(); //Load all system settings from EEPROM

  //Begin listening on I2C only after we've setup all our config and opened any files
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Wire.begin(setting_i2c_address); //Start I2C and answer calls for this address

  //Setup UART
  NewSerial.begin(9600);
  NewSerial.print(F("1"));

  //Setup SD & FAT
  if (sd.begin(SD_CHIP_SELECT, SPI_FULL_SPEED) == false)
  {
    systemStatus &= ~(1<<STATUS_SD_INIT_GOOD); //Init failed
    systemError(ERROR_CARD_INIT);
  }
  systemStatus |= (1<<STATUS_SD_INIT_GOOD); //Init success! 

  if (sd.chdir() == false) //Change to root directory. All new file creation will be in root.
  {
    systemStatus &= ~(1<<STATUS_SD_INIT_GOOD); //Init failed
    systemStatus &= ~(1<<STATUS_IN_ROOT_DIRECTORY); //Init failed
    systemError(ERROR_ROOT_INIT); 
  }
  systemStatus |= (1<<STATUS_IN_ROOT_DIRECTORY); //We are in root
  
  NewSerial.print(F("2"));

  //Search for a config file and load any settings found. This will over-ride previous EEPROM settings if found.
  readConfigFile();

#if DEBUG
  NewSerial.print(F("FreeStack: "));
  NewSerial.println(FreeStack());
#endif

  NewSerial.print(F("<")); //Prompt to indicate we are good to go

  digitalWrite(stat1, HIGH); //Turn on LED to indicate power/ok

  sd.chdir("/"); //Change 'volume working directory' to root

  systemStatus |= (1<<STATUS_LAST_COMMAND_SUCCESS); //Last command was success (start up default)
  systemStatus |= (1<<STATUS_LAST_COMMAND_KNOWN); //Last command was known (start up default)
}

void loop(void)
{
  //Start recording incoming characters

  //The I2C receiveEvent() interrupt records all incoming bytes to workingFile
  //If a command is detected in the I2C receiveEvent(), command shell is run. This allows for clock stretching
  //while we do file manipulations.

  if (toPrint1 != 0)
  {
    NewSerial.print("toPrint: ");
    NewSerial.println(toPrint1);

    //NewSerial.println("fileList: ");
    //String temp = (String)fileList;
    //NewSerial.print(temp);
    //NewSerial.println("Done");

    toPrint1 = 0;
  }

  if ( (millis() - lastSyncTime) > MAX_IDLE_TIME_MSEC) { //If we haven't received any characters after amount of time, goto sleep

    noInterrupts(); //Disable I2C interrupt
    if(workingFile.isOpen())
    {
      if (incomingDataSpot > 0)
      {
        workingFile.write(incomingData, incomingDataSpot);
        incomingDataSpot = 0;
        workingFile.sync(); //Sync the card before we go to sleep
      }
    }
    interrupts();

    //STAT1_PORT &= ~(1 << STAT1); //Turn off stat LED to save power

    power_timer0_disable(); //Shut down peripherals we don't need
    power_spi_disable();
    //sleep_mode(); //Stop everything and go to sleep. Wake up if serial character received

    power_spi_enable(); //After wake up, power up peripherals
    power_timer0_enable();

    lastSyncTime = millis(); //Reset the last sync time to now
  }
}

//When OpenLog receives data bytes, this function is called as an interrupt
//Arduino [Uno] buffer is limited to 32 bytes so we pull data into a second local buffer that is larger
//We also scan for control characters. If we detect enough of them, we have received a command
void receiveEvent(int numberOfBytesReceived)
{
  while (Wire.available())
  {
    //Record bytes to local array
    incomingData[incomingDataSpot] = Wire.read();

    if (incomingData[incomingDataSpot] == setting_escape_character)
    {
      escapeCharsReceived++;
      if (escapeCharsReceived == setting_max_escape_character)
      {
        //We have a command to parse

        //Get the remainder of the command
        commandBufferSpot = 0;
        while (Wire.available())
        {
          commandBuffer[commandBufferSpot++] = Wire.read();
          //commandBuffer is 32 bytes. We shouldn't spill over because receiveEvent can't receive more than 32 bytes
        }
        incomingDataSpot -= setting_max_escape_character; //Prevent logging of escape chars

        //Before we do any commands, record buffer to file
        if (incomingDataSpot > 0)
        {
          workingFile.write(incomingData, incomingDataSpot);
          workingFile.sync(); //Sync the card before we do commands
        }

        incomingDataSpot = -1; //Because it's about to increment
        escapeCharsReceived = 0;
        lastSyncTime = millis();

        commandShell(); //We want to act on any commands here so that the clock is stretched while we are doing card manipulations
      }
    }

    incomingDataSpot++;

    if (incomingDataSpot == BUFFER_SIZE)
    {
      //Record buffer to file
      workingFile.write(incomingData, BUFFER_SIZE);
      incomingDataSpot = 0;
      escapeCharsReceived = 0;
      lastSyncTime = millis();
    }
  }
}

//Send back a number of bytes via an array, max 32 bytes
//When OpenLog gets a request for data from the user, this function is called as an interrupt
//The interrupt will respond with different types of data depending on what response state we are in
//The user sets the response type using the command interface
void requestEvent()
{
  switch (responseType)
  {
    case RESPONSE_STATUS:
      //Respond with the system status byte
      Wire.write(systemStatus);
      break;

    case RESPONSE_VALUE:
      //File size, number of files removed, etc. May be 1, 2, or 4 bytes depending on what loads it
      //Respond with a 4 byte signed long indicating size of this file
      Wire.write(responseBuffer, responseSize);
      break;

    case RESPONSE_FILE_READ:
      //Respond with array containing up to 32 bytes of file contents
      Wire.write(responseBuffer, responseSize);

      //Fill the array with more bytes for the next call
      loadArrayWithFile();
      break;

    case RESPONSE_FILE_LIST:
      //Respond with a string of characters indicating the next file or directory name
      Wire.write(responseBuffer, responseSize);

      //Fill the array with more bytes for the next call
      loadArrayWithFileName(sd.vwd(), fileListArguments);
      break;

    default:
      Wire.write(0xF0); //Unknown response state
      break;
  }
}


//Handle errors by printing the error type and blinking LEDs in certain way
//The function will never exit - it loops forever inside blink_error
void systemError(byte error_type)
{
  NewSerial.print(F("Error "));
  switch (error_type)
  {
    case ERROR_CARD_INIT:
      NewSerial.print(F("card.init"));
      blink_error(ERROR_SD_INIT);
      break;
    case ERROR_VOLUME_INIT:
      NewSerial.print(F("volume.init"));
      blink_error(ERROR_SD_INIT);
      break;
    case ERROR_ROOT_INIT:
      NewSerial.print(F("root.init"));
      blink_error(ERROR_SD_INIT);
      break;
    case ERROR_FILE_OPEN:
      NewSerial.print(F("file.open"));
      blink_error(ERROR_SD_INIT);
      break;
  }
}

//Blink the status LED to indicate an error
void blink_error(byte ERROR_TYPE)
{
  while (1)
  {
    for (byte x = 0 ; x < ERROR_TYPE ; x++)
    {
      digitalWrite(stat1, HIGH);
      delay(200);
      digitalWrite(stat1, LOW);
      delay(200);
    }

    delay(2000);
  }
}

