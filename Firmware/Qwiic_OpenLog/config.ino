/*
  These functions read and write to the config.txt file. They also set the global configuration variables.
*/

#define CFG_FILENAME "config.txt" //This is the name of the file that contains the unit settings

#define MAX_CFG "119,103,14,0\n" //Address 119, escape char of ASCII(103), 14 times, new log mode
#define CFG_LENGTH (strlen(MAX_CFG) + 1) //Length of text found in config file. strlen ignores \0 so we have to add it back 
#define SEQ_FILENAME "SEQLOG00.TXT" //This is the name for the file when you're in sequential modeu

//Internal EEPROM locations for the user settings
#define LOCATION_SYSTEM_SETTING   0x02
#define LOCATION_FILE_NUMBER_LSB  0x03
#define LOCATION_FILE_NUMBER_MSB  0x04
#define LOCATION_ESCAPE_CHAR    0x05
#define LOCATION_MAX_ESCAPE_CHAR  0x06
#define LOCATION_I2C_ADDRESS    0x0D

#define MODE_NEWLOG     0
#define MODE_SEQLOG     1
#define MODE_COMMAND    2

//Change how OpenLog works
//1) Turn on unit, unit will create new file, and just start logging
//2) Turn on, append to known file, and just start logging
//3) Turn on, sit at command prompt
//4) Resets the newLog file number to zero
void systemMenu(byte command)
{
  byte systemMode = EEPROM.read(LOCATION_SYSTEM_SETTING);

  /*    NewSerial.println(F("1) New file logging"));
      NewSerial.println(F("2) Append file logging"));
      NewSerial.println(F("3) Command prompt"));
      NewSerial.println(F("4) Reset new file number"));
  */

  //Execute command
  if (command == '1')
  {
    EEPROM.write(LOCATION_SYSTEM_SETTING, MODE_NEWLOG);
    recordConfigFile(); //Put this new setting into the config file
  }
  else if (command == '2')
  {
    EEPROM.write(LOCATION_SYSTEM_SETTING, MODE_SEQLOG);
    recordConfigFile(); //Put this new setting into the config file
  }
  else if (command == '3')
  {
    EEPROM.write(LOCATION_SYSTEM_SETTING, MODE_COMMAND);
    recordConfigFile(); //Put this new setting into the config file
    return;
  }
  else if (command == '4')
  {
    EEPROM.write(LOCATION_FILE_NUMBER_LSB, 0);
    EEPROM.write(LOCATION_FILE_NUMBER_MSB, 0);

  }
  else if (command == '5')
  {
    NewSerial.print(F("Enter a new escape character: "));

    while (!NewSerial.available()); //Wait for user to hit character
    setting_escape_character = NewSerial.read();

    EEPROM.write(LOCATION_ESCAPE_CHAR, setting_escape_character);
    recordConfigFile(); //Put this new setting into the config file

    NewSerial.print(F("\n\rNew escape character: "));
    NewSerial.println(setting_escape_character, DEC);
  }
  else if (command == '6')
  {
    byte choice = 255;
    while (choice == 255)
    {
      NewSerial.print(F("\n\rEnter number of escape characters to look for (0 to 254): "));
      while (!NewSerial.available()); //Wait for user to hit character
      choice = NewSerial.read() - '0';
    }

    setting_max_escape_character = choice;
    EEPROM.write(LOCATION_MAX_ESCAPE_CHAR, setting_max_escape_character);
    recordConfigFile(); //Put this new setting into the config file

    NewSerial.print(F("\n\rNumber of escape characters needed: "));
    NewSerial.println(setting_max_escape_character, DEC);
  }
}


//Resets all the system settings to safe values
void setDefaultSettings(void)
{
  //Reset system to new log mode
  EEPROM.write(LOCATION_SYSTEM_SETTING, MODE_NEWLOG);

  //Reset escape character to ctrl+z
  EEPROM.write(LOCATION_ESCAPE_CHAR, 26);

  //Reset number of escape characters to 3
  EEPROM.write(LOCATION_MAX_ESCAPE_CHAR, 3);

  //Reset the I2C Address to default of 42
  EEPROM.write(LOCATION_I2C_ADDRESS, 42);

  //These settings are not recorded to the config file
  //We can't do it here because we are not sure the FAT system is init'd
}

