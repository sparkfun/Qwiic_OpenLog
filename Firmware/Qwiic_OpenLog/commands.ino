/*
  These functions are at the core of how users can send commands to OpenLog. If user sends proper escape characters
  then these functions parse and respond to the command the user has sent.
*/

//Internal EEPROM locations for the user settings
#define LOCATION_SYSTEM_SETTING   0x02
#define LOCATION_FILE_NUMBER_LSB  0x03
#define LOCATION_FILE_NUMBER_MSB  0x04
#define LOCATION_ESCAPE_CHAR    0x05
#define LOCATION_MAX_ESCAPE_CHAR  0x06
#define LOCATION_I2C_ADDRESS    0x0D

#define MODE_NEWLOG     0
#define MODE_SEQLOG     1

char general_buffer[30]; //Needed for command shell
#define MIN(a,b) ((a)<(b))?(a):(b)

void idReturn(char *myData) {
  responseType = RESPONSE_VALUE;
  loadArray((byte)valueMap.id);
  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
}

void statusReturn(char *myData) {
  responseType = RESPONSE_VALUE;
  loadArray((byte)valueMap.status);
  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
}

void firmwareMajorReturn(char *myData) {
  responseType = RESPONSE_VALUE;
  loadArray((byte)valueMap.firmwareMajor);
  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
}

void firmwareMinorReturn(char *myData) {
  responseType = RESPONSE_VALUE;
  loadArray((byte)valueMap.firmwareMinor);
  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
}

void addressReturn(char *myData) {
  if (myData[1] == 'e')
  {
    NewSerial.println(valueMap.i2cAddress);
    responseType = RESPONSE_VALUE;
    loadArray((byte)valueMap.i2cAddress);
  }
  else
  {
    String str(myData);
    byte tempAddress = str.toInt();
    NewSerial.print("0x");
    NewSerial.println(tempAddress, HEX);
    if (tempAddress < 0x08 || tempAddress > 0x77)
      return; //Command failed. This address is out of bounds.
    valueMap.i2cAddress = tempAddress;
    EEPROM.write(LOCATION_I2C_ADDRESS, valueMap.i2cAddress);

    newConfigData = true; //Tell the main loop to record the config.txt file

    //Our I2C address may have changed because of user's command
    startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus

    valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS);
  }
}

void initFunction(char *myData) {
  if (!sd.begin(SD_CHIP_SELECT, SPI_FULL_SPEED)) systemError(ERROR_CARD_INIT);
  if (!sd.chdir()) systemError(ERROR_ROOT_INIT); //Change to root directory

  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
}

void createFile(char *myData) {
  SdFile tempFile;
  if (myData == 0) return; //Argument missing. Command failed.

  //Note: We do not close the current working file that may or may not be open.

  responseType = RESPONSE_STATUS;

  //Try to open file
  if (tempFile.open(myData, O_CREAT | O_EXCL | O_WRITE)) //Will fail if file already exsists
  {
    valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
    tempFile.close(); //Everything is good, Close this new file we just opened
  }
  else
  {
    valueMap.status &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command Failure
  }
}

void mkDir(char *myData) {
  if (myData == 0) return; //Argument missing. Command failed.
  responseType = RESPONSE_STATUS;
  if (sd.mkdir(myData) == true) //Make a directory in our current folder
  {
    valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
  }
  return;
}

void chDir(char *myData) {

  responseType = RESPONSE_STATUS;

  if (myData[0] == '.' && myData[1] == '.')
  {
    //User is trying to move up the directory tree. Move to root instead
    //TODO store the parent directory name and change to that instead
    if (sd.chdir("/")) //Change to root
    {
      valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
      valueMap.status |= (1 << STATUS_IN_ROOT_DIRECTORY);
      return;
    }
    else
    {
      valueMap.status &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command failed
      valueMap.status &= ~(1 << STATUS_IN_ROOT_DIRECTORY); //It is unknown what directory we are in
      return;
    }
  }
  else
  {
    //User is trying to change to a named subdirectory
    if (sd.chdir(myData))
    {
      valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
      valueMap.status &= ~(1 << STATUS_IN_ROOT_DIRECTORY); //We are no longer in root
      return;
    }
    else
    {
      valueMap.status &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command failed
      valueMap.status &= ~(1 << STATUS_IN_ROOT_DIRECTORY); //It is unknown what directory we are in
      return;
    }
  }
}

void readFile(char *myData) {
  //Close any previous file
  if (workingFile.isOpen()) workingFile.close();

  responseType = RESPONSE_FILE_READ;

  //Search file in current directory and open it
  if (!workingFile.open(myData, O_READ))
  {
    //Failed to open file
    valueMap.status &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command failed
    valueMap.status &= ~(1 << STATUS_FILE_OPEN); //File not open

    //File is not open so clear buffer.
    responseSize = 1;
    responseBuffer[0] = 0xFF;

    return;
  }

  //Argument 2: File seek position
  if (valueMap.startPosition != 0) {
    if (!workingFile.seekSet(valueMap.startPosition)) {
      workingFile.close();

      valueMap.status &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command failed
      valueMap.status &= ~(1 << STATUS_FILE_OPEN); //File not open

      //File is not open so clear buffer.
      responseSize = 1;
      responseBuffer[0] = 0xFF;

      return;
    }
  }

  //Load the responseBuffer with up to 32 bytes from the file contents
  loadArrayWithFile();

  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command succeeded
  valueMap.status |= (1 << STATUS_FILE_OPEN); //File open
  return;
}

