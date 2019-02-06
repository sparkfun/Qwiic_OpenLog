/*
  These functions create a new log, sequential log, or append to a log file.

  These are used when OpenLog powers up and when the user asks to change files via new or append commands
*/

//Log to a new file everytime the system boots
//Checks the spots in EEPROM for the next available LOG#####.txt file name
//Updates EEPROM and then returns the file name.
//Limited to 65535 files.
char* newLog(void)
{
  SdFile newFile; //This will contain the file for SD writing

  //Combine two 8-bit EEPROM spots into one 16-bit number
  uint16_t newFileNumber = (EEPROM.read(LOCATION_FILE_NUMBER_MSB) << 8) | EEPROM.read(LOCATION_FILE_NUMBER_LSB);

  //If both EEPROM spots are 255 (0xFF), that means they are un-initialized (first time OpenLog has been turned on)
  //Let's init them both to 0
  if (newFileNumber == 0xFFFF)
  {
    newFileNumber = 0; //By default, unit will start at file number zero
    EEPROM.write(LOCATION_FILE_NUMBER_LSB, 0x00);
    EEPROM.write(LOCATION_FILE_NUMBER_MSB, 0x00);
  }

  //The above code looks like it will forever loop if we ever create 65535 logs
  //Let's quit if we ever get to 65534
  //65534 logs is quite possible if you have a system with lots of power on/off cycles
  if (newFileNumber == 65534)
  {
    //Gracefully drop out to command prompt with some error
    NewSerial.print(F("!Too many logs:1!"));
    return (0); //Bail!
  }

  //If we made it this far, everything looks good - let's start testing to see if our file number is the next available

  //Search for next available log spot
  //char newFileName[] = "LOG00000.TXT";
  static char newFileName[13];
  while (1)
  {
    sprintf_P(newFileName, PSTR("LOG%05d.TXT"), newFileNumber); //Splice the new file number into this file name

    //Try to open file, if fail (file doesn't exist), then break
    if (newFile.open(newFileName, O_CREAT | O_EXCL | O_WRITE)) break;

    //Try to open file and see if it is empty. If so, use it.
    if (newFile.open(newFileName, O_READ))
    {
      if (newFile.fileSize() == 0)
      {
        //Success!
        break;
      }
      newFile.close(); // Close this existing file we just opened.
    }

    //Try the next number
    newFileNumber++;
    if (newFileNumber > 65533) //There is a max of 65534 logs
    {
      NewSerial.print(F("!Too many logs:2!"));
      return (0); //Bail!
    }
  }
  newFile.close(); //Close this new file we just opened

  newFileNumber++; //Increment so the next power up uses the next file #

  //Record new_file number to EEPROM
  EEPROM.write(LOCATION_FILE_NUMBER_LSB, newFileNumber & 0xFF); // LSB

  if (EEPROM.read(LOCATION_FILE_NUMBER_MSB) != newFileNumber >> 8) //Don't touch upper EEPROM if it's not new
    EEPROM.write(LOCATION_FILE_NUMBER_MSB, newFileNumber >> 8); // MSB

#if DEBUG
  NewSerial.print(F("\nCreated new file: "));
  NewSerial.println(newFileName);
#endif

  return (newFileName);
}

//Log to the same file every time the system boots, sequentially
//Checks to see if the file SEQLOG.txt is available
//If not, create it
//If yes, append to it
void seqLog(void)
{
  SdFile seqFile;

  char sequentialFileName[strlen(SEQ_FILENAME)]; //Limited to 8.3
  strcpy_P(sequentialFileName, PSTR(SEQ_FILENAME)); //This is the name of the config file. 'config.sys' is probably a bad idea.

  //Try to create sequential file
  if (!seqFile.open(sequentialFileName, O_CREAT | O_WRITE))
  {
    NewSerial.print(F("Error creating SEQLOG\n"));
    return;
  }

  seqFile.close(); //Close this new file we just opened

  appendFile(sequentialFileName);
}

//Given a file name attempt to open it for appending
//Upon completion, workingFile is set. The I2C interrupt now records to this file.
boolean appendFile(char* file_name)
{
  //If we have a workingFile open then close it.
  if (workingFile.isOpen()) workingFile.close(); //Forces sync of any cached text

  // O_CREAT - create the file if it does not exist
  // O_APPEND - seek to the end of the file prior to each write
  // O_WRITE - open for write
  if (!workingFile.open(file_name, O_CREAT | O_APPEND | O_WRITE)) return (false); //File failed to open

  if (workingFile.fileSize() == 0) {
    //This is a trick to make sure first cluster is allocated - found in Bill's example/beta code
    workingFile.rewind();
    workingFile.sync();
  }
  return (true); //File is now open
}

