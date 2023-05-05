Library Dependencies:

* [SerialPort](https://github.com/greiman/SerialPort)
    * Users will need to manually install the [`SerialPort` folder](https://github.com/greiman/SerialPort/tree/master/SerialPort) from this repository as an Arduino library
        * Method 1:
            * Download or clone the repository
            * Compress the [`SerialPort` folder](https://github.com/greiman/SerialPort/tree/master/SerialPort) into a `*.zip` file
            * In the Arduino IDE, install the library from the drop-down menu: **Sketch** > **Include Library** > **Add .ZIP Library**
        * Method 2:
            * Download or clone the repository
            * Copy the [`SerialPort` folder](https://github.com/greiman/SerialPort/tree/master/SerialPort) into the directory for the Arduino libraries
* [SdFat *(v1.1.4)*](https://github.com/greiman/SdFat)
    * To avoid compile issues, please use v1.1.4 (or another version prior to v2.x.x) of the SDFat Arduino library