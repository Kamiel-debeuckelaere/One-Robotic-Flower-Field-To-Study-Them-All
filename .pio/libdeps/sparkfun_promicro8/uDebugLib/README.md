# Arduino IDE uDebugLib library

## What is this repository for? ##

Simple convenient library to centralize simple serial debug with easy activation/deactivation and some customization



## How do I get set up? ##

You can get it from Arduino libraries directly, searching by uDebugLib.

For manual installation:

 * Get the ZIP.
 * Rename to uDebugLib.zip
 * Install library on Arduino


## Options (#defines) ##

All must be defined before the include.

 - DEBUG - Define it to activate debug; if not defined all debug code is removed and all calls are removed by the compiler
 - UDEBUGLIB_DEV - Define it to use a custom hardware serial device. If defined SoftwareSerial is disabled. Defaults to "Serial".
 - UDEBUGLIB_SOFWARESERIAL - If UDEBUGLIB_DEV is not defined you need to define this to tell debug library to use a SoftwareSerial device.
 - UDEBUGLIB_RX - When usking SoftwareSerial, it mermits you to specify RX pin (default: 10)
 - UDEBUGLIB_TX - When usking SoftwareSerial, it mermits you to specify TX pin (default: 11)
 - UDEBUGLIB_BAUDS - Set baud rate of device (default: 9600, very slow)


## Methods ##


uDebugLibInit() - Must be called on Setup, initialize device with baud rate

DEBUG_PRINT - Equivalent to print
DEBUG_PRINTLN - Equivalent to println

You can use that functions as Serial.print(ln), it will accept same arguments.




## Usage ##

```
#include <Arduino.h>

// DebugUtils defines:
#define UDEBUGLIB_BAUDS 57600

// Activate debug:
#define DEBUG

#include <DebugUtils.h>



void setup() {
	DebugUtilsInit();
	DEBUG_PRINT("Debug in same line");
	DEBUG_PRINTLN(" -- Debug with new line");
	DEBUG_PRINT(10, DEC); // Same line
	DEBUG_PRINTLN(99, DEC); // New line
}

void loop() {
}
```



## Example ##

Included on example folder, available on Arduino IDE.




## Who do I talk to? ##

 * [Naguissa](https://github.com/Naguissa)
 * https://www.foroelectro.net/librerias-arduino-ide-f29/udebuglib-arduino-libreria-simple-y-conveniente-pa-t242.html
 * https://www.naguissa.com



## Contribute ##

Any code contribution, report or comment are always welcome. Don't hesitate to use GitHub for that.


You can make a donation via PayPal: https://paypal.me/foroelectro


Thanks for your support.


Contributors hall of fame: https://www.foroelectro.net/hall-of-fame-f32/contributors-contribuyentes-t271.html