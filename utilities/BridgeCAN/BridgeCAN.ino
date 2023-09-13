#include <FlexCAN_T4.h>
#include "CAN-Logger-3-Teensy36-Mounted.h"

FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> Can0;
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;

elapsedMillis display_timer;
CAN_message_t msg;

// Define CAN TXRX Transmission Silent pins
// See the CAN Logger 3 Schematic for the source pins


boolean GREEN_LED_state;
boolean RED_LED_state;
boolean YELLOW_LED_state;
boolean BLUE_LED_state;

void setup(void) {

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
  BLUE_LED_state = HIGH;
  digitalWrite(GREEN_LED,GREEN_LED_state);
  digitalWrite(YELLOW_LED,YELLOW_LED_state);
  digitalWrite(RED_LED,RED_LED_state);
//  for (int i=0;i<8;i++){
//    BLUE_LED_state = !BLUE_LED_state;
//    delay(200);
    digitalWrite(BLUE_LED,BLUE_LED_state);
//  }
//  Serial.begin(115200);
  Can0.begin();
  Can0.setBaudRate(500000);
  Can1.begin();
  Can1.setBaudRate(500000);
  msg.flags.extended = 1;
  msg.len = 8;
}

void loop() {
  
  if (Can0.read(msg)){
    RED_LED_state = !RED_LED_state;
    digitalWrite(RED_LED,RED_LED_state);   
    Can1.write(msg);
      // A display timer keeps the USB Serial stack from getting too backed up.  
      Serial.printf("CAN0 -> CAN1 %08X %X %02X %02X %02X %02X %02X %02X %02X %02X\n", msg.id, msg.len, msg.buf[0], msg.buf[1], msg.buf[2], msg.buf[3], msg.buf[4], msg.buf[5], msg.buf[6], msg.buf[7]); 
//    if (display_timer > 5 ){
//      display_timer = 0;
//    }
  }

  if (Can1.read(msg)){
    YELLOW_LED_state = !YELLOW_LED_state;
    digitalWrite(YELLOW_LED,YELLOW_LED_state);
    Serial.printf("CAN1 -> CAN0 %08X %X %02X %02X %02X %02X %02X %02X %02X %02X\n", msg.id, msg.len, msg.buf[0], msg.buf[1], msg.buf[2], msg.buf[3], msg.buf[4], msg.buf[5], msg.buf[6], msg.buf[7]); 
    Can0.write(msg);
  }
//
//   
//    if (display_timer > 5 ){
//      display_timer = 0;
//      Serial.printf("CAN1 -> CAN0 %08X: %02X %02X %02X %02X %02X %02X %02X %02X\n", msg.id, msg.buf[0], msg.buf[1], msg.buf[2], msg.buf[3], msg.buf[4], msg.buf[5], msg.buf[6], msg.buf[7]); 
//    }
//  }
}
