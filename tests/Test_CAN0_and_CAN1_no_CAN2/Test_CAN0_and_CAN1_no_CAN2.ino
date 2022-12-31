/*
 * This is a test sketch for the NMFTA/NSF CAN Logger 3
 * It uses a Teensy 3.6, two CAN Transceivers and 3 LEDs.
 * The test will light the LEDs upon transmit and toggle the LED on receive.
 * Tests can be conducted with a live network, or the CAN connections can be bridged
 * to so a device can be tested by itself.
 * 
 * The serial monitor may look something like this (with bridged CAN busses):
 * 
CAN1 Message Sent: 2965
0         2964    264216579 00000101 1 8 0F BF 9D C1 00 00 0B 94
CAN0 Message Sent: 1771
1         1770    264266572 00000100 1 8 0F C0 61 11 00 00 06 EA
CAN1 Message Sent: 2966
0         2965    264305572 00000101 1 8 0F C0 F9 69 00 00 0B 95
CAN1 Message Sent: 2967
0         2966    264394564 00000101 1 8 0F C2 55 10 00 00 0B 96
CAN0 Message Sent: 1772
1         1771    264415572 00000100 1 8 0F C2 A7 18 00 00 06 EB
CAN1 Message Sent: 2968
0         2967    264483568 00000101 1 8 0F C3 B0 B9 00 00 0B 97

 */
#include <FlexCAN.h>

//Define message from FlexCAN library
static CAN_message_t txmsg0;
static CAN_message_t txmsg1;
static CAN_message_t rxmsg0;
static CAN_message_t rxmsg1;

//Set up timing variables (Use prime numbers so they don't overlap)
#define TXPeriod0 149
elapsedMillis TXTimer0;

#define TXPeriod1 89
elapsedMillis TXTimer1;


//Create a counter to keep track of message traffic
uint32_t TXCount0 = 0;
uint32_t TXCount1 = 0;
uint32_t RXCount0 = 0;
uint32_t RXCount1 = 0;

//Define LED
#define GREEN_LED_PIN 6
#define RED_LED_PIN 14
#define YELLOW_LED_PIN 5

boolean GREEN_LED_state; 
boolean RED_LED_state;
boolean YELLOW_LED_state;

//Define CAN TXRX Transmission Silent pins
#define SILENT_0 42
#define SILENT_1 41
#define SILENT_2 40

//Define default baudrate
#define BAUDRATE250K 250000
#define BAUDRATE500K 250000


//A generic CAN Frame print function for the Serial terminal
void printFrame(CAN_message_t rxmsg, uint8_t channel, uint32_t RXCount)
{
  char CANdataDisplay[50];
  sprintf(CANdataDisplay, "%d %12lu %12lu %08X %d %d", channel, RXCount, micros(), rxmsg.id, rxmsg.ext, rxmsg.len);
  Serial.print(CANdataDisplay);
  for (uint8_t i = 0; i < rxmsg.len; i++) {
    char CANBytes[4];
    sprintf(CANBytes, " %02X", rxmsg.buf[i]);
    Serial.print(CANBytes);
  }
  Serial.println();
}


void setup() {
  // put your setup code here, to run once:
  //Set baudrate
  Can1.begin(BAUDRATE250K);
  Can0.begin(BAUDRATE250K);
  
  //Set message extension, ID, and length
  txmsg0.ext = 1;
  txmsg0.id=0x100;
  txmsg0.len=8;
  
  txmsg1.ext = 1;
  txmsg1.id=0x101;
  txmsg1.len=8;
  
  // Enable transmission for the CAN TXRX
  pinMode(SILENT_0,OUTPUT);
  pinMode(SILENT_1,OUTPUT);
  pinMode(SILENT_2,OUTPUT);
  digitalWrite(SILENT_0,LOW);
  digitalWrite(SILENT_1,LOW);
  digitalWrite(SILENT_2,LOW);
  
  pinMode(GREEN_LED_PIN,OUTPUT);
  pinMode(RED_LED_PIN,OUTPUT);
  pinMode(YELLOW_LED_PIN,OUTPUT);
  
  //The default filters exclude the extended IDs, so we have to set up CAN filters to allow those to pass.
  CAN_filter_t allPassFilter;
  allPassFilter.ext=1;
  for (uint8_t filterNum = 0; filterNum < 8;filterNum++){ //only use half the available filters for the extended IDs
   Can0.setFilter(allPassFilter,filterNum); 
   Can1.setFilter(allPassFilter,filterNum); 
  }
  Can0.write(txmsg0);
  txmsg0.id = 0x18FF30D8;
  Can0.write(txmsg0);
  txmsg0.id = 0x18FF30D9;
  Can0.write(txmsg0);

}


void loop() {
  // put your main code here, to run repeatedly:

  if (Can0.available()) {
    Can0.read(rxmsg0);
    printFrame(rxmsg0,0,RXCount0++);
    //Toggle the LED
    GREEN_LED_state = !GREEN_LED_state;
    digitalWrite(GREEN_LED_PIN,GREEN_LED_state);
  }
  if (Can1.available()) {
    Can1.read(rxmsg1);
    printFrame(rxmsg1,1,RXCount1++);
    //Toggle the LED
    GREEN_LED_state = !GREEN_LED_state;
    digitalWrite(GREEN_LED_PIN,GREEN_LED_state);
  }
  
  if (TXTimer0 >= TXPeriod0){
	  TXTimer0 = 0;//Reset Timer
    txmsg0.id = 0x0CF00427;
	  //Convert the 32-bit timestamp into 4 bytes with the most significant byte (MSB) first (Big endian).
    uint32_t sysMicros = micros();
    txmsg0.buf[0] = (sysMicros & 0xFF000000) >> 24;
    txmsg0.buf[1] = (sysMicros & 0x00FF0000) >> 16;
    txmsg0.buf[2] = (sysMicros & 0x0000FF00) >>  8;
    txmsg0.buf[3] = (sysMicros & 0x000000FF);

    //Convert the 32-bit transmit counter into 4 bytes with the most significant byte (MSB) first (Big endian). 
    txmsg0.buf[4] = (TXCount0 & 0xFF000000) >> 24;
    txmsg0.buf[5] = (TXCount0 & 0x00FF0000) >> 16;
    txmsg0.buf[6] = (TXCount0 & 0x0000FF00) >>  8;
    txmsg0.buf[7] = (TXCount0 & 0x000000FF);

    //Write the message on CAN channel 0
  	txmsg0.id = 0x0CF00427;
    
    TXCount0++;
    txmsg0.id = 0x0CFE6C27;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3227;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3327;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3427;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3527;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3627;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3727;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3827;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3927;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3A27;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3B27;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3C27;
    Can0.write(txmsg0);
    txmsg0.id = 0x18FF3D27;
    Can0.write(txmsg0);

    
    //Toggle the LED
    RED_LED_state = !RED_LED_state;
    digitalWrite(RED_LED_PIN,RED_LED_state);
    Serial.print("CAN0 Message Sent: ");
    Serial.println(TXCount0);
  }

  // Repeat for CAN1
  if (TXTimer1 >= TXPeriod1){
    TXTimer1 = 0;//Reset Timer
  
    //Convert the 32-bit timestamp into 4 bytes with the most significant byte (MSB) first (Big endian).
    uint32_t sysMicros = micros();
    txmsg1.buf[0] = (sysMicros & 0xFF000000) >> 24;
    txmsg1.buf[1] = (sysMicros & 0x00FF0000) >> 16;
    txmsg1.buf[2] = (sysMicros & 0x0000FF00) >>  8;
    txmsg1.buf[3] = (sysMicros & 0x000000FF);

    //Convert the 32-bit transmit counter into 4 bytes with the most significant byte (MSB) first (Big endian). 
    txmsg1.buf[4] = (TXCount1 & 0xFF000000) >> 24;
    txmsg1.buf[5] = (TXCount1 & 0x00FF0000) >> 16;
    txmsg1.buf[6] = (TXCount1 & 0x0000FF00) >>  8;
    txmsg1.buf[7] = (TXCount1 & 0x000000FF);

    //Write the message on CAN channel 1
    Can1.write(txmsg1);
    TXCount1++;
    //Toggle the LED
    YELLOW_LED_state = !YELLOW_LED_state;
    digitalWrite(YELLOW_LED_PIN,YELLOW_LED_state);
    Serial.print("CAN1 Message Sent: ");
    Serial.println(TXCount1);
  }
}
