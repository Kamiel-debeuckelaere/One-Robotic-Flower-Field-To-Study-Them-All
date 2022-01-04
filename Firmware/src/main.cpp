#include <Arduino.h>
#include <FastLED.h>
#include <VarSpeedServo.h>
#include <rn2xx3.h>
//#include <EEPROM.h>
#include <cppQueue.h>
#include <LowPower.h>
#include <avr/power.h>

// include "variables" for normal function/ "variables_DEV" for developmental function
#include "variables" 

#include "fixed_variables"

#include <uDebugLib.h>

//FUNCTIONS

// 1) LoRaWAN module (RN2483)

  // structuring
typedef struct strVisitRec
{
  unsigned long startVisit; // internal clock time start visit (sec.) --> 4 bytes
  uint8_t visit;            // visit duration (sec.) --> 1 byte
} visitRec;

// reserve space in SRAM memory for this max. amount of visitElements
visitRec visitContainer[RESERVED_VISIT_SPACE]; 

//make queue from container, FIFO: first in first out, overwrite = false
cppQueue visitQueue(sizeof(visitRec), RESERVED_VISIT_SPACE, FIFO, false);

struct Alarms
{
  bool queueIsFull : 1;     // 0= false, 1= true, too many visits for container
  bool sendDuringVisit : 1; // 0 = false, 1 = true, send during visit
  bool batteryWarning : 1;  // 0= false, 1 = true, battery under warning voltage
  bool obstruction : 1;     // 0= false, 1 = true, IR sensor obstructed longer than threshold
  bool startBit : 1;        // always 1, for decoding purposes in NodeRed
};

struct uplinkmsg
{
  uint16_t battery;
  Alarms alarms;
} uplinkmsg;

struct datamsg
{
  uint16_t startTimeBeforeSend; //2 bytes
  uint8_t visitDuration; //1 byte
} datamsg;

    // sending message
TX_RETURN_TYPE SENDdata(uint8_t port, bool cnf = false)
{
  uplinkmsg.battery = Battery_voltage * 100;
  uplinkmsg.alarms.queueIsFull = queueIsFull;
  uplinkmsg.alarms.sendDuringVisit = sendDuringVisit;
  uplinkmsg.alarms.batteryWarning = batteryWarning;
  uplinkmsg.alarms.obstruction = obstruction;
  uplinkmsg.alarms.startBit = 1;

  String cmd = "mac tx ";
  cmd += (cnf) ? "cnf " : "uncnf ";
  cmd += port;
  cmd += " ";

  // Convert binary data to hexadecimal string
  String data;
  char buffer[3];
  for (uint8_t i = 0; i < sizeof(uplinkmsg); i++)
  {
    sprintf(buffer, "%02X", ((unsigned char *)&uplinkmsg)[i]);
    data += buffer;
    //memcpy(&msgBuffer[i * 2], &buffer, sizeof(buffer));
  }

  int sizeOfBuffer = sizeof(uplinkmsg);

  while ((sizeOfBuffer + 5 <= 53) && (!visitQueue.isEmpty()))
  { //checking if max. sending 53 bytes reached and if there is still data to send
    visitRec visitRec2send;
    visitQueue.pop(&visitRec2send);
    datamsg.startTimeBeforeSend = (millis() / PRECISION) - visitRec2send.startVisit; //visit started this many seconds before sending
    datamsg.visitDuration = visitRec2send.visit;

    for (uint8_t i = 0; i < sizeof(datamsg); i++)
    {
      sprintf(buffer, "%02X", ((unsigned char *)&datamsg)[i]);
      data += buffer;
    }

    sizeOfBuffer += sizeof(datamsg);
  }

  DEBUG_PRINT(F("   Size of data message: ")); //dev.
  DEBUG_PRINT(sizeOfBuffer);
  DEBUG_PRINT(F(" bytes")); //dev.

  retryCount = 0;
  TX_RETURN_TYPE resultOfSend;
  resultOfSend = myLora.txCommand(cmd, data, false);

  while ((TX_WITH_RX != resultOfSend) && retryCount < RETRY_AMOUNT)
  { //send data and receive downlink message from backend
    //DEBUG_PRINT(F("resultOfSend: ")); DEBUG_PRINTLN(resultOfSend);
    resultOfSend = myLora.txCommand(cmd, data, false);
    retryCount++;
  }

  if (retryCount == RETRY_AMOUNT)
  {
    DEBUG_PRINT(F(" / Failed TX and RX after "));
    DEBUG_PRINT(retryCount);
    DEBUG_PRINTLN(F(" retries"));
  }
  else
  {
    DEBUG_PRINTLN(F(" --> SEND"));
    timeToSleepString = myLora.getRx(); // get downlink message
    timeToSleep = timeToSleepString.toInt(); //change string to integer
    DEBUG_PRINT(F("Sleep message: "));
    DEBUG_PRINT(timeToSleep); DEBUG_PRINT(F(" - "));
    DEBUG_PRINTLN(timeToSleepString);
  }
}

