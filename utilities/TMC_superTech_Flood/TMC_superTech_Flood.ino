#include <FlexCAN_T4.h>
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;
FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> Can0;
#include <TimeLib.h> // be able to keep realtime.

#include <OneButton.h> // Handle the buttons

const uint8_t NUM_STATES = 4;
uint8_t state = 1;

// Keep track of the CAN Channel (0, 1)
uint8_t current_channel;

#define SILENT_0   42
#define SILENT_1   41
#define SILENT_2   40

#define BUTTON_PIN 28 //version 3b
#define POWER_PIN  21

// Use the button for multiple inputs: click, doubleclick, and long click.
OneButton button(BUTTON_PIN, true);
OneButton right_button(BUTTON_PIN, true);

#define FAST_BLINK_TIME 50 //milliseconds
#define SLOW_BLINK_TIME 500 //milliseconds

//Setup LEDs
#define GREEN_LED 6
#define RED_LED 14
#define YELLOW_LED 5
#define BLUE_LED 39

boolean GREEN_LED_state;
boolean RED_LED_state;
boolean YELLOW_LED_state;
boolean BLUE_LED_state;

elapsedMillis GREEN_LED_Timer;
elapsedMillis RED_LED_Timer;
elapsedMillis YELLOW_LED_Timer;
elapsedMillis BLUE_LED_Timer;

elapsedMicros microsecondsPerSecond;

//Create a counter and timer to keep track of received message traffic
#define RX_TIME_OUT 2000 //milliseconds
elapsedMillis RXTimer1;
elapsedMillis RXTimer0;
uint32_t RXCount1 = 0;
uint32_t RXCount0 = 0;
elapsedMillis lastCAN1messageTimer;
elapsedMillis lastCan0messageTimer;
uint32_t ErrorCount1 = 0;
uint32_t ErrorCount2 = 0;

#define AUTOBAUD_TIMEOUT 400 //millis
elapsedMillis routine_time;

// All CAN messages are received in the FlexCAN structure
CAN_message_t rxmsg, txmsg;

#define NUM_BAUD_RATES 6
//first and last are the defaults
uint32_t baud_rates[NUM_BAUD_RATES] = {500000,250000,125000,666666,1000000,250000};

uint32_t Can1_bitrate;
uint32_t Can0_bitrate;


void doState0(){
  GREEN_LED_state = LOW;
  BLUE_LED_state = LOW;
}

void doState1(){
// Flood bus with zeros
  GREEN_LED_state = HIGH;
  BLUE_LED_state = HIGH;
  memset(&txmsg.buf[0],0,8);
  txmsg.id = 0;
  Can0.write(txmsg);
  Can1.write(txmsg);
}

void doState2(){
// Flood bus with zeros
  GREEN_LED_state = HIGH;
  BLUE_LED_state = LOW;
}

void doState3(){
// Flood bus with zeros
  GREEN_LED_state = LOW;
  BLUE_LED_state = HIGH;
}
  
/*
 * Reset the microsecond counter and return the realtime clock
 * This function requires the sync interval to be exactly 1 second.
 */
time_t getTeensy3Time(){
  microsecondsPerSecond = 0;
  return millis()/1000;
}

void myClickFunction(){
    GREEN_LED_state = HIGH;
    //digitalWrite(GREEN_LED, GREEN_LED_state);
    state+=1;
    //Serial.print(state);
    if (state >= NUM_STATES){
       state = 0;
    }
}


void setup() {
  // put your setup code here, to run once:
  // Setup timing services
  setSyncProvider(getTeensy3Time);
  setSyncInterval(1);

  
  //setup pin modes for the transeivers
  pinMode(SILENT_0,OUTPUT);
  pinMode(SILENT_1,OUTPUT);
  pinMode(SILENT_2,OUTPUT);
  
  //Set High to prevent transmission for the CAN TXRX
  digitalWrite(SILENT_0,LOW); 
  digitalWrite(SILENT_1,LOW);
  digitalWrite(SILENT_2,LOW);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  GREEN_LED_state = HIGH;
  YELLOW_LED_state = HIGH;
  RED_LED_state = HIGH;
  BLUE_LED_state = LOW;
  digitalWrite(GREEN_LED,GREEN_LED_state);
  digitalWrite(YELLOW_LED,YELLOW_LED_state);
  digitalWrite(RED_LED,RED_LED_state);
  digitalWrite(BLUE_LED,BLUE_LED_state);
 
  // Setup the button with an internal pull-up
  pinMode(BUTTON_PIN,INPUT_PULLUP);
    // Setup button functions 
  button.attachClick(myClickFunction);
  // button.attachDoubleClick(myDoubleClickFunction);
  button.attachLongPressStart(myLongPressStartFunction);
  button.attachLongPressStop(myLongPressStopFunction);

  //Initialize the CAN channels with autobaud.
  Can1.begin();
  Can1_bitrate = baud_rates[0];
  Can1.setBaudRate(Can1_bitrate);
  Can1.setMaxMB(16);
  Can1.enableFIFO();
  Can1.enableFIFOInterrupt();
  Can1.onReceive(processCan1);
  Can1.mailboxStatus();

  Can0.begin();
  Can0_bitrate = baud_rates[0];
  Can0.setBaudRate(Can0_bitrate);
  Can0.setMaxMB(16);
  Can0.enableFIFO();
  Can0.enableFIFOInterrupt();
  Can0.onReceive(processCan0);
  Can0.mailboxStatus();
  
  // These should really be built into the library
  autoBaud0(); 
  autoBaud1();

  //Flex CAN defaults
  txmsg.flags.extended = 1;
  txmsg.len = 8;  
}


