/**
 * Simple convenient library to centralize simple serial debug with easy activation/deactivation and some customization
 *
 *
 * @copyright Naguissa
 * @author Naguissa
 * @email naguissa@foroelectro.net
 * @url https://github.com/Naguissa/uDebugLib
 * @version 1.0.2
 * @created 2018-07-27
 */

#ifndef UDEBUGLIB
	#define UDEBUGLIB
	#ifdef DEBUG
		#include <Arduino.h>
		#ifndef UDEBUGLIB_DEV
			#ifdef UDEBUGLIB_SOFWARESERIAL
				#ifndef UDEBUGLIB_RX
					#define UDEBUGLIB_RX 10
				#endif
				#ifndef UDEBUGLIB_TX
					#define UDEBUGLIB_TX 11
				#endif
				#ifndef SoftwareSerial_h
					#include <SoftwareSerial.h>
					//#error "You need to include SoftwareSerial in your project in order to use it: #include <SoftwareSerial.h>"
				#endif
				SoftwareSerial UDEBUGLIB_SS_DEV(UDEBUGLIB_RX, UDEBUGLIB_TX); // RX, TX
				#define UDEBUGLIB_DEV UDEBUGLIB_SS_DEV
			#else
				#define UDEBUGLIB_DEV Serial
			#endif
		#endif
		#ifndef UDEBUGLIB_BAUDS
			#define UDEBUGLIB_BAUDS 9600
		#endif
		void uDebugLibInitFunction() {
			UDEBUGLIB_DEV.begin(UDEBUGLIB_BAUDS);
			while (!UDEBUGLIB_DEV) ; // wait for serial port to connect. Needed for native USB
		}
		#define uDebugLibInit() uDebugLibInitFunction()
		#define DEBUG_PRINT UDEBUGLIB_DEV.print
		#define DEBUG_PRINTLN UDEBUGLIB_DEV.println
	#else
		#define uDebugLibInit()
		#define DEBUG_PRINT
		#define DEBUG_PRINTLN
	#endif
#endif