// 2) EEPROM configuration

// Config that is stored and read from EEPROM
struct eepromConfig
{
  unsigned long writes; // Number of times a config is written to EEPROM
  char DevEUI[16 + 1];  // Device EUI as a \0 terminated HEX string. Example "0011223344556677"
  char AppEUI[16 + 1];  // Application EUI as a \0 terminated HEX string. Example "70B3D57ED00001A6"
  char AppKey[32 + 1];  // Application key as a \0 terminated HEX string. Example "A23C96EE13804963F8C2BD6285448198"
} config;

  // Write config data to EEPROM
void configWrite(void)
{
  config.writes++; // Increase
  eeprom_write_block((const void *)&config, (void *)0, sizeof(config));
}

  // Initialise config data and write to EEPROM (set everything to 0)
void configInit(void)
{
  //DEBUG_PRINTLN(F("Initializing configuration..."));
  //String hwEUI = myLora.hweui();
  myLora.hweui().toCharArray(config.DevEUI, sizeof(config.DevEUI));
  config.DevEUI[16] = 0; //add zero to terminate input
  memcpy(config.AppEUI, "0000000000000000", sizeof(config.AppEUI));
  config.AppEUI[16] = 0; //add zero to terminate input
  memcpy(config.AppKey, "00000000000000000000000000000000", sizeof(config.AppKey));
  config.AppKey[32] = 0; //add zero to terminate input
  configWrite();
  //DEBUG_PRINTLN(F("Initializing finished"));
}

  // Read config data from EEPROM (empty when uninitialised)
void configRead(void)
{
  eeprom_read_block((void *)&config, (void *)0, sizeof(config));
  if (config.writes == 0xFFFFFFFF)
  { // EEPROM has never been written... start with a clear config
    config.writes = 0;
    configInit();
  }
}

// 3) DingNet connection
  // Menu print
void menuPrint(void)
{
  DEBUG_PRINTLN(F(""));
  DEBUG_PRINTLN(F("  ############################"));
  DEBUG_PRINTLN(F(" #  ONE ROBOTIC FLOWER FIELD  #"));
  DEBUG_PRINTLN(F(" #     TO STUDY THEM ALL      #"));
  DEBUG_PRINTLN(F("  ############################"));
  DEBUG_PRINTLN(F(""));
  DEBUG_PRINTLN(F(" [1]  Device info"));
  DEBUG_PRINTLN(F(" [2]  Set DevEUI"));
  DEBUG_PRINTLN(F(" [3]  Set AppEUI"));
  DEBUG_PRINTLN(F(" [4]  Set AppKey"));
  DEBUG_PRINTLN(F(" [0]  Start operational mode"));
  DEBUG_PRINTLN(F(" [R]  Factory reset"));
  DEBUG_PRINTLN(F(""));
}

  // Read serial input (choice of menu, typing by user) until \r encounter or buffer is full (buflen-1 chars).
    // buffer is always 0 terminated!
    // Remove \r and \n from input
    // Return number of characters read
