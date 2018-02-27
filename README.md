SparkFun Qwiic OpenLog
========================================

![SparkFun Qwiic OpenLog](https://cdn.sparkfun.com//assets/parts/1/2/7/3/3/14586-VR_IMU__Qwiic__-_BNO080-01.jpg)

[*SparkX Qwiic OpenLog (SPX-14641)*](https://www.sparkfun.com/products/14641)

The [Qwiic](https://www.sparkfun.com/qwiic) system makes it easy to read all sorts of sensors, but what if you want to log that data? Qwiic OpenLog is an easy to use I2C based data logger. Simply write code like:

    recordToOpenLog("The battery voltage is:");
    recordToOpenLog(batteryVoltage);

And your data will be recorded to LOG00001.TXT for later review. 

The SparkFun Qwiic OpenLog is the smarter, better looking cousin to the extremely popular [OpenLog](https://www.sparkfun.com/products/13712). We've ported the serial based interface to I2C. Now you can daisy-chain lots of I2C devices and log them all without taking up your serial port.

Qwiic OpenLog supports clock stretching which means it performs even better than the original! Qwiic OpenLog will record data up to 21,000 bytes per second at 400kHz. As the receive buffer fills up QOL will hold the clock line letting the master know that it is busy. Once QOL is finished with a task it releases the clock allowing the data to continue flowing without corruption.

We've written a large number of example sketches to show how to record logs, create new logs, create and navigate directories, remove files and directories, and read the contents of files. We will be adding more features to the firmware over time and we've made it very easy to upgrade! If you're comfortable sending a sketch to an Uno then you can upgrade the firmware on Qwiic OpenLog.

Repository Contents
-------------------

* **/Examples** - A large number of examples to show how to record logs, create new logs, create and navigate directories, remove files and directories, and read the contents of files.
* **/Firmware** - The core sketch that runs Qwiic OpenLog
* **/Hardware** - Eagle design files (.brd, .sch)

License Information
-------------------

This product is _**open source**_! 

Please review the LICENSE.md file for license information. 

If you have any questions or concerns on licensing, please contact techsupport@sparkfun.com.

Please use, reuse, and modify these files as you see fit. Please maintain attribution to SparkFun Electronics and release any derivative under the same license.

Distributed as-is; no warranty is given.

- Your friends at SparkFun.