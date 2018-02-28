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

#pragma once

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <Wire.h>

//The default I2C address for the Qwiic OpenLog is 0x2A (42). 0x29 is also possible.
#define QOL_DEFAULT_ADDRESS 42

//Platform specific configurations

//Define the size of the I2C buffer based on the platform the user has
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)

//I2C_BUFFER_LENGTH is defined in Wire.H
#define I2C_BUFFER_LENGTH BUFFER_LENGTH

#elif defined(__SAMD21G18A__)

//SAMD21 uses RingBuffer.h
#define I2C_BUFFER_LENGTH SERIAL_BUFFER_SIZE

#elif __MK20DX256__
//Teensy

#elif ARDUINO_ARCH_ESP32
//ESP32 based platforms

#else

//The catch-all default is 32
#define I2C_BUFFER_LENGTH 32

#endif
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Registers
#define THISREGISTER 0x0F

class QOL : public Print{
    //virtual size_t write(uint8_t);

    virtual size_t write(uint8_t *buffer, size_t size);
    virtual size_t write(uint8_t character);

  public:
    
    //By default use the default I2C addres, and use Wire port
    boolean begin(uint8_t deviceAddress = QOL_DEFAULT_ADDRESS, TwoWire &wirePort = Wire);
    boolean begin(int deviceAddress);

    boolean send(String, byte, byte);

    byte readRegister(uint8_t addr, uint8_t *outputPointer);
    boolean writeRegister(uint8_t addr, uint8_t val);

    void setI2CAddress(uint8_t addr); //Set the I2C address we read and write to

  private:

    //Variables
    TwoWire *_i2cPort; //The generic connection to user's chosen I2C hardware
    uint8_t _deviceAddress; //Keeps track of I2C address. setI2CAddress changes this.

    Stream *_debugPort; //The stream to send debug messages to if enabled. Usually Serial.
    boolean _printDebug = false; //Flag to print debugging variables
};