int inputReadline(char *buf, uint8_t buflen)
{
  uint8_t cnt = 0;

  memset(buf, 0, buflen); // Clear current buffer

  while (cnt < (buflen - 1))
  {
    while (Serial.available() > 0)
    {
      int data = Serial.read();
      Serial.write(data); // echo input back
      if (data == '\n')
      {
        continue;
      }
      if (data == '\r')
      {
        goto done;
      }
      buf[cnt] = data;
      cnt++;
    }
    delay(1); // Wait a little before checking serial again
  }
done:
  DEBUG_PRINTLN(F("")); // Print a newline
  return cnt;
}

  // Check if the first 'len' characters of 'buf' are a hexadecimal string
    // Also modify buf and uppercase all characters
    // returns true if all checks ok, false otherwise
bool verifyHexstring(char *buf, uint8_t len)
{
  for (uint8_t i = 0; i < len; i++)
  {
    if (isHexadecimalDigit(buf[i])) // test if hexadecimal
    {
      buf[i] = toupper(buf[i]); // to uppercase
    }
    else
    {
      // Invalid input
      return false;
    }
  }
  return true;
}
#define serialBuflen 64 // Length of serial inputbuffer

// Handle user input in Serial Monitor
void serialHandler(void)
{
  char buf[serialBuflen];
  int len;
  char cmd;

  menuPrint();

  while (1)
  {
    DEBUG_PRINT(F("Command > "));
    len = inputReadline(buf, serialBuflen);

    if (len != 1)
    {
      DEBUG_PRINTLN(F("Invalid input"));
      continue;
    }

    cmd = buf[0]; // store 1st character in different variable (cmd) since we reuse buf
    switch (cmd)
    {
    case '1':
      DEBUG_PRINT(F("Config writes: "));
      DEBUG_PRINTLN(config.writes);
      DEBUG_PRINT(F("Radio version: "));
      DEBUG_PRINTLN(myLora.sysver());
      DEBUG_PRINT(F("HwEUI        : "));
      DEBUG_PRINTLN(myLora.hweui());
      DEBUG_PRINT(F("DevEUI       : "));
      DEBUG_PRINTLN(config.DevEUI);
      DEBUG_PRINT(F("AppEUI       : "));
      DEBUG_PRINTLN(config.AppEUI);
      DEBUG_PRINT(F("AppKey       : "));
      DEBUG_PRINTLN(config.AppKey);
      break;
    case '2':
      DEBUG_PRINT(F("Enter DevEUI: "));
      len = inputReadline(buf, serialBuflen);
      if (verifyHexstring(buf, 16))
      {
        memcpy(config.DevEUI, buf, sizeof(config.DevEUI));
        configWrite();
        DEBUG_PRINTLN(F("DevEUI saved"));
      }
      else
      {
        DEBUG_PRINTLN(F("ERROR: Invalid key"));
      }
      break;
    case '3':
      DEBUG_PRINT(F("Enter AppEUI: "));
      len = inputReadline(buf, serialBuflen);
      if (verifyHexstring(buf, 16))
      {
        memcpy(config.AppEUI, buf, sizeof(config.AppEUI));
        configWrite();
        DEBUG_PRINTLN(F("AppEUI saved"));
      }
      else
      {
        DEBUG_PRINTLN(F("ERROR: Invalid key"));
      }
      break;
    case '4':
      DEBUG_PRINT(F("Enter AppKey: "));
      len = inputReadline(buf, serialBuflen);
      if (verifyHexstring(buf, 32))
      {
        memcpy(config.AppKey, buf, sizeof(config.AppKey));
        configWrite();
        DEBUG_PRINTLN(F("AppKey saved"));
      }
      else
      {
        DEBUG_PRINTLN(F("ERROR: Invalid key"));
      }
      break;
    case '0':
      DEBUG_PRINTLN(F("Switching to operational mode..."));
      return; // jump out of loop, start software
      break;
    case 'r':
    case 'R':
      DEBUG_PRINT(F("Are you sure? (y/n): "));
      len = inputReadline(buf, serialBuflen);
      if (len == 1 && (buf[0] == 'Y' || buf[0] == 'y'))
      {
        configInit(); // everything to 0, DevEUI reset
      }
      else
      {
        DEBUG_PRINTLN(F("Invalid input... aborting"));
      }
      break;
    default:
      DEBUG_PRINTLN(F("Unknown option"));
      menuPrint();
    }
  }
}

