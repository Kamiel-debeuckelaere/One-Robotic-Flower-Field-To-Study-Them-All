#include "Servo.h"
#include "mbed.h"

Servo::Servo(PinName Pin) : ServoPin(Pin) {}

int Servo:: SpeedConvert (int _speed)
{

    if (_speed > 50)
        _speed = 50;
    else if (_speed < 1)
        _speed =1;
    speed = 10000/_speed;
    return speed;

}

void Servo::SetPosition(int Pos)             //servo go to position
{
    NewPosition = Pos;
}

void Servo::Update()
{
    if(NewPosition != CurrentPosition) {

        if(NewPosition > CurrentPosition) {
            CurrentPosition++;
            Position =  CurrentPosition;

        } else  {
            CurrentPosition--;
            Position = CurrentPosition;
        }
    }
    Position = CurrentPosition;
}


void Servo::StartPulse()
{
    ServoPin = 1;

    PulseStop.attach_us(this, &Servo::EndPulse, Position);
}

void Servo::EndPulse()
{
    ServoPin = 0;
}

void Servo::Enable(int StartPos, int RefreshRate, int _speed)
{
    SpeedConvert(_speed);
    Period = (1000000/RefreshRate);                            //converts from HZ to period in  us
    Position = StartPos;
    NewPosition = StartPos;
    CurrentPosition = StartPos;                              //sets CurrentPosition to  StartPos value
    Pulse.attach_us(this, &Servo::StartPulse, Period);       //starts servo period Ticker r
    Speed.attach_us(this, &Servo::Update, speed);            //starts servo Speed Ticker  speed of servo update pulse width steps in  us,    slow =100000 100ms per step
}

void Servo::Disable()           //turns off Ticker Pulse
{
    Pulse.detach();
}