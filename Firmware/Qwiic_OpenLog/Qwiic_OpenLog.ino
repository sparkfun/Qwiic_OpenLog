/*
  An I2C based datalogger - Like the OpenLog but for I2C
  By: Nathan Seidle
  SparkFun Electronics
  Date: January 21st, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14641

  Qwiic OpenLog records any incoming byte to the default log file. If quered by the master OpenLog will
  normally respond with a status byte with the following structure:

  Bit 0: SD/Init Good
  Bit 1: Last Command Succeeded
  Bit 2: Last Command Known
  Bit 3: File Currently Open
  Bit 4: In Root Directory
  Bit 5: 0 - Future Use
  Bit 6: 0 - Future Use
  Bit 7: 0 - Future Use

  At 115200bps with OpenLog Serial, it logged 11,520 bytes per second with a few buffer overruns
  At 100kHz I2C that's 10,000 bytes per second (10-bits per byte in I2C). Should be ok.
  At 400kHz, that's 40,000 bytes per second. We're going to need clock stretching.

  Red LED is 1.8V forward voltage. 3.3V - 1.8V = 1.5V across 1k = 1.5mA for the "PWR" LED

  3.5mA during idle
  12mA during constant write
  2.85mA idle, no card

  Why do we use NewSerial? NewSerial uses approximately 60 bytes less RAM than built-in Serial.

  Recording works great at 400kHz. Clock stretches correctly.
  The clock stretch occurs every 36ms, curious... Probably happening every 512 byte write

  Commands look like this:
  "new test.txt" - Create file 'test.txt' in current directory
  "append test.txt" - Create (if not exist) and append to file 'test.txt'
  "md day2" - Make directory day2
  "cd day2" - Change directory to day2
  "set 1" - Set recording type to SeqLog. 0 = Create new log each power up, 1 = Append to SeqLog
  "adr 98" - Change I2C address to 98. Valid/allowed addresses are 0x08 to 0x77.
  "esc 36" - Change escape character tp $. Valid 0 to 255
  "num 5" - Change number of escape characters to 5. Valid 0 to 255
  "log 100" - Change number of log to start at LOG00100.TXT. Valid 0 to 65,534
  If the master issues a read after these commands are sent, QOL responds with systemStatus byte

  "ls" - List files and subdirectories in current directory
  "ls *.*" - List files in current directory
  "ls *<forwardslash>" - List subdirectories in current directory
  If the master issues a read after this command, QOL responds with /0 terminated string of object.
  Each subsequent read gets another object.
  Directory names end in / character.
  
  "rm LOG*.TXT" - Remove all files that have the name LOG?????.TXT.
  "rm MONDAY" - Remove the *empty* directory MONDAY
  "rm -rf MONDAY" - Remove the directory MONDAY and any files within it

  "size <file>" - Get size of file. -1 if file does not exist
  If the master issues a read after this command, QOL responds with 4 bytes, signed long of file size

  "read test.txt <start> <length> <type>" - Get contents of file
  If the master issues a read after this command, QOL responds with up to 32 bytes of file contents
  Used size command prior to read to determine the number of bytes to read.

  "get <setting>" - Get the value of a setting
  If the master issues a read after this command, QOL responds with the adr, esc, num, or set value

  "default" - Reset to all factory settings
  Normal logging, address to 42, escape to 26, escape num to 3, log number to zero.
  Begin logging with the first log number available

  (future) Set time/date via timestamp and dateTimeCallback() function

  TODO:
  Library
  See if increasing the buffer size(S) increases record from 21k to more
  Write example that scans for I2C devices to find lost OpenLog
  get file count command - number of files in this directory
  get directory count command - number of directories in this directory
  add command to force sync. Sometimes card is pulled before 500ms?
  When SD card is out, does OpenLog hang the I2C bus?
*/

#include <Wire.h>

//Set version numbers for this firmware
const byte versionMajor = 1;
const byte versionMinor = 0;

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

//The I2C ADR jumper is on pin 3. Used to change the I2C address by decreasing it by 1.
//Default I2C address is 0x2A. When jumper is closed address goes down by one to 0x29.
const byte addr = 3;

//Blinking LED error codes
#define ERROR_SD_INIT   3
#define ERROR_CARD_INIT   6
#define ERROR_VOLUME_INIT 7
#define ERROR_ROOT_INIT   8
#define ERROR_FILE_OPEN   9

#define OFF   0x00
#define ON    0x01

SdFat sd;
SdFile workingFile; //This is the main file we are writing to or reading from

const unsigned int MAX_IDLE_TIME_MSEC = 500; //Max idle time before unit syncs buffer and goes to sleep

//Variables used in the I2C interrupt
volatile byte setting_i2c_address = 42; //The 7-bit I2C address of this OpenLog
volatile byte setting_system_mode; //This is the mode the system runs in, default is MODE_NEWLOG
volatile byte setting_escape_character; //This is the ASCII character we look for to break logging, default is ctrl+z
volatile byte setting_max_escape_character; //Number of escape chars before break logging, default is 3

#define LOCAL_BUFFER_SIZE 256
byte incomingData[LOCAL_BUFFER_SIZE]; //Local buffer to record I2C bytes before committing to file
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

