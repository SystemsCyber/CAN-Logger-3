#include <FlexCAN_T4.h>
FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> Can0;
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;

#define SILENT_0   42
#define SILENT_1   41
#define SILENT_2   40
#define BUTTON_PIN 28 //version 3b
#define POWER_PIN  21

//Setup LEDs
#define GREEN_LED 6
#define RED_LED 14
#define YELLOW_LED 5
#define BLUE_LED 39


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
  digitalWrite(BLUE_LED,BLUE_LED_state);
    
  Can0.begin();
  Can0.setBaudRate(500000);
  Can0.setMaxMB(16);
  Can0.enableFIFO();
  Can0.enableFIFOInterrupt();
  Can0.onReceive(sendCAN1);
  //Can0.mailboxStatus();

  Can1.begin();
  Can1.setBaudRate(500000);
  Can1.setMaxMB(16);
  Can1.enableFIFO();
  Can1.enableFIFOInterrupt();
  Can1.onReceive(sendCAN0);
  //Can1.mailboxStatus();
}

void sendCAN0(const CAN_message_t &msg){
  Can0.write(msg);
  YELLOW_LED_state = !YELLOW_LED_state;
  digitalWrite(YELLOW_LED,YELLOW_LED_state);  
}
void sendCAN1(const CAN_message_t &msg){
  Can1.write(msg);
  RED_LED_state = !RED_LED_state;
  digitalWrite(RED_LED,RED_LED_state); 
}

void loop() {
  Can0.events();
  Can1.events();
}