// 4) Flower functioning
void Read_battery()
{ 
  //voltage, *2 because of voltage devider, calibration value set at 1.005797 with multimeter
  //measured voltage at VCC pin 3.27 V (not exactly 3.3 V)
  Battery_voltage = analogRead(Battery_pin) * 2 * (3.27 / 1024) * 1.005797;

  if (Battery_voltage >= WARNING_VOLTAGE)
  {
    batteryWarning = 0; //normal function
  }
  if (Battery_voltage < WARNING_VOLTAGE)
  {
    batteryWarning = 1; //warning: battery low
  }
}

void refill()
{
  myServo.attach(servoControl);
  digitalWrite(PowerSwitch, LOW); //power switch ON
  DEBUG_PRINTLN(F("Refill"));
  myServo.write(SERVO_OPEN, SERVO_SPEED, true);  //position = open, slow speed, true = wait until position reached
  delay(REFILL_TIME);                            // keep servo arm in nectar reservoir
  myServo.write(SERVO_CLOSE, SERVO_SPEED, true); //position = close
  digitalWrite(PowerSwitch, HIGH);               //power switch OFF
  myServo.detach();                              //to save energy
  refillNeed = 0;                                //reset refill need

  startTimeAutoRefill = millis();
}

void automaticRefill()
{
  if (millis() >= startTimeAutoRefill + AUTO_REFILL_TIME)
  {
    DEBUG_PRINT(F("Auto refill...")); //dev.
    refill();
  }
}

void Battery_LEDflash(unsigned long normalInterval, unsigned long warningInterval)
{
  currentBatteryTime = millis();
  if (DEV_MODE == true) //development
  {
    if (batteryWarning == 0)
    {
      if (currentBatteryTime >= LightTimer + normalInterval and currentBatteryTime <= LightTimer + normalInterval + FLASH_TIME)
      {
        digitalWrite(PowerSwitch, LOW); // ON
        FastLED.setBrightness(20);
        leds[0] = CRGB::Green;
        FastLED.show();
      }

      if (currentBatteryTime > LightTimer + normalInterval + FLASH_TIME)
      {
        leds[0] = CRGB::Black;
        FastLED.show();
        digitalWrite(PowerSwitch, HIGH); // OFF
        LightTimer = millis();
      }
    }

    if (batteryWarning == 1) // flashing to show battery low
    {
      if (currentBatteryTime >= LightTimer + warningInterval)
      {
        digitalWrite(PowerSwitch, LOW); // ON
        FastLED.setBrightness(20);
        leds[0] = CRGB::Red;
        FastLED.show();
      }

      if (currentBatteryTime >= LightTimer + warningInterval + FLASH_TIME)
      {
        leds[0] = CRGB::Black;
        FastLED.show();
        digitalWrite(PowerSwitch, HIGH); // OFF
        LightTimer = millis();
      }
    }
  }
}