volatile boolean newConfigData = false;

void setup(void)
{
  pinMode(stat1, OUTPUT);
  pinMode(addr, INPUT_PULLUP);

  //Power down various bits of hardware to lower power usage
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();

  //Shut off Timer2, Timer1, ADC
  ADCSRA &= ~(1 << ADEN); //Disable ADC
  ACSR = (1 << ACD); //Disable the analog comparator
  DIDR0 = 0x3F; //Disable digital input buffers on all ADC0-ADC5 pins
  DIDR1 = (1 << AIN1D) | (1 << AIN0D); //Disable digital input buffer on AIN1/0

  power_timer1_disable();
  power_timer2_disable();
  power_adc_disable();

  readSystemSettings(); //Load all system settings from EEPROM

  //Begin listening on I2C only after we've setup all our config and opened any files
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus

  //Setup UART
  NewSerial.begin(9600);
  NewSerial.print(F("1"));

  //Setup SD & FAT
  if (sd.begin(SD_CHIP_SELECT, SPI_FULL_SPEED) == false)
  {
    systemStatus &= ~(1 << STATUS_SD_INIT_GOOD); //Init failed
    systemError(ERROR_CARD_INIT);
  }
  systemStatus |= (1 << STATUS_SD_INIT_GOOD); //Init success!

  if (sd.chdir() == false) //Change to root directory. All new file creation will be in root.
  {
    systemStatus &= ~(1 << STATUS_SD_INIT_GOOD); //Init failed
    systemStatus &= ~(1 << STATUS_IN_ROOT_DIRECTORY); //Init failed
    systemError(ERROR_ROOT_INIT);
  }
  systemStatus |= (1 << STATUS_IN_ROOT_DIRECTORY); //We are in root

  NewSerial.print(F("2"));

  //Search for a config file and load any settings found. This will over-ride previous EEPROM settings if found.
  readConfigFile();

  //Our I2C address may have changed from the config file
  startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus

  NewSerial.print(F("(Adr: "));
  if (digitalRead(addr) == LOW)
    NewSerial.print(0x29, DEC);
  else
    NewSerial.print(setting_i2c_address, DEC);
  NewSerial.print(F(")"));

#if DEBUG
  NewSerial.print(F("FreeStack: "));
  NewSerial.println(FreeStack());
#endif

  NewSerial.print(F("<")); //Prompt to indicate we are good to go

  digitalWrite(stat1, HIGH); //Turn on LED to indicate power/ok

  sd.chdir("/"); //Change 'volume working directory' to root

  systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Last command was success (start up default)
  systemStatus |= (1 << STATUS_LAST_COMMAND_KNOWN); //Last command was known (start up default)
}

void loop(void)
{
  //The I2C receiveEvent() interrupt records all incoming bytes to workingFile

  //If an amount of time passes, put OpenLog into low power mode
  if ( (millis() - lastSyncTime) > MAX_IDLE_TIME_MSEC) { //If we haven't received any characters after amount of time, goto sleep

    noInterrupts(); //Disable I2C interrupt
    if (workingFile.isOpen())
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
    power_usart0_disable();
    sleep_mode(); //Stop everything and go to sleep. Wake up if I2C event occurs.

    power_spi_enable(); //After wake up, power up peripherals
    power_timer0_enable();
    power_usart0_enable();

    lastSyncTime = millis(); //Reset the last sync time to now
  }

  //Check to see if we need to commit the current EEPROM settings to a file
  //OpenLog likes to crash if we try to do this in the command shell (which is inside the interrupt)
  if (newConfigData == true)
  {
    recordConfigFile(); //Put this new setting into the config file
    newConfigData = false;
  }
}

//When OpenLog receives data bytes, this function is called as an interrupt
//Arduino [Uno] buffer is limited to 32 bytes so we pull data into a second local buffer that is larger
//We also scan for control characters. If we detect enough of them, we have received a command.
//If a command is detected the command shell is run. This allows for clock stretching while we do file manipulations.
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

    if (incomingDataSpot == LOCAL_BUFFER_SIZE)
    {
      //Record buffer to file
      workingFile.write(incomingData, LOCAL_BUFFER_SIZE);
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

      //Once read, clear the last command known and last command success bits
      systemStatus &= ~(1 << STATUS_LAST_COMMAND_SUCCESS);
      systemStatus &= ~(1 << STATUS_LAST_COMMAND_KNOWN);
      break;

    case RESPONSE_VALUE:
      //File size, number of files removed, etc. May be 1, 2, or 4 bytes depending on what loads it
      Wire.write(responseBuffer, responseSize);

      responseType = RESPONSE_STATUS; //Return to default state
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
      Wire.write(0x80); //Unknown response state
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

//Begin listening on I2C bus as I2C slave using the global variable setting_i2c_address
void startI2C()
{
  Wire.end();
  if (digitalRead(addr) == LOW)
    Wire.begin(0x29); //Force address to 0x29 if user has soldered jumper closed
  else
    Wire.begin(setting_i2c_address); //Start I2C and answer calls using address from EEPROM
}