void myLongPressStartFunction(){
  BLUE_LED_state = HIGH;
  //digitalWrite(BLUE_LED, BLUE_LED_state);
}

void myLongPressStopFunction(){
  BLUE_LED_state = LOW;
  //digitalWrite(BLUE_LED, BLUE_LED_state);
}

void autoBaud0(){
 // Setup a timer for an overall baudrate detection timeout
  CAN_error_t error_message;  
  for (uint8_t i = 0; i<NUM_BAUD_RATES;  i++ ){
    Can0_bitrate = baud_rates[i];
    Can0.setBaudRate(Can0_bitrate);
    Serial.print ("Looking for Messages on Can0 at ");
    Serial.print (Can0_bitrate);
    Serial.println (" bps.");

    routine_time=0;
    while (routine_time < AUTOBAUD_TIMEOUT)
    { 
      // If something has been recieved, then we have the correct bitrate.
      if (RXCount0 > 0) {
        Serial.print("Success! Found message on CAN0. Using ");
        Serial.print(Can0_bitrate);
        Serial.println(" bps.");
        return;
      }

      //If there is an error, then we have the wrong bitrate, and we should change.
      if (Can0.error(error_message,true)){
        routine_time = AUTOBAUD_TIMEOUT;
      }
      // if we have nothing, then keep looping.
  
    }
    // After timeout, try a new one.

  }
  Serial.print("No Messages Found on CAN0. Using ");
  Serial.print(Can0_bitrate);
  Serial.println(" bps.");
}

void autoBaud1(){
 // Setup a timer for an overall baudrate detection timeout
  CAN_error_t error_message;
  for (uint8_t i = 0; i<NUM_BAUD_RATES;  i++ ){
    Can1_bitrate = baud_rates[i];
    Can1.setBaudRate(Can1_bitrate);
    Serial.print ("Looking for Messages on Can1 at ");
    Serial.print (Can1_bitrate);
    Serial.println (" bps.");

    routine_time = 0;
    while (routine_time < AUTOBAUD_TIMEOUT)
    { 
      // If something has been recieved, then we have the correct bitrate.
      if (RXCount1 > 0) {
        Serial.print("Success! Found message on CAN1. Using ");
        Serial.print(Can1_bitrate);
        Serial.println(" bps.");
        return;
      }
      //If there is an error, then we have the wrong bitrate, and we should change.
      if (Can1.error(error_message,true)){
        routine_time = AUTOBAUD_TIMEOUT;
      }

    }
    // if we have nothing, then keep looping.

  }
  Serial.print("No Messages Found on CAN1. Using ");
  Serial.print(Can1_bitrate);
  Serial.println(" bps.");
}

void processCan0(const CAN_message_t &msg) {
  RXCount0++;
  RXTimer0 = 0;
  RED_LED_state = !RED_LED_state;
  //digitalWrite(RED_LED,RED_LED_state);
  printCAN(msg,0);
}

void processCan1(const CAN_message_t &msg) {
  RXCount1++;
  RXTimer1 = 0;
  YELLOW_LED_state = !YELLOW_LED_state;
  //digitalWrite(YELLOW_LED,YELLOW_LED_state);
  printCAN(msg,1);
}

void printCAN(const CAN_message_t &msg,uint8_t channel){
  char channel_string[5] = {'c','a','n',' ',0};
  double timestamp = double(now()) + double(microsecondsPerSecond)/1000000.0;
  if (channel == 0) channel_string[3] = '0';
  else if (channel == 1) channel_string[3] = '1';
  Serial.printf("(%0.6f) %s %08X [%d]",timestamp,channel_string,msg.id,msg.len); 
  for ( uint8_t i = 0; i < msg.len; i++ ) {
    Serial.printf(" %02X",msg.buf[i]); 
  } 
  Serial.println();
}

void loop() {
  button.tick();
  Can0.events();
  Can1.events();

  if      (state == 0) doState0();
  else if (state == 1) doState1();
  else if (state == 2) doState2();
  else if (state == 3) doState3();
  
  //Turn the LEDs on if there's no traffic
  if (RXTimer0 > 500) RED_LED_state = HIGH;
  if (RXTimer1 > 500) YELLOW_LED_state = HIGH;
  
  digitalWrite(GREEN_LED,GREEN_LED_state);
  digitalWrite(YELLOW_LED,YELLOW_LED_state);
  digitalWrite(RED_LED,RED_LED_state);
  digitalWrite(BLUE_LED,BLUE_LED_state);
}


