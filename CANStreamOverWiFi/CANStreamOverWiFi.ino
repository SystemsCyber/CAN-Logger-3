/*
CAN Stream Over Wifi
The following line
https://github.com/arduino-libraries/WiFi101/blob/master/src/bus_wrapper/source/nm_bus_wrapper_samd21.cpp#L54
should end with SPI1

 */

#include <SPI.h>
#include "WiFi101_SPI1.h"
#include <WiFiUDP.h>
#include "arduino_secrets.h" 
#include <TimeLib.h>
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;
FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> Can0;

#define SILENT_0   42
#define SILENT_1   41
#define SILENT_2   40

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

// There are 2 data buffers that get switched. We need to know which one
// we are using and where we are in the buffer.
#define CAN_FRAME_SIZE 25
#define MAX_MESSAGES 19
#define BUFFER_POSITION_LIMIT (CAN_FRAME_SIZE * MAX_MESSAGES)
#define BUFFER_SIZE 512
char data_buffer[BUFFER_SIZE];
uint16_t current_position;
uint32_t buffer_counter;

// All CAN messages are received in the FlexCAN structure
CAN_message_t rxmsg, txmsg;

#define NUM_BAUD_RATES 6
//first and last are the defaults
uint32_t baud_rates[NUM_BAUD_RATES] = {500000,250000,125000,666666,1000000,250000};

uint32_t Can1_bitrate;
uint32_t Can0_bitrate;

IPAddress ip(192, 168, 30, 2);
IPAddress broadcastIp(192,168,30,255);
IPAddress netmask(255, 255, 255, 0);
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;   // your network password (use for WPA, or use as key for WEP)

unsigned int localPort = 2390;      // local port to listen on
unsigned int destPort = 2390;      // local port to listen on

char packetBuffer[1500]; //buffer to hold incoming packet
char ReplyBuffer[1024] = "Acknowledged";       // a string to send back



//Define the pins for WiFi chip
#define WiFi_EN 24
#define WiFi_RST 25
#define WiFi_CS 31
#define WiFi_IRQ 23
#define WAKE 36

#define WIFI_MOSI_PIN          0
#define WIFI_MISO_PIN          1
#define WIFI_SCLK_PIN        32
#define WIFI_CS_PIN        31

int led =  6;
int status = WL_IDLE_STATUS;

WiFiUDP UDP;

bool first_buffer_sent;

bool sendMessage = false;
CAN_message_t messageToSend;

// Keep track of the CAN Channel (0, 1)
uint8_t current_channel;


char prefix[5];


/*
 * The load_buffer() function maps the received CAN message into
 * the positions in the buffer to store. Each buffer is 512 bytes.
 * 20 CAN messages can fit in the buffer since they are 25 bytes long.
 * Note: an additional channel byte was added when comparing to the 
 * NMFTA CAN Logger 1 files. 
 * 
 * Use this function to understand how the the binary file format was created.
 * When reading the binary, be sure to read it in 512 byte chunks.
 */
void load_buffer(){
  // //SHA256 last buffer
  // if (first_buffer_sent){
  //   micro_timer=0;
  //   if ((current_position - 4) % 50 == 0){ // use 50 because it represents 2 messages.
  //     if (hash_counter < 8){
  //       for (int z = 0; z < SHA_UPDATE_SIZE; z++){
  //         hash_ciphertext[z]=cipher_text[hash_counter*SHA_UPDATE_SIZE + z];
  //       }
  //       sha256Instance->update(hash_ciphertext,SHA_UPDATE_SIZE);
  //       hash_counter++;
  //     }
  //   }
  // }
//Toggle the LED
  GREEN_LED_state = !GREEN_LED_state;
  digitalWrite(GREEN_LED, GREEN_LED_state);
    
  data_buffer[current_position] = current_channel;
  current_position += 1;
  
  time_t timeStamp = now();
  memcpy(&data_buffer[current_position], &timeStamp, 4);
  current_position += 4;
  
  //memcpy(&data_buffer[current_position], &rxmsg.micros, 4);
  current_position += 4;

  memcpy(&data_buffer[current_position], &rxmsg.id, 4);
  current_position += 4;

  // Store the message length as the most significant byte and use the 
  // lower 24 bits to store the microsecond counter for each second.
  uint32_t DLC = (rxmsg.len << 24) | (0x00FFFFFF & uint32_t(microsecondsPerSecond));
  memcpy(&data_buffer[current_position], &DLC, 4);
  current_position += 4;

  memcpy(&data_buffer[current_position], &rxmsg.buf, 8); 
  current_position += 8;

  // if ((rxmsg.id & CAN_ERR_FLAG) == CAN_ERR_FLAG){
  //   RED_LED_state = !RED_LED_state;
  //   digitalWrite(RED_LED, RED_LED_state);  
  // }
  
  // Check the current position and see if the buffer needs to be written 
  // to the SD card.
  check_buffer();
}

