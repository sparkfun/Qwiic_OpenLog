/*
  This is a library written for the [device name]
  SparkFun sells these at its website: www.sparkfun.com
  Do you like this library? Help support SparkFun. Buy a board!
  https://www.sparkfun.com/products/[device product ID]

  Written by Nathan Seidle @ SparkFun Electronics, December 28th, 2017

  The [device name] can remotely measure object temperatures within 1 degree C.

  This library handles the initialization of the [device name] and the calculations
  to get the temperatures.

  https://github.com/sparkfun/SparkFun_[device name]_Arduino_Library

  Development environment specifics:
  Arduino IDE 1.8.3

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "SparkFun_Qwiic_OpenLog_Arduino_Library.h"

//Write a single character to Qwiic OpenLog
size_t QOL::write(uint8_t character) {
  Wire.beginTransmission(_deviceAddress);
  Wire.write(character);
  if (Wire.endTransmission() != 0)
  {
    Serial.println("Error: Sensor did not ack");
    return(0);
  }

  return(1);
}

//Write a string to Qwiic OpenLong
//The common Arduinos have a limit of 32 bytes per I2C write
//This splits writes up into I2C_BUFFER_LENGTH sized chunks
size_t QOL::write(uint8_t *buffer, size_t size) {

  byte startPoint = 0;
  const char subBuffer[I2C_BUFFER_LENGTH];
  
  while (startPoint < size)
  {
    //Pick the smaller of 32 or the remaining number of characters to send
    byte endPoint = startPoint + I2C_BUFFER_LENGTH;
    if(endPoint > size) endPoint = size;
    
    //Copy a subset of the buffer to a temp sub buffer
    memcpy(subBuffer, &buffer[startPoint], endPoint - startPoint);

    Wire.beginTransmission(_deviceAddress);
    Wire.print(subBuffer); //Send the subBuffer
    if (Wire.endTransmission() != 0)
      return(0); //Error: Sensor did not ack

    startPoint = endPoint; //Advance the start point
  }

  return(size);
}

//Attempt communication with the device
//Return true if we got a 'Polo' back from Marco
boolean QOL::begin(uint8_t deviceAddress, TwoWire &wirePort)
{
  _deviceAddress = deviceAddress; //If provided, store the I2C address from user
  _i2cPort = &wirePort; //Grab which port the user wants us to use

  //We require caller to begin their I2C port, with the speed of their choice
  //external to the library
  //_i2cPort->begin();

  //Check communication with device
  /*uint8_t result;
    if (result == 0x09)
    {
    if (result == 0x09) //Check to see if the device responded with an expected result
      return (true);
    else
      return (false);
    }*/

  return (true);
}

boolean QOL::begin(int deviceAddress)
{
  begin(deviceAddress, Wire);
}

//Change the address we read and write to
void QOL::setI2CAddress(uint8_t addr)
{
  _deviceAddress = addr;
}

//Reads from a given location
//Stores the result at the provided outputPointer
byte QOL::readRegister(uint8_t addr, uint8_t* outputPointer)
{
  _i2cPort->beginTransmission(_deviceAddress);
  _i2cPort->write(addr);
  if (_i2cPort->endTransmission(false) != 0) //Send a restart command. Do not release bus.
  {
    return (0xFF); //Sensor did not ACK
  }

  _i2cPort->requestFrom((uint8_t)_deviceAddress, (uint8_t)1);
  if (_i2cPort->available())
  {
    return (_i2cPort->read());
  }
}

//Write a value to a spot
boolean QOL::writeRegister(uint8_t addr, uint8_t val)
{
  _i2cPort->beginTransmission(_deviceAddress);
  _i2cPort->write(addr);
  _i2cPort->write(val);
  if (_i2cPort->endTransmission() != 0)
    return (false); //Sensor did not ACK

  return (true);
}

//Write a value to a spot
boolean QOL::send(String thingToPrint, byte addr, byte val)
{
  _i2cPort->beginTransmission(_deviceAddress);
  _i2cPort->write(addr);
  _i2cPort->write(val);
  if (_i2cPort->endTransmission() != 0)
    return (false); //Sensor did not ACK

  return (true);
}


