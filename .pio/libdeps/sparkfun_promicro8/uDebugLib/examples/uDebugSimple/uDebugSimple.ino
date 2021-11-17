//This example is set up to have some information sent to the Serial port
// and some only print DEBUG is defined.
//Try this example with and without DEBUG being defined.
/**
   DebugUtils defines:
*/
// Activate debug be defining DEBUG below. Deactivate by commenting the line below.
//#define DEBUG

// Hardware serial: This is the default so isn't needed for this example.
// #define UDEBUGLIB_DEV Serial

// Debug Serial speed only needs to be set if no serial is used when DEBUG is not defined.
//#define DEBUGUTILS_BAUDS 57600

#include <uDebugLib.h>

void setup() {
  Serial.begin(9600);
  //uDebugLibInit(); This not needed because we have some Serial.prints we want to use all the time.
  Serial.println("This line prints with or without DEBUG being defined");
  DEBUG_PRINT("Debug in same line");
  DEBUG_PRINTLN(" -- Debug with new line");
  DEBUG_PRINT(10, DEC); // Same line
  DEBUG_PRINTLN(99, DEC); // New line
  Serial.println("This line is after the debug print lines and alway prints with or without DEBUG defined");

}
//Output with DEBUG defined
/*
This line prints with or without DEBUG being defined
Debug in same line -- Debug with new line
1099
This line is after the debug print lines and alway prints with or without DEBUG defined
*/
//This the output when DEBUG is commented out
/*
This line prints with or without DEBUG being defined
This line is after the debug print lines and alway prints with or without DEBUG defined
 */

void loop() {
}