void setStartPosition(char *myData) {
  valueMap.startPosition = (int)myData[0];
  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
}

void openFile(char *myData) {

  //Argument 1: File name
  //Find the end of a current file and begins writing to it
  //Ends only when the user inputs the escape character
  if (myData == 0) return; //Argument missing. Command failed.

  responseType = RESPONSE_STATUS;

  //Point workingFile at this new file name
  //appendFile() closes any currently open file
  if (appendFile(myData) == true)
  {
    valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
    valueMap.status |= (1 << STATUS_FILE_OPEN); //Set status bit
  }
  else
  {
    valueMap.status &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command fail
    valueMap.status &= ~(1 << STATUS_FILE_OPEN); //Clear status bit
  }
}

//we don't actually want to sync() every time we write to the buffer
//writing a full buffer to the card and writing 1 or 2 bytes seems
//to take about the same amount of time. Once the buffer is full
//it will automatically sync anyways.
void writeFile(char *myData) {
  workingFile.write(myData, strlen(myData));
  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
}

//if you definitely want your buffer synced right now then you can 
//manually call it
void syncFile(char *myData) {
  workingFile.sync();
  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
}

void fileSize(char *myData) {
  responseType = RESPONSE_VALUE;

  if (workingFile.isOpen()) workingFile.close();
  //Search file in current directory and open it
  if (workingFile.open(myData, O_READ)) {
    loadArray(workingFile.fileSize()); //Put this data in the I2C response buffer
    workingFile.close();
    valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
  }
  else
  {
    //Indicate no file is found
    loadArray((long) - 1); //Put this data in the I2C response buffer
    valueMap.status &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command failed
  }
}

void listFiles(char *myData) {
  responseType = RESPONSE_FILE_LIST;

  if (myData == 0)  // has no arguments
  {
    // Don't use the 'ls()' method in the SdFat library as it does not
    // limit recursion into subdirectories.
    myData = "*\0";       // use global wildcard=
  }
  else      // has argument (and possible wildcards)
  {
    strupr(myData); //Convert to uppercase
  }

  fileListArguments = myData; //We have to remember what to search for between I2C interrupts

  sd.vwd()->rewind(); //Reset our location in the directory listing to the start

  //Load the responseBuffer with first file or directory name
  loadArrayWithFileName(sd.vwd(), myData);

  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
  return;
}

void removeFiles(char *myData) {
  if (myData == 0) return; //Argument missing. Command failed.

  SdFile tempFile;
  responseType = RESPONSE_VALUE;
  if (tempFile.open(sd.vwd(), myData, O_READ))
  {
    bool tempStatus = false;
    if (tempFile.isDir() || tempFile.isSubDir())
    {
      tempStatus = tempFile.rmdir();
    }
    else
    {
      tempFile.close();
      if (tempFile.open(myData, O_WRITE))
      {
        tempStatus = tempFile.remove();
      }
    }
    tempFile.close();

    if (tempStatus == true)
    {
      valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
      loadArray((long)1); //Success!
    }
    else
    {
      valueMap.status &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
      loadArray((long) - 1); //Fail
    }

    return;
  }

  //Argument 1: File wildcard removal
  //Fixed by dlkeng - Thank you!
  int32_t filesDeleted = 0;

  char fname[13]; //Used to store the file and directory names as we step through this directory

  sd.vwd()->rewind();
  while (tempFile.openNext(sd.vwd(), O_READ)) //Step through each object in the current directory
  {
    if (tempFile.isFile()) // Remove only files
    {
      if (tempFile.getSFN(fname)) // Get the filename of the object we're looking at
      {
        if (wildcmp(myData, fname))  // See if it matches the wildcard
        {
          tempFile.close();
          tempFile.open(fname, O_WRITE);  // Re-open for WRITE to be deleted
          if (tempFile.remove()) // Remove this file
          {
            ++filesDeleted;
          }
        }
      }
    }
    tempFile.close();
  }

  loadArray(filesDeleted);
  valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
  return;
}

void recursiveRemove(char *myData) {
  bool tempStatus = false;
  SdFile tempFile;
  responseType = RESPONSE_VALUE;
  //Remove the subfolder
  if (tempFile.open(sd.vwd(), myData, O_READ))
  {
    tempStatus = tempFile.rmRfStar();
    tempFile.close();
  }
  if (tempStatus == true)
  {
    valueMap.status |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    loadArray((long)1); //Success!
  }
  else
  {
    valueMap.status &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    loadArray((long) - 1); //Fail
  }

  return;
}

//Loads a long into the start of the responseBuffer
void loadArray(unsigned long myNumber)
{
  for (byte x = 0 ; x < sizeof(myNumber) ; x++)
    responseBuffer[x] = (myNumber >> (((sizeof(myNumber) - 1) - x) * 8)) & 0xFF;
  responseSize = sizeof(myNumber);
}

