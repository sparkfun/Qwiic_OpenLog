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

//The number of command line arguments
//Increase to support more arguments but be aware of the memory restrictions
//command <arg1> <arg2> <arg3> <arg4> <arg5>
#define MAX_COUNT_COMMAND_LINE_ARGS 5

//Used for wild card delete and search
struct commandArg
{
  char* arg; //Points to first character in command line argument
  byte arg_length; //Length of command line argument
};

static struct commandArg cmd_arg[MAX_COUNT_COMMAND_LINE_ARGS];

char general_buffer[30]; //Needed for command shell
#define MIN(a,b) ((a)<(b))?(a):(b)

//Look at incoming command buffer, parse it, and do various commands
//Returns a status indicating a variety of possible errors
void commandShell()
{
  systemStatus &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Assume command failed
  systemStatus |= (1 << STATUS_LAST_COMMAND_KNOWN); //Assume command is known

  SdFile tempFile;

#if DEBUG
  NewSerial.print(F("FreeStack: "));
  NewSerial.println(FreeStack());
#endif

  //Split the command line into arguments
  splitCmdLineArgs(commandBuffer, commandBufferSpot);

  //Argument 0: The actual command
  char* commandArg = getCmdArg(0);

  //Begin looking at commands

  //Re-init the SD interface
  if (strcmp_P(commandArg, PSTR("init")) == 0)
  {
    if (!sd.begin(SD_CHIP_SELECT, SPI_FULL_SPEED)) systemError(ERROR_CARD_INIT);
    if (!sd.chdir()) systemError(ERROR_ROOT_INIT); //Change to root directory

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    return;
  }

  //Set which type of logging the user wants
  else if (strcmp_P(commandArg, PSTR("set")) == 0)
  {
    //Argument 1: Number of logging type to go to
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    responseType = RESPONSE_STATUS;

    if (strToLong(commandArg) == 1) //Sequential logging mode
    {
      EEPROM.write(LOCATION_SYSTEM_SETTING, MODE_SEQLOG);
    }
    else //Normal logging mode for all other values we may be sent
    {
      EEPROM.write(LOCATION_SYSTEM_SETTING, MODE_NEWLOG);
    }

    newConfigData = true; //Tell the main loop to record the config.txt file

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    return;
  }

  //Set the escape character the user wants to use to send a command
  else if (strcmp_P(commandArg, PSTR("esc")) == 0)
  {
    //Argument 1: The escape character to listen for
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    responseType = RESPONSE_STATUS;

    setting_escape_character = strToLong(commandArg);
    EEPROM.write(LOCATION_ESCAPE_CHAR, setting_escape_character);

    newConfigData = true; //Tell the main loop to record the config.txt file

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    return;
  }

  //Set the number of escape characters before it triggers a command
  else if (strcmp_P(commandArg, PSTR("num")) == 0)
  {
    //Argument 1: Number of escape characters before command parsing is triggered
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    responseType = RESPONSE_STATUS;

    setting_max_escape_character = strToLong(commandArg);
    EEPROM.write(LOCATION_MAX_ESCAPE_CHAR, setting_max_escape_character);

    newConfigData = true; //Tell the main loop to record the config.txt file

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    return;
  }

  //Set the I2C address of this device
  else if (strcmp_P(commandArg, PSTR("adr")) == 0)
  {
    //Argument 1: I2C address to listen to
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    responseType = RESPONSE_STATUS;

    setting_i2c_address = strToLong(commandArg);

    //Error check
    if (setting_i2c_address < 0x08 || setting_i2c_address > 0x77)
      return; //Command failed. This address is out of bounds.

    EEPROM.write(LOCATION_I2C_ADDRESS, setting_i2c_address);

    newConfigData = true; //Tell the main loop to record the config.txt file
    
    //Our I2C address may have changed because of user's command
    startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    return;
  }

  //Set the log number to use when logging in New Log mode
  else if (strcmp_P(commandArg, PSTR("log")) == 0)
  {
    responseType = RESPONSE_STATUS;

    //If there is no argument then reset log number to 0
    int32_t logNumber = 0;

    //Argument 1 (optional): Two byte number to start from
    commandArg = getCmdArg(1);
    if (commandArg != 0) //We have a log number to store
    {
      logNumber = strToLong(commandArg) & 0xFFFF;
    }

    //Commit this number to EEPROM
    EEPROM.write(LOCATION_FILE_NUMBER_LSB, (logNumber & 0xFF));
    EEPROM.write(LOCATION_FILE_NUMBER_MSB, (logNumber & 0xFF00) >> 8);

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    return;
  }

  //Query OpenLog for its current settings
  else if (strcmp_P(commandArg, PSTR("get")) == 0)
  {
    //Argument 1: Setting the user wants to look up
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    responseType = RESPONSE_VALUE; //Respond with the setting value

    if (strcmp_P(commandArg, PSTR("set")) == 0)
    {
      loadArray(setting_system_mode);
    }
    else if (strcmp_P(commandArg, PSTR("adr")) == 0)
    {
      loadArray(setting_i2c_address);
    }
    else if (strcmp_P(commandArg, PSTR("esc")) == 0)
    {
      loadArray(setting_escape_character);
    }
    else if (strcmp_P(commandArg, PSTR("num")) == 0)
    {
      loadArray(setting_max_escape_character);
    }
    else if (strcmp_P(commandArg, PSTR("log")) == 0)
    {
      //Combine two 8-bit EEPROM spots into one 16-bit number
      uint16_t newFileNumber = (EEPROM.read(LOCATION_FILE_NUMBER_MSB) << 8) | EEPROM.read(LOCATION_FILE_NUMBER_LSB);
      loadArray(newFileNumber);
    }

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    return;
  }

  //Tell OpenLog to reset to factory defaults
  //Defaults: Normal logging, address to 42, escape to 26, escape num to 3, log number to zero
  //Begin logging with the first log number available
  else if (strcmp_P(commandArg, PSTR("default")) == 0)
  {
    responseType = RESPONSE_STATUS;

    setDefaultSettings(); //Set EEPROM to safe settings

    setting_system_mode = MODE_NEWLOG; //By default, unit will turn on and go to new file logging
    setting_escape_character = 26; //Reset escape character to ctrl+z
    setting_max_escape_character = 3; //Reset number of escape characters to 3
    setting_i2c_address = 42; //By default, we listen for 42

    EEPROM.write(LOCATION_FILE_NUMBER_LSB, 0); //Reset log number to 0
    EEPROM.write(LOCATION_FILE_NUMBER_MSB, 0);

    //Our I2C address may have changed because of user's command
    startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus

    newConfigData = true; //Tell the main loop to record the config.txt file

    //Close any previous file
    if (workingFile.isOpen()) workingFile.close();

    //Begin logging with the first log number available
    appendFile(newLog()); //Append data to the file name that newlog() returns

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    return;
  }

  //List files in the current directory
  else if (strcmp_P(commandArg, PSTR("ls")) == 0)
  {
    responseType = RESPONSE_FILE_LIST;

    if (countCmdArgs() == 1)  // has no arguments
    {
      // Don't use the 'ls()' method in the SdFat library as it does not
      // limit recursion into subdirectories.
      commandArg[0] = '*';       // use global wildcard
      commandArg[1] = '\0';
    }
    else      // has argument (and possible wildcards)
    {
      commandArg = getCmdArg(1);
      strupr(commandArg); //Convert to uppercase
    }

    fileListArguments = commandArg; //We have to remember what to search for between I2C interrupts

    sd.vwd()->rewind(); //Reset our location in the directory listing to the start

    //Load the responseBuffer with first file or directory name
    loadArrayWithFileName(sd.vwd(), commandArg);

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    return;
  }

  else if (strcmp_P(commandArg, PSTR("md")) == 0)
  {
    //Argument 1: Directory name
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    responseType = RESPONSE_STATUS;

    if (sd.mkdir(commandArg) == true) //Make a directory in our current folder
    {
      systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    }

    return;
  }

  //NOTE on using "rm <option>/<file> <subfolder>"
  // "rm -rf <subfolder>" removes the <subfolder> and all contents recursively
  // "rm <subfolder>" removes the <subfolder> only if its empty
  // "rm <filename>" removes the <filename>
  else if (strcmp_P(commandArg, PSTR("rm")) == 0)
  {
    //Argument 1: Remove option or file name/subdirectory to remove
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    responseType = RESPONSE_VALUE;

    //Argument 1: Remove subfolder recursively?
    if ((countCmdArgs() == 3) && (strcmp_P(commandArg, PSTR("-rf")) == 0))
    {
      bool tempStatus = true;
      //Remove the subfolder
      if (tempFile.open(sd.vwd(), getCmdArg(2), O_READ))
      {
        tempStatus = tempFile.rmRfStar();
        tempFile.close();
      }
      if (tempStatus == true)
      {
        systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
        loadArray((long)1); //Success!
      }
      else
      {
        systemStatus &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
        loadArray((long) - 1); //Fail
      }

      return;
    }

    //Argument 1: Remove subfolder if empty or remove file
    if (tempFile.open(sd.vwd(), commandArg, O_READ))
    {
      bool tempStatus = true;
      if (tempFile.isDir() || tempFile.isSubDir())
        tempStatus = tempFile.rmdir();
      else
      {
        tempFile.close();
        if (tempFile.open(commandArg, O_WRITE))
          tempStatus = tempFile.remove();
      }
      tempFile.close();

      if (tempStatus == true)
      {
        systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
        loadArray((long)1); //Success!
      }
      else
      {
        systemStatus &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
        loadArray((long) - 1); //Fail
      }

      return;
    }

    //Argument 1: File wildcard removal
    //Fixed by dlkeng - Thank you!
    int32_t filesDeleted = 0;

    char fname[13]; //Used to store the file and directory names as we step through this directory

    strupr(commandArg);

    while (tempFile.openNext(sd.vwd(), O_READ)) //Step through each object in the current directory
    {
      if (tempFile.isFile()) // Remove only files
      {
        if (tempFile.getSFN(fname)) // Get the filename of the object we're looking at
        {
          if (wildcmp(commandArg, fname))  // See if it matches the wildcard
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
    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
    return;
  }

  //Change directory
  else if (strcmp_P(commandArg, PSTR("cd")) == 0)
  {
    //Argument 1: Directory name
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    responseType = RESPONSE_STATUS;

    if (strcmp_P(commandArg, PSTR("..")) == 0)
    {
      //User is trying to move up the directory tree. Move to root instead
      //TODO store the parent directory name and change to that instead
      if (sd.chdir("/")) //Change to root
      {
        systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
        systemStatus |= (1 << STATUS_IN_ROOT_DIRECTORY);
        return;
      }
      else
      {
        systemStatus &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command failed
        systemStatus &= ~(1 << STATUS_IN_ROOT_DIRECTORY); //It is unknown what directory we are in
        return;
      }
    }
    else
    {
      //User is trying to change to a named subdirectory
      if (sd.chdir(commandArg))
      {
        systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command successful. Set status bit.
        systemStatus &= ~(1 << STATUS_IN_ROOT_DIRECTORY); //We are no longer in root
        return;
      }
      else
      {
        systemStatus &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command failed
        systemStatus &= ~(1 << STATUS_IN_ROOT_DIRECTORY); //It is unknown what directory we are in
        return;
      }
    }
  }

  //Setup workFile pointer to read from a given file
  else if (strcmp_P(commandArg, PSTR("read")) == 0)
  {
    //Argument 1: File name
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    //Close any previous file
    if (workingFile.isOpen()) workingFile.close();

    responseType = RESPONSE_FILE_READ;

    //Search file in current directory and open it
    if (workingFile.open(commandArg, O_READ) == false)
    {
      //Failed to open file
      systemStatus &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command failed
      systemStatus &= ~(1 << STATUS_FILE_OPEN); //File not open

      //File is not open so clear buffer.
      responseSize = 1;
      responseBuffer[0] = 0xFF;

      return;
    }

    //Argument 2: File seek position
    if ((commandArg = getCmdArg(2)) != 0) {
      if ((commandArg = isNumber(commandArg, strlen(commandArg))) != 0) {
        int32_t offset = strToLong(commandArg);
        if (!workingFile.seekSet(offset)) {
          //NewSerial.print(F("Error seeking to "));
          //NewSerial.println(commandArg);
          workingFile.close();

          systemStatus &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command failed
          systemStatus &= ~(1 << STATUS_FILE_OPEN); //File not open

          //File is not open so clear buffer.
          responseSize = 1;
          responseBuffer[0] = 0xFF;

          return;
        }
      }
    }

    //Load the responseBuffer with up to 32 bytes from the file contents
    loadArrayWithFile();

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command succeeded
    systemStatus |= (1 << STATUS_FILE_OPEN); //File open
    return;
  }

  //Get the size of a given file
  //Always returns four bytes. -1 on failure
  else if (strcmp_P(commandArg, PSTR("size")) == 0)
  {
    //Argument 1: File name - no wildcard search
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    responseType = RESPONSE_VALUE;

    //Search file in current directory and open it
    if (tempFile.open(commandArg, O_READ)) {
      loadArray(tempFile.fileSize()); //Put this data in the I2C response buffer
      tempFile.close();

      systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
    }
    else
    {
      //Indicate no file is found
      loadArray((long) - 1); //Put this data in the I2C response buffer
      systemStatus &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command failed
    }

    return;
  }

  //Get the current firmware version. Always returns two bytes.
  else if (strcmp_P(commandArg, PSTR("ver")) == 0)
  {
    responseType = RESPONSE_VALUE;

    unsigned int OpenLogVersion = versionMajor << 8 | versionMinor;

    loadArray(OpenLogVersion);
    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success

    return;
  }

  //Reset the AVR
  else if (strcmp_P(commandArg, PSTR("reset")) == 0)
  {
    Reset_AVR();
  }

  //Create new file
  else if (strcmp_P(commandArg, PSTR("new")) == 0)
  {
    //Argument 1: File name
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    //Note: We do not close the current working file that may or may not be open.

    responseType = RESPONSE_STATUS;

    //Try to open file
    if (tempFile.open(commandArg, O_CREAT | O_EXCL | O_WRITE)) //Will fail if file already exsists
    {
      tempFile.close(); //Everything is good, Close this new file we just opened
    }

    systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success

    return;
  }

  //Append to a given file
  else if (strcmp_P(commandArg, PSTR("append")) == 0)
  {
    //Argument 1: File name
    //Find the end of a current file and begins writing to it
    //Ends only when the user inputs the escape character
    commandArg = getCmdArg(1);
    if (commandArg == 0) return; //Argument missing. Command failed.

    responseType = RESPONSE_STATUS;

    //Point workingFile at this new file name
    //appendFile() closes any currently open file
    if (appendFile(commandArg) == true)
    {
      systemStatus |= (1 << STATUS_LAST_COMMAND_SUCCESS); //Command success
      systemStatus |= (1 << STATUS_FILE_OPEN); //Set status bit
    }
    else
    {
      systemStatus &= ~(1 << STATUS_LAST_COMMAND_SUCCESS); //Command fail
      systemStatus &= ~(1 << STATUS_FILE_OPEN); //Clear status bit
    }

    return;
  }

  systemStatus &= ~(1 << STATUS_LAST_COMMAND_KNOWN); //Clear bit. We don't know what this command is.

  NewSerial.print(F("unknown command: "));
  NewSerial.println(commandArg);
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

//Returns the number of command line arguments
byte countCmdArgs(void)
{
  byte count = 0;
  byte i = 0;
  for (; i < MAX_COUNT_COMMAND_LINE_ARGS; i++)
    if ((cmd_arg[i].arg != 0) && (cmd_arg[i].arg_length > 0))
      count++;

  return count;
}

//Safe index handling of command line arguments
//Pulls a given command (via the index) from the cmd_arg structure and copies it into the general_buffer
//Returns pointer to the general_buffer
char* getCmdArg(byte index)
{
  memset(general_buffer, 0, sizeof(general_buffer));
  if (index < MAX_COUNT_COMMAND_LINE_ARGS)
    if ((cmd_arg[index].arg != 0) && (cmd_arg[index].arg_length > 0))
      return strncpy(general_buffer, cmd_arg[index].arg, MIN(sizeof(general_buffer), cmd_arg[index].arg_length));

  return 0;
}

//Safe adding of command line arguments
void addCmdArg(char* buffer, byte bufferLength)
{
  byte count = countCmdArgs();
  if (count < MAX_COUNT_COMMAND_LINE_ARGS)
  {
    cmd_arg[count].arg = buffer;
    cmd_arg[count].arg_length = bufferLength;
  }
}

//Split the command line arguments
//Example:
//  read <filename> <start> <length>
//  arg[0] -> read
//  arg[1] -> <filename>
//  arg[2] -> <start>
//  arg[3] -> <end>
byte splitCmdLineArgs(byte* buffer, byte bufferLength)
{
  byte arg_index_start = 0;
  byte arg_index_end = 1;

  //Reset command line arguments
  memset(cmd_arg, 0, sizeof(cmd_arg));

  //Split the command line arguments
  while (arg_index_end < bufferLength)
  {
    //Search for ASCII 32 (Space)
    if ((buffer[arg_index_end] == ' ') || (arg_index_end + 1 == bufferLength))
    {
      //Fix for last character
      if (arg_index_end + 1 == bufferLength)
        arg_index_end = bufferLength;

      //Add this command line argument to the list
      addCmdArg(&(buffer[arg_index_start]), (arg_index_end - arg_index_start));
      arg_index_start = ++arg_index_end;
    }

    arg_index_end++;
  }

  //Return the number of available command line arguments
  return countCmdArgs();
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