void check_buffer(){
  //Check to see if there is anymore room in the buffer
  if (current_position >= BUFFER_POSITION_LIMIT){ //max number of messages
    //Create a file if it is not open yet
    // if (!file_open) {
    //   open_binFile();
    // }  
    uint32_t start_micros = micros();
    // Write the beginning of each line in the 512 byte block
    sprintf(prefix,"CAN2");
    memcpy(&data_buffer[0], &prefix[0], 4);
    current_position = 4;
    
    memcpy(&data_buffer[479], &RXCount0, 4);
    memcpy(&data_buffer[483], &RXCount1, 4);
    //memcpy(&data_buffer[487], &RXCount2, 4);
    
    // data_buffer[491] = Can0.readREC();
    // data_buffer[492] = Can1.readREC();
    // //data_buffer[493] = uint8_t(Can2.errorCountRX());
    // data_buffer[493] = 0;
    
    // data_buffer[494] = Can0.readTEC();
    // data_buffer[495] = Can1.readTEC();
    // //data_buffer[496] = uint8_t(Can2.errorCountTX());
    // data_buffer[496] = 0;
    
    // // Write the filename to each line in the 512 byte block
    // memcpy(&data_buffer[497], &current_file_name, 8);
  
    // uint32_t checksum = CRC32.crc32(data_buffer, CRC32_BUFFER_LOC);
    // memcpy(&data_buffer[CRC32_BUFFER_LOC], &checksum, 4);

    // write_to_sd();
    
    
    // send a reply, to the IP address and port that sent us the packet we received
    UDP.beginPacket(broadcastIp, destPort); //UDP.remotePort());
    //UDP.beginPacket(IPAddress(0, 0, 0, 0), UDP.remotePort());
    UDP.write(data_buffer,512);
    if (UDP.endPacket()) {
      Serial.print(" - Sent to ");
      Serial.print(broadcastIp);
      Serial.print(' ');
      Serial.println(destPort);
      BLUE_LED_state = !BLUE_LED_state;
    }
    else {
      Serial.println(" - Error");
      
    }
    
    
    buffer_counter++;
    // if (buffer_counter >= 1843200) {//When file size reaches 900 MB, start a new one
    //   Serial.print("buffer_counter = ");
    //   Serial.println(buffer_counter);
    //   buffer_counter = 0;
    //   myDoubleClickFunction(); 
    // }
    //Reset the record
    memset(&data_buffer,0xFF,BUFFER_SIZE);
    
    //Toggle LED to show SD card writing
    YELLOW_LED_state = !YELLOW_LED_state;
    digitalWrite(YELLOW_LED,YELLOW_LED_state);
    //Record write times for the previous frame, since the existing frame was just written
    uint32_t elapsed_micros = micros() - start_micros;
    memcpy(&data_buffer[505], &elapsed_micros, 3);
    
    
  }
}
/*
 * Reset the microsecond counter and return the realtime clock
 * This function requires the sync interval to be exactly 1 second.
 */
time_t getTeensy3Time(){
  microsecondsPerSecond = 0;
  return millis()/1000;
}

