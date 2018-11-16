:Begin
avrdude -Cavrdude.conf -v -patmega328p -cusbtiny -Uflash:w:Qwiic_OpenLog.ino.with_bootloader.standard.hex:i -e -Ulock:w:0x3F:m -Uefuse:w:0xFD:m -Uhfuse:w:0xDE:m -Ulfuse:w:0xFF:m -B4
pause
goto Begin