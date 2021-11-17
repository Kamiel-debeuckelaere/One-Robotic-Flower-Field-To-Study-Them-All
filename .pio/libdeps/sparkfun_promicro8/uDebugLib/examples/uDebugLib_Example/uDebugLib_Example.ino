#include <Arduino.h>

/**
 * DebugUtils defines:
 */
// Activate debug:
#define DEBUG

// Hardware serial:
// #define UDEBUGLIB_DEV Serial

// SoftwareSerial
#define UDEBUGLIB_SOFWARESERIAL
#define UDEBUGLIB_RX 10
#define UDEBUGLIB_TX 11

// Speed
#define DEBUGUTILS_BAUDS 57600


#include <uDebugLib.h>



void setup() {
	uDebugLibInit();
	DEBUG_PRINT("Debug in same line");
	DEBUG_PRINTLN(" -- Debug with new line");
	DEBUG_PRINT(10, DEC); // Same line
	DEBUG_PRINTLN(99, DEC); // New line
}

void loop() {
}