//Reads the current system settings from EEPROM
//If anything looks weird, reset setting to default value
void readSystemSettings(void)
{
  //Determine the system mode we should be in
  //Default is NEWLOG mode
  setting_system_mode = EEPROM.read(LOCATION_SYSTEM_SETTING);
  if (setting_system_mode > 5)
  {
    setting_system_mode = MODE_NEWLOG; //By default, unit will turn on and go to new file logging
    EEPROM.write(LOCATION_SYSTEM_SETTING, setting_system_mode);
  }

  //Read the escape_character
  //Default is ASCII(26)/ctrl+z
  setting_escape_character = EEPROM.read(LOCATION_ESCAPE_CHAR);
  if (setting_escape_character == 0 || setting_escape_character == 255)
  {
    setting_escape_character = 26; //Reset escape character to ctrl+z
    EEPROM.write(LOCATION_ESCAPE_CHAR, setting_escape_character);
  }

  //Read the number of escape_characters to look for
  //Default is 3
  setting_max_escape_character = EEPROM.read(LOCATION_MAX_ESCAPE_CHAR);
  if (setting_max_escape_character == 255)
  {
    setting_max_escape_character = 3; //Reset number of escape characters to 3
    EEPROM.write(LOCATION_MAX_ESCAPE_CHAR, setting_max_escape_character);
  }

  //Read what I2C address we should use
  //Default is 42 (or 41 if jumper is closed)
  setting_i2c_address = EEPROM.read(LOCATION_I2C_ADDRESS);
  if (setting_i2c_address == 255)
  {
    setting_i2c_address = 42; //By default, we listen for 42
    EEPROM.write(LOCATION_I2C_ADDRESS, setting_i2c_address);
  }
}

void readConfigFile(void)
{
  SdFile configFile;

  if (!sd.chdir()) systemError(ERROR_ROOT_INIT); // open the root directory

  char configFileName[strlen(CFG_FILENAME)]; //Limited to 8.3
  strcpy_P(configFileName, PSTR(CFG_FILENAME)); //This is the name of the config file. 'config.sys' is probably a bad idea.

  //Check to see if we have a config file
  if (!configFile.open(configFileName, O_READ)) {
    //If we don't have a config file already, then create config file and record the current system settings to the file
#if DEBUG
    NewSerial.println(F("No config found - creating default:"));
#endif
    configFile.close();

    //Record the current eeprom settings to the config file
    recordConfigFile();
    return;
  }

  //If we found the config file then load settings from file and push them into EEPROM
#if DEBUG
  NewSerial.println(F("Found config file!"));
#endif

  //Read up to 13 characters from the file. There may be a better way of doing this...
  char c;
  byte settingsLength;

  byte settings_string[CFG_LENGTH]; //"119,103,14,0\n" = Address 119, escape char of ASCII(103), 14 times, new log mode

  for (settingsLength = 0 ; settingsLength < CFG_LENGTH ; settingsLength++) {
    if ( (c = configFile.read()) < 0) break; //We've reached the end of the file
    if (c == '\n') break; //Bail if we hit the end of this string
    settings_string[settingsLength] = c;
  }
  configFile.close();

#if DEBUG
  //Print line for debugging
  NewSerial.print(F("Text Settings: "));
  for (byte i = 0 ; i < settingsLength ; i++)
    NewSerial.write(settings_string[i]);
  NewSerial.println();
  NewSerial.print(F("Len: "));
  NewSerial.println(settingsLength);
#endif

  //Default the system settings in case things go horribly wrong
  long new_system_i2c_address = 42;
  byte new_system_mode = MODE_NEWLOG;
  byte new_system_escape = 26;
  byte new_system_max_escape = 3;

  //Parse the settings out
  byte i = 0, j = 0, setting_number = 0;
  char new_setting[8]; //Max length of a setting is 6, the bps setting = '115200' plus '\0'
  byte new_setting_int = 0;

  for (i = 0 ; i < settingsLength; i++)
  {
    //Pick out one setting from the line of text
    for (j = 0 ; settings_string[i] != ',' && i < settingsLength && j < 6 ; )
    {
      new_setting[j] = settings_string[i];
      i++;
      j++;
    }

    new_setting[j] = '\0'; //Terminate the string for array compare
    new_setting_int = atoi(new_setting); //Convert string to int

    if (setting_number == 0) //I2C Address
    {
      new_system_i2c_address = strToLong(new_setting);

      //7-bit I2C addresses must be between 0x08 and 0x77
      if (new_system_i2c_address < 0x08 || new_system_i2c_address > 0x77) new_system_i2c_address = 42; //Default to 42
    }
    else if (setting_number == 1) //Escape character
    {
      new_system_escape = new_setting_int;
      if (new_system_escape == 0 || new_system_escape > 127) new_system_escape = 26; //Default is ctrl+z
    }
    else if (setting_number == 2) //Max amount escape character
    {
      new_system_max_escape = new_setting_int;
      if (new_system_max_escape > 254) new_system_max_escape = 3; //Default is 3
    }
    else if (setting_number == 3) //System mode
    {
      new_system_mode = new_setting_int;
      if (new_system_mode == 0 || new_system_mode > MODE_COMMAND) new_system_mode = MODE_NEWLOG; //Default is NEWLOG
    }
    else
      //We're done! Stop looking for settings
      break;

    setting_number++;
  }

  //We now have the settings loaded into the global variables. Now check if they're different from EEPROM settings
  boolean recordNewSettings = false;

  if (new_system_i2c_address != setting_i2c_address) {
    //If the i2c address from the file is different from the current setting,
    //then update the setting to the file setting and re-init Wire
    EEPROM.write(LOCATION_I2C_ADDRESS, new_system_i2c_address);
    setting_i2c_address = new_system_i2c_address;

    recordNewSettings = true;
  }

  if (new_system_mode != setting_system_mode) {
    //Goto new system mode
    setting_system_mode = new_system_mode;
    EEPROM.write(LOCATION_SYSTEM_SETTING, setting_system_mode);

    recordNewSettings = true;
  }

  if (new_system_escape != setting_escape_character) {
    //Goto new system escape char
    setting_escape_character = new_system_escape;
    EEPROM.write(LOCATION_ESCAPE_CHAR, setting_escape_character);

    recordNewSettings = true;
  }

  if (new_system_max_escape != setting_max_escape_character) {
    //Goto new max escape
    setting_max_escape_character = new_system_max_escape;
    EEPROM.write(LOCATION_MAX_ESCAPE_CHAR, setting_max_escape_character);

    recordNewSettings = true;
  }

  //We don't want to constantly record a new config file on each power on. Only record when there is a change.
  if (recordNewSettings == true)
    recordConfigFile(); //If we corrected some values because the config file was corrupt, then overwrite any corruption
#if DEBUG
  else
    NewSerial.println(F("Config file matches system settings"));
#endif

  //If we are in new log mode, find a new file name to write to
  if (setting_system_mode == MODE_NEWLOG)
    appendFile(newLog()); //Append data to the file name that newlog() returns
  else if (setting_system_mode == MODE_SEQLOG) //If we are in sequential log mode, determine if seqlog.txt has been created or not, and then open it for logging
    seqLog();
    
  //workingFile pointer has now been set
  systemStatus |= (1<<STATUS_FILE_OPEN); //File is open for business  

  //All done! New settings are loaded. System will now operate off new config settings found in file.
}

//Records the current EEPROM settings to the config file
//If a config file exists, it is trashed and a new one is created
void recordConfigFile(void)
{
  SdFile myFile;

  if (!sd.chdir()) systemError(ERROR_ROOT_INIT); // open the root directory

  char configFileName[strlen(CFG_FILENAME)];
  strcpy_P(configFileName, PSTR(CFG_FILENAME)); //This is the name of the config file. 'config.sys' is probably a bad idea.

  //If there is currently a config file, trash it
  if (myFile.open(configFileName, O_WRITE)) {
    if (!myFile.remove()) {
      NewSerial.println(F("Remove config failed"));
      myFile.close(); //Close this file
      return;
    }
  }

  //myFile.close(); //Not sure if we need to close the file before we try to reopen it

  //Create config file
  myFile.open(configFileName, O_CREAT | O_APPEND | O_WRITE);

  //Config was successfully created, now record current system settings to the config file

  char settings_string[CFG_LENGTH]; //"119,103,14,0\0" = Address 119, escape char of ASCII(103), 14 times, new log mode

  //Before we read the EEPROM values, they've already been tested and defaulted in the read_system_settings function
  byte current_system_i2c_address = EEPROM.read(LOCATION_I2C_ADDRESS);
  byte current_system_escape = EEPROM.read(LOCATION_ESCAPE_CHAR);
  byte current_system_max_escape = EEPROM.read(LOCATION_MAX_ESCAPE_CHAR);
  byte current_system_mode = EEPROM.read(LOCATION_SYSTEM_SETTING);

  //Convert system settings to visible ASCII characters
  sprintf_P(settings_string, PSTR("%d,%d,%d,%d\n"), current_system_i2c_address, current_system_escape, current_system_max_escape, current_system_mode);

  //Record current system settings to the config file
  if (myFile.write(settings_string, strlen(settings_string)) != strlen(settings_string))
    NewSerial.println(F("error writing to file"));

  myFile.println(); //Add a break between lines

  //Add a decoder line to the file
#define HELP_STR "i2c_address,escape,esc#,mode\n"
  char helperString[strlen(HELP_STR) + 1]; //strlen is preprocessed but returns one less because it ignores the \0
  strcpy_P(helperString, PSTR(HELP_STR));
  myFile.write(helperString); //Add this string to the file

  myFile.sync(); //Sync all newly written data to card
  myFile.close(); //Close this file
  //Now that the new config file has the current system settings, nothing else to do!
}