void savetoRAM(unsigned long startTime, uint8_t visitDuration)
{
  DEBUG_PRINTLN(F(""));
  DEBUG_PRINT(F(" --> SAVING..."));

  if (DEV_MODE == true) //development
  {
    digitalWrite(PowerSwitch, LOW); // ON
    FastLED.setBrightness(50);
    leds[0] = CRGB::SkyBlue;
    FastLED.show();
    delay(50);
  }

  if (visitDuration > 0)
  { //only save when visit duration at least 1 sec.
    visitRec element;
    element.startVisit = startTime; //in sec.
    element.visit = visitDuration;  //in sec.
    if (!visitQueue.push(&element)) // add visit to queue -> 'first in first out' principle
    {
      //if false: queue is full, too many visits
      queueIsFull = 1;
      DEBUG_PRINT(F("ERROR: RESERVED_VISIT_SPACE full"));
    }

    else
    {
      DEBUG_PRINTLN(F(" SAVED"));
      DEBUG_PRINTLN(F(""));
      queueIsFull = 0;
    }
  }

  leds[0] = CRGB::Black;           //development
  FastLED.show();                  //development
  digitalWrite(PowerSwitch, HIGH); // OFF //development
}

void sendData(unsigned long frequency, uint8_t port) // frequency in ms
{
  if (millis() >= sendStartTime + frequency)
  {
    startSending = millis(); //development

    myLora.autobaud(); //wake up Lora module
    delay(15);

    DEBUG_PRINT(F("SEND..."));

    if (DEV_MODE == true) //development
    {
      digitalWrite(PowerSwitch, LOW); // ON
      FastLED.setBrightness(50);
      leds[0] = CRGB::Yellow;
      FastLED.show();
    }

    Read_battery(); //to send battery level to backend application

    //sending during visit
    if (visitState == 1)
    {
      DEBUG_PRINT(F(" --> during visit"));
      visitCounter = round((millis() - startVisitTimer) / PRECISION);
      savetoRAM(startVisit, visitCounter);
      visitState = 0;
      sendDuringVisit = 1;

      if (refillNeed == 1)
      {
        refillGapTimer = millis() + 15000; // reset refill gap timer when sending during refill gap (plus 15 sec. for sending)
      }
    }

    SENDdata(port); //sending, port can be specified

    queueIsFull = 0;     // reset
    sendDuringVisit = 0; //reset

    //DEBUG_PRINT(F("Battery: "));DEBUG_PRINT(Battery_voltage); DEBUG_PRINTLN(F("v"));
    //DEBUG_PRINT(F("Warning: ")); DEBUG_PRINTLN(batteryWarning); DEBUG_PRINTLN(F(""));

    //shut down LED when it was switched on in development mode
    leds[0] = CRGB::Black;           //development
    FastLED.show();                  //development
    digitalWrite(PowerSwitch, HIGH); // OFF //development

    sendStartTime = millis(); //timer for message sending reset

    myLora.sleep(43200000); //save energy in between sending, sleeping 12 hrs or untill waked up

    //show how long microcontroller occupied with sending function
    DEBUG_PRINT(F("send time:")); //development
    DEBUG_PRINTLN((millis() - startSending) / PRECISION); //development
  }
}

// SET-UP
void setup()
{
  //extra energy saving possibile with the right hardware:
    //delay(40000);
    //clock_prescale_set(clock_div_8); // change clock speed to 1 MHz (divide 8 MHz by 8) (also compile sketch with this clock speed)

  // 1 Flower function

  pinMode(LoRaReset, OUTPUT);
  digitalWrite(LoRaReset, LOW); //reset LoRa module at start of flower
  delay(10);
  digitalWrite(LoRaReset, HIGH);

  debugSerial.begin(9600); // link to serial monitor (cable)
  delay(5000);             //give 5sec. time to open Serial Monitor

  pinMode(PowerSwitch, OUTPUT);
  digitalWrite(PowerSwitch, LOW); // power switch ON

  FlowerState = wakeUp; //begin state

  // 2 InfraRed System
  pinMode(IRreceiverPower, OUTPUT);
  pinMode(IRemitterPower, OUTPUT);
  pinMode(IRsensorRead, INPUT);

  obstruction = 0;

  // 3 Battery
  pinMode(Battery_pin, INPUT);
  batteryWarning = 0;

  // 4 LED
  FastLED.addLeds<WS2812, LEDdataPin, RGB>(leds, 1);
  FastLED.setBrightness(40);
  digitalWrite(PowerSwitch, LOW); // ON
  leds[0] = CRGB::Red;
  FastLED.show();

  // 5 LoRaWAN initialisation
  loraSerial.begin(57600); //send data via Lora radio
  delay(3000);
  leds[0] = CRGB::Black;
  FastLED.show();

  sendDuringVisit = 0;
  queueIsFull = 0;
  DEBUG_PRINTLN(F(""));
  DEBUG_PRINTLN(F("Starting Robotic Flower"));
  DEBUG_PRINTLN(F(""));

  delay(500); // Allow some time for the RN2483 to boot and do a factory reset
  //loraSerial.println("sys factoryRESET");
  delay(2000);
  myLora.setFrequencyPlan(TTN_EU);
  myLora.autobaud();
  //DEBUG_PRINTLN(F("Radio module reset"));

  // Read config from EEPROM
  configRead();
  //DEBUG_PRINTLN(F("Read config from EEPROM"));

  // Initialise the hardware
  //DEBUG_PRINTLN(F("Initialise hardware"));

  unsigned long begin = millis();
  while (millis() - begin < 10000)
  { // when not connected to serial monitor, try to join network automatically after 10 sec. with current config
    delay(200);
    if (debugSerial || debugSerial.available())
    {
      serialHandler(); //open menu
      break;
    }
  }

  // Try to join the network
  leds[0] = CRGB::Red;
  FastLED.show();
  DEBUG_PRINT(F("OTAA join DingNet... "));
  while (!myLora.initOTAA(config.AppEUI, config.AppKey, config.DevEUI))
  { //try to make connection, if not successful try again
    DEBUG_PRINTLN(F("FAIL (retry in 10 sec.)"));
    delay(10000);
    DEBUG_PRINT(F("OTAA join DingNet... ")); //over the air activation
    delay(100);
  }
  DEBUG_PRINTLN(F("OK"));
  leds[0] = CRGB::Green;
  FastLED.show();
  delay(2000);
  leds[0] = CRGB::Black;
  FastLED.show();

  // 6 Initialize Servo system

  myServo.attach(servoControl); //open servo for first fill of nectar cup

  // 7 start flower functioning
  leds[0] = CRGB::Green;
  FastLED.show();
  delay(100);
  leds[0].fadeLightBy(255);
  FastLED.show();
  delay(100);
  leds[0] = CRGB::Green;
  FastLED.show();
  delay(100);
  leds[0] = CRGB::Black;
  FastLED.show();

  digitalWrite(PowerSwitch, HIGH); // OFF

  LightTimer = millis(); // start timer for flashing

  Read_battery();

  if (DEV_MODE == false)
  { // shut of USB module to save energy
    USBCON |= (1 << FRZCLK); // Freeze the USB Clock
    PLLCSR &= ~(1 << PLLE);  // Disable the USB Clock (PPL)
    USBCON &= ~(1 << USBE);  // Disable the USB
  }

  SENDdata(2); //first send for set-up
  myLora.sleep(43200000); //save energy in between sending
}