void loadArray(long myNumber)
{
  for (byte x = 0 ; x < sizeof(myNumber) ; x++)
    responseBuffer[x] = (myNumber >> (((sizeof(myNumber) - 1) - x) * 8)) & 0xFF;
  responseSize = sizeof(myNumber);
}

//Loads an int into the start of the responseBuffer
void loadArray(int myNumber)
{
  for (byte x = 0 ; x < sizeof(myNumber) ; x++)
    responseBuffer[x] = (myNumber >> (((sizeof(myNumber) - 1) - x) * 8)) & 0xFF;
  responseSize = sizeof(myNumber);
}

void loadArray(unsigned int myNumber)
{
  for (byte x = 0 ; x < sizeof(myNumber) ; x++)
    responseBuffer[x] = (myNumber >> (((sizeof(myNumber) - 1) - x) * 8)) & 0xFF;
  responseSize = sizeof(myNumber);
}

//Loads an byte into the start of the responseBuffer
void loadArray(byte myNumber)
{
  for (byte x = 0 ; x < sizeof(myNumber) ; x++)
    responseBuffer[x] = (myNumber >> (((sizeof(myNumber) - 1) - x) * 8)) & 0xFF;
  responseSize = sizeof(myNumber);
}

//Loads a bool into the start of the responseBuffer
void loadArray(boolean myStatus)
{
  responseBuffer[0] = myStatus;
  responseSize = sizeof(myStatus);
}

//If we are reading out the contents of a file, this loads the next (upto) 32 bytes from
//the workingFile.
void loadArrayWithFile(void)
{
  if (workingFile.isOpen())
  {
    //Prep array with 32 bytes
    for (responseSize = 0 ; responseSize < I2C_BUFFER_SIZE ; responseSize++)
    {
      int nextByte = workingFile.read();
      if (nextByte < 0) break; //End of file

      responseBuffer[responseSize] = (byte)nextByte;
    }
  }
  else
  {
    //File is not open so clear buffer.
    responseSize = 1;
    responseBuffer[0] = 0xFF;
  }
}

//A rudimentary way to convert a string to a long 32 bit integer
//Used by the read command, in command shell
uint32_t strToLong(const char* str)
{
  uint32_t l = 0;
  while (*str >= '0' && *str <= '9')
    l = l * 10 + (*str++ - '0');

  return l;
}

//The following functions are required for wildcard use
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Returns char* pointer to buffer if buffer is a valid number or
//0(null) if not.
char* isNumber(char* buffer, byte bufferLength)
{
  for (int i = 0; i < bufferLength; i++)
    if (!isdigit(buffer[i]))
      return 0;

  return buffer;
}

//Wildcard string compare.
//Written by Jack Handy - jakkhandy@hotmail.com
//http://www.codeproject.com/KB/string/wildcmp.aspx
byte wildcmp(const char* wild, const char* string)
{
  const char *cp = 0;
  const char *mp = 0;

  while (*string && (*wild != '*'))
  {
    if ((*wild != *string) && (*wild != '?'))
      return 0;

    wild++;
    string++;
  }

  while (*string)
  {
    if (*wild == '*')
    {
      if (!(*(++wild)))
        return 1;

      mp = wild;
      cp = string + 1;
    }
    else if ((*wild == *string) || (*wild == '?'))
    {
      wild++;
      string++;
    }
    else
    {
      wild = mp;
      string = cp++;
    }
  }

  while (*wild == '*')
    wild++;
  return !(*wild);
}

//End wildcard functions
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//------------------------------------------------------------------------------
// Listing functions
//  - Modeled loosely on functions written by dlkeng in relation to issue 135 https://github.com/sparkfun/OpenLog/issues/135

//From the current directory, load the next file name into the response array
void loadArrayWithFileName(FatFile * theDir, char * cmdStr)
{
#define FILENAME_EXT_SIZE   14 //Add one because directory names will have / added on end

  char fname[FILENAME_EXT_SIZE];
  SdFile tempFile;
  byte open_stat;         // file open status

  while ((open_stat = tempFile.openNext(theDir, O_READ)))
  {
    if (tempFile.getSFN(fname)) // Get the name of the object we're looking at
    {
      if (tempFile.isDir()) //If this is a directory, add / to end of name
        sprintf(fname, "%s/", fname);

      if (wildcmp(cmdStr, fname))  // See if it matches the wildcard
      {
        //We have a valid file name to report
        break;
      }
    }

    tempFile.close();
  }

  if (!open_stat) //No more files in this directory
  {
    responseSize = 1;
    responseBuffer[0] = 0xFF;
    return;
  }

  //Load fname into response array
  responseSize = sprintf(responseBuffer, "%s", fname) + 1; //Add 1 for null terminator

  //Hack to print rBuffer
  /*String temp = "";
    for (int x = 0 ; x < 15 ; x++)
    {
    temp += (char)responseBuffer[x];
    if (responseBuffer[x] == '\0') break;
    }
    NewSerial.println(temp);*/

  tempFile.close();
  return;
}