void setup() {
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

  SPI1.setMOSI(WIFI_MOSI_PIN);
  SPI1.setMISO(WIFI_MISO_PIN);
  SPI1.setSCK(WIFI_SCLK_PIN);
  SPI1.begin();

  pinMode(WAKE,OUTPUT);
  digitalWrite(WAKE,HIGH);

   Serial.println("Access Point Web Server");
  //Initialize WiFi module
  WiFi.setPins(WiFi_CS,WiFi_IRQ,WiFi_RST,WiFi_EN);
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue
    //while (true);
  }

  String fv = WiFi.firmwareVersion();
  Serial.print("Firmware Version: ");
  Serial.println(fv);


  // by default the local IP address of will be 192.168.1.1
  // you can override it with the following:
   WiFi.config(ip);
  // My ip definitions for reference

  // print the network name (SSID);
  Serial.print("Creating access point named: ");
  Serial.println(ssid);

  // Create open network. Change this line if you want to create an WEP network:
  status = WiFi.beginAP(ssid);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // don't continue
    while (true);
  }

  // wait 10 seconds for connection:
  //delay(10000);

  // start the web server on port 80
  UDP.begin(localPort);

  // you're connected now, so print out the status
  printWiFiStatus();

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



void loop() {
  Can0.events();
  Can1.events();
  
  //Turn the LEDs on if there's no traffic
  if (RXTimer0 > 500) RED_LED_state = HIGH;
  if (RXTimer1 > 500) YELLOW_LED_state = HIGH;
  
  digitalWrite(GREEN_LED,GREEN_LED_state);
  digitalWrite(YELLOW_LED,YELLOW_LED_state);
  digitalWrite(RED_LED,RED_LED_state);
  digitalWrite(BLUE_LED,BLUE_LED_state);

  // if there's data available, read a packet
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remoteIp = UDP.remoteIP();
    Serial.print(remoteIp);
    Serial.print(", port ");
    Serial.println(UDP.remotePort());
    // read the packet into packetBufffer
    int len = UDP.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    Serial.println("Contents:");
    Serial.println(packetBuffer);
    // send a reply, to the IP address and port that sent us the packet we received
    UDP.beginPacket(broadcastIp, UDP.remotePort());
    UDP.write(ReplyBuffer);
    UDP.endPacket();
  }
  // if (sendMessage){
  //   sendMessage=false;
  //   load_buffer(rxmsg);
    
  // } 
  
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
  printCAN(msg,0);
}

void processCan1(const CAN_message_t &msg) {
  RXCount1++;
  RXTimer1 = 0;
  YELLOW_LED_state = !YELLOW_LED_state;
  printCAN(msg,1);
}

void printCAN(const CAN_message_t &msg, uint8_t channel){
  current_channel=channel;
  rxmsg=msg;
  load_buffer();
  
  char channel_string[] = "can "; //{'c','a','n',' ',0};
  double timestamp = double(now()) + double(microsecondsPerSecond)/1000000.0;
  if (channel == 0) channel_string[3] = '0';
  else if (channel == 1) channel_string[3] = '1';
  
  memset(ReplyBuffer,0,sizeof(ReplyBuffer));
  
  sprintf(ReplyBuffer,"(%0.6f) %s %08X [%d]",timestamp,channel_string,msg.id,msg.len); 
  
  for ( uint8_t i = 0; i < msg.len; i++ ) {
    sprintf(&ReplyBuffer[strlen(ReplyBuffer)]," %02X",msg.buf[i]);
    //Serial.printf(" %02X",msg.buf[i]); 
  } 
  
  Serial.println(ReplyBuffer);
  
  // // send a reply, to the IP address and port that sent us the packet we received
  // UDP.beginPacket(broadcastIp, UDP.remotePort());
  // //UDP.beginPacket(IPAddress(0, 0, 0, 0), UDP.remotePort());
  // UDP.write(ReplyBuffer);
  // if (UDP.endPacket()) {
  //   Serial.print(" - Sent to ");
  //   Serial.print(broadcastIp);
  //   Serial.print(' ');
  //   Serial.println(UDP.remotePort());
  //   BLUE_LED_state = !BLUE_LED_state;
  // }
  // else {
  //   Serial.println(" - Error");
    
  // }
  // 
  
}


void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);

}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}