// MAIN LOOP
void loop()
{
  //1. Going-To-Sleep State
  if (FlowerState == goingToSleep)
  {
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("GOING TO SLEEP"));

    visitState = 0;

    digitalWrite(PowerSwitch, HIGH); //power switch OFF
    digitalWrite(IRemitterPower, LOW);
    digitalWrite(IRreceiverPower, LOW);

    while (!visitQueue.isEmpty())
    { //send data untill queue is empty
      sendData(GOING_TO_SLEEP_SEND_FREQUENCY, 1); // send data over LoRaWAN, port 1
    }

    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("SLEEP"));

    FlowerState = sleep;

  } // goingToSleep state end

  //2. Sleep State
  if (FlowerState == sleep)
  {   
    // deep sleep mode

    while (sleepCounter < SLEEP_TIME)
    { // deep sleep for some time during night to save battery
      sleepCounter += 9;

      if (DEV_MODE == true && DEV_SLEEP_MODE == true)
      { //keep serial monitor open for development
        delay(8000);
        DEBUG_PRINT(F("Deep Sleep:"));
        DEBUG_PRINT(sleepCounter);
        DEBUG_PRINT(F("/"));
        DEBUG_PRINTLN(SLEEP_TIME);
      }

      else
      { //.powerDown shuts down serial monitor (energy saving)
        if (DEV_MODE == true)
        { // flash led to signal sleep state
          digitalWrite(PowerSwitch, LOW); // ON
          FastLED.setBrightness(50);
          leds[0] = CRGB::BlueViolet;
          FastLED.show();
          delay(1000);
          digitalWrite(PowerSwitch, HIGH); // OFF
        }

        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); //max. sleep time 8 sec. (+- 10%)
      }
    }

    //interupt sleep to send message now and then to be able to receive message with LoRaWAN

    if (DEV_MODE == true && DEV_SLEEP_MODE == true)
    { //keep serial monitor open for development
      sendData(SLEEP_SEND_FREQUENCY_DEV, 2); // send data over LoRaWAN, port 2
      delay(8000);

      //digitalWrite(PowerSwitch, LOW); // ON
      //FastLED.setBrightness(100);
      //leds[0] = CRGB::Yellow;
      //FastLED.show();
      //delay(1000);
      //digitalWrite(PowerSwitch, HIGH); // OFF
    }

    else
    { 
      //while (wakeUpSendCounter < SLEEP_SEND_FREQUENCY)
      //{
        if (DEV_MODE == true)
        {
        digitalWrite(PowerSwitch, LOW); // ON
        FastLED.setBrightness(100);
        leds[0] = CRGB::Yellow;
        FastLED.show();
        delay(1000);
        digitalWrite(PowerSwitch, HIGH); //OFF
        }

        //wakeUpSendCounter += 9;
        //LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); //max. sleep time 8 sec. (+- 10%)
      //}

      sendData(SLEEP_SEND_FREQUENCY,2); //send to be able to receive message for waking up, port 2
      
      //wakeUpSendCounter = 0; // reset counter
    }

    if (timeToSleep == false) //Go to wake-up when message received through LoRaWAN tells it's time to do so
    {
      FlowerState = wakeUp;
    }

  } // sleep state end

  //3. Wake-Up State
  if (FlowerState == wakeUp)
  {
    sleepCounter = 0; // reset

    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("WAKE UP"));

    refillGapTimer = millis(); //first refill in work state when timer ends
    refillNeed = 1;

    startTimeAutoRefill = millis(); //start automatic refill timer
    sendStartTime = millis();       //start timer for message sending
    LightTimer = millis();          // start timer for flashing

    //first IR-sensor read - to start
    digitalWrite(IRreceiverPower, HIGH);
    delay(1);
    digitalWrite(IRemitterPower, HIGH);
    delay(20);
    previousSensorValue = analogRead(IRsensorRead);
    
    digitalWrite(IRreceiverPower, LOW);
    digitalWrite(IRemitterPower, LOW);

    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("WORK"));

    FlowerState = work;
  } // wake-up state end

  //4. Work State
  if (FlowerState == work)
  {

    if (timeToSleep == true)
    { //Go to sleep when message received through LoRaWAN tells it's time to do so
      FlowerState = goingToSleep;
    }

    Battery_LEDflash(NORMAL_FLASH_INTERVAL, WARNING_FLASH_INTERVAL); // flash led to show functioning (development)
    sendData(WORK_SEND_FREQUENCY, 1);   // send data over LoRaWAN (e.g. every 10min.), port 1
    automaticRefill();                  // automatic servo movement to prevent drying out of nectar

    // detection system timer reset
    if (millis() > startTimeCycle + CYCLE_TIME)
    {
      startTimeCycle = millis();
    }

    // refill interval after visit
    if (millis() - refillGapTimer >= REFILL_GAP and millis() - refillGapTimer < REFILL_GAP + 10 and refillNeed == 1)
    {
      refill();
    }

    // detection on
    if (millis() <= startTimeCycle + ON_TIME)
    {
      //IR-sensor system turned on once every TimeCycle
      digitalWrite(IRreceiverPower, HIGH);
      delay(1);
      digitalWrite(IRemitterPower, HIGH);
      delay(1);
      sensorValue = analogRead(IRsensorRead); //read voltage of IR sensor
      delay(1);
      digitalWrite(IRemitterPower, LOW);
      digitalWrite(IRreceiverPower, LOW);

      if (SHOW_IR_VALUE == true) //development
      {
        DEBUG_PRINT(F("IR:")); DEBUG_PRINTLN(sensorValue);
        //DEBUG_PRINT(F("Previous value: ")); DEBUG_PRINTLN(previousSensorValue);
      }

      // visit
      if (sensorValue <= previousSensorValue - SENSOR_SENSITIVITY)
      {
        if (DEV_MODE == true)
        {
          digitalWrite(PowerSwitch, LOW); // ON
          FastLED.setBrightness(50);
          leds[0] = CRGB::Red;
          FastLED.show();
        }

        if (visitState == 0)
        { //new visit starting
          startVisit = round(millis()) / PRECISION;
          startVisitTimer = millis();
        }

        // max duration of 250 sec; because of rounding, visits less then 0,5 sec. are not registered (=0)
        visitCounter = round((millis() - startVisitTimer) / PRECISION); 

        visitState = 1;
        previousVisitState = 1;

        // max visit time reached
        if (visitCounter >= MAX_VISIT_DURATION) //maxCounter not over 255 (to be able to send visitDuration in 1 byte)
        { // visitor might be sleeping (or dead) in flower

          DEBUG_PRINTLN(F("Max.Visit")); //dev.
          //DEBUG_PRINT(visitCounter); DEBUG_PRINTLN(F("sec."));

          savetoRAM(startVisit, visitCounter); //save visit start time & visit duration

          visitState = 0;
          previousVisitState = 0;

          obstrCounter++;
          if (obstrCounter >= OBSTR_TRESH)
          {
            obstruction = 1;
          }
        }

        startTimeAutoRefill = millis(); //avoid automatic refill during visit, restart timer
        
        if (refillNeed == 0)
        {// only reset refill gap timer if this is the first visit since last refilling
          refillGapTimer = millis();      //refill after visit to renew 'nectar'
        }


        LightTimer = millis();          //development, avoid LED flashing during visit
      }

      // no visit
      else
      {
        visitState = 0;
        previousSensorValue = sensorValue;
        obstruction = 0; //reset

        //shut down LED when it was switched on in development mode
        leds[0] = CRGB::Black;
        FastLED.show();
        digitalWrite(PowerSwitch, HIGH); // OFF
      }

      // end of visit
      if (previousVisitState != visitState)
      {
        DEBUG_PRINT(F("Visit: "));
        DEBUG_PRINT(visitCounter);
        //DEBUG_PRINTLN(F(" sec."));

        savetoRAM(startVisit, visitCounter); //save start time & visit duration to queue

        if (visitCounter > VISIT_TRESHOLD)
        {
          DEBUG_PRINTLN(F("Refill gap")); DEBUG_PRINTLN(F(""));

          refillNeed = 1;       //need to refill after visit
        }
        visitCounter = 0;
      }

      previousVisitState = visitState;

      delay(100); // check if cycle has passed every 10 times per second in stead of constant, save energy
      
    }
  } // work state end
}
