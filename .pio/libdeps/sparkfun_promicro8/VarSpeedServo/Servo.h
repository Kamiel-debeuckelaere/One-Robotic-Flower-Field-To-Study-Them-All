/* mbed Servo Library without using PWM pins
 * Copyright (c) 2010 Jasper Denkers
 *
 * Changes and additions made by David Garrison, October 2015 to add ability to independently control each servo's speed
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef MBED_SERVO_H
#define MBED_SERVO_H

#include "mbed.h"

/** Class to control servos on any pin, without using pwm 
 * Tested on Nucleo -L152RE @40MHz MCU clock and the 
 *  st disco -f746ng RUNNING AT 216mhZ.
 *
 * Example:
 * @code
 *
 * // Keep sweeping two servos from left to right
 * #include "mbed.h"
 * #include "Servo.h"
 * 
 *int main()
 *{
 *
 *   Servo Servo1(D12);            // create new instance of servo with output pin number
 *   Servo Servo2(D10);
 *   Servo1.Enable(1500,50,10);    // Start position ; in us (1500 = center), servo refresh rate in Hz (analog servos = 50 Hz), servo movement speed range from 1-50, 1 slowest, about 20 seconds/180 degrees
 *   Servo2.Enable(1500,50,10);
 *   while(1) {
 *
 *       Servo1.SetPosition(2300);
 *       Servo2.SetPosition(2300);
 *       wait(2.5);
 *
 *
 *       Servo1.SetPosition(700);
 *       Servo2.SetPosition(700);
 *       wait(2.5);
 *   }
 *
 *}
 * @endcode
 */

class Servo {

public:
    /** Create a new Servo object on any mbed pin
     *
     * @param Pin Pin on mbed to connect servo to
     */
    Servo(PinName Pin);
    
    /** Change the position of the servo. Position in us
     *
     * @param NewPos The new value of the servos position (us)
     */
    void SetPosition(int NewPos);
    
    /** speed oonversion from arbitary range of 1-50 to us to set Ticker Speed rate.
    * used in Enable function
    */
    int SpeedConvert (int _speed);  
    
    /** Enable the servo. Without enabling the servo won't be running. Startposition and period both in us.
     *
     * @param StartPos The position of the servo to start (us) 
     * @param RefreshRate.set in Hz  The time between every servo pulse. 20000 us = 50 Hz(standard) (us) 
                Analog servos typically use a 50Hz refresh rate, while digital servos can use up to a 300Hz refresh rate. 
     * @parm _speed The time between updating the pulse width of the servo pulses --controls speed of servo movement
     */
    void Enable(int StartPos, int RefreshRate, int _speed);
    
    /** Disable the servo. After disabling the servo won't get any signal anymore
     *
     */     
     void Disable();
     
     /** determines direction of servo's move, called by Ticker Speed
     */
     void Update();
     //int Position;
     int speed;
     int CurrentPosition;
    

private:
    void StartPulse();
    void EndPulse();
    int Position;
    int Period; 
    int NewPosition;
    DigitalOut ServoPin;
    Ticker Pulse;
    Ticker Speed;
    Timeout PulseStop;
};

#endif