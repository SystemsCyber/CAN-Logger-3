#include <FlexCAN_T4.h>
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;

elapsedMillis display_timer;
elapsedMicros micro_counter;
uint32_t TX_counter;
CAN_message_t msg;

// Define CAN TXRX Transmission Silent pins
// See the CAN Logger 2 Schematic for the source pins
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

boolean GREEN_LED_state;
boolean RED_LED_state;
boolean YELLOW_LED_state;
boolean BLUE_LED_state;

uint32_t mask[4]={0xFF, 0xFF00, 0xFF0000, 0xFF000000};
uint8_t shift[4]={0, 8, 16, 32};

void setup(void) {
  Can0.begin();
  Can0.setBaudRate(250000);
  msg.flags.extended = 1;
  msg.len = 4;
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
}

void loop() {
  msg.id = TX_counter++;
  if (msg.id >= 0x1FFFFFFF) TX_counter = 0;
  uint32_t timer_value = micro_counter;
  //memcpy(&msg.buf[0], &timer_value, 4);
  for (int i = 0; i < 4; i++){
    msg.buf[i] = (timer_value & mask[i]) >> shift[i];
  }
  while(!Can0.write(msg));
  if (display_timer>100){
    display_timer = 0;
    GREEN_LED_state = !GREEN_LED_state;
    digitalWrite(GREEN_LED,GREEN_LED_state);
    //Serial.printf("%08X: %02X %02X %02X %02X\n", msg.id, msg.buf[0], msg.buf[1], msg.buf[2], msg.buf[3]); 
  }
}
