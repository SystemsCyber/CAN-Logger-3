//#define USE_ENCRYPTION 1

/*
 * NMFTA CAN Logger 3 Project   
 * 
 * Arduino Sketch for the CAN Logger 3 to record up to 3 CAN channels using
 * a Teensy 3.6. See https://github.com/Heavy-Vehicle-Networking-At-U-Tulsa/CAN-Logger-3
 * for more details.
 * 
 * Written By Dr. Jeremy S. Daily
 * Colorado State University
 * Department of Systems Engineering
 *  
 * Released under the MIT License
 *
 * Copyright (c) 2020        Jeremy S. Daily, Duy Van
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * 
 * This program logs data to a binary file.  
 * 
 * Samples are logged at regular intervals.  The maximum logging rate
 * depends on the quality of your SD card. 
 * 
 * Much of this sketch was inspired by the examples from 
 * https://github.com/greiman/SdFs
 * 
 * The program makes use of the SdFs library
 * 
 */


//included libraries 
#include <SdFs.h>  // access the SD Card
#include <FlexCAN.h> // Access the CAN network.
#include <error.h> // Include definitions to help understand CAN errors.
#include <OneButton.h> // Handle the buttons
#include <TimeLib.h> // be able to keep realtime.
#include <EEPROM.h> // Use this to keep track of the file names 
#include <FastCRC.h> // Add the CRC to include in the record to validate CAN frame messages.
#include <sha256.h> // Keep track of the file hash as it is created.

#ifdef USE_ENCRYPTION
  #include "CryptoAccel.h" //Makes the cryptographic acceleration hardware arduino compatible
#endif

#include <SparkFun_ATECCX08a_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_ATECCX08a
#include <i2c_t3.h> //use to communicate with the ATECC608a cryptographic coprocessor

const String version_number = "1.0.0";

ATECCX08A atecc;
//Get access to a hardware based CRC32 
FastCRC32 CRC32;


// Use this buffer to keep track of the meta data for each file
char data_file_contents[570]; //Size of MetaData file
uint16_t data_file_index = 0;
char filesize_hash_contents[42]; //10 bytes for filesize and 32 bytes for bin hash

// define the number of rounds with AES encryption.
#define AES_128_NROUNDS 10

// EEPROM memory addresses for creating file names
// The Address 0 and 1 are used for baud rates
#define EEPROM_DEVICE_ID_ADDR     4  // 5, 6, 7=00
#define EEPROM_FILE_ID_ADDR       12  // 9, 10, 11 ==00
#define EEPROM_metadata_ADDR      16 
#define EEPROM_filesize_hash_ADDR (EEPROM_metadata_ADDR + sizeof(data_file_contents))
#define ENCRYPTED_LOGGING_ADDR 2049 // A byte representing the last state of encryption.

#define initial_metadata_size  274 //Time, bitrate, SN, encrypted AES key, IV, Pub

#define CRC32_BUFFER_LOC       508

// Setup a limit to turn off the CAN controller after this many messages.
#define ERROR_COUNT_LIMIT 5000

// Set up the SD Card object
SdFs sd;

#define SD_CONFIG SdioConfig(FIFO_SDIO)

// Create a file object
FsFile binFile;
FsFile dataFile;

String directory_listing;

// All CAN messages are received in the FlexCAN structure
CAN_message_t rxmsg,txmsg;

// Define CAN TXRX Transmission Silent pins
// See the CAN Logger 2 Schematic for the source pins
#define SILENT_0   42
#define SILENT_1   41
#define SILENT_2   40
//#define BUTTON_PIN 20 //version 3
#define BUTTON_PIN 28 //version 3b
#define POWER_PIN  21

//RAW analog Voltage Monitoring
#define RAW_input A22
int RAW_voltage;
int RAW_voltage_threshold;


// Use the button for multiple inputs: click, doubleclick, and long click.
OneButton button(BUTTON_PIN, true);

String commandString;

/*  code to process time sync messages from the serial port   */
char timeString[36];
char iv_string[16];

// define a counter to reset after each second is counted.
elapsedMicros microsecondsPerSecond;
elapsedMicros micro_timer;
elapsedMicros voltage_timer;

// Get a uniqueName for the Logger File
char logger_name[6];
bool file_open;
char current_file_name[13];
char data_file_name[13];
char prefix[5];
char file_name_prefix[6];
char brand_name[3];

//variables for the CAN Message
uint32_t rxId;
uint8_t len;
uint8_t rxBuf[8];
uint8_t ext_flag;

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

elapsedMillis GREEN_LED_Slow_Timer;
elapsedMillis RED_LED_Slow_Timer;
elapsedMillis YELLOW_LED_Slow_Timer;

elapsedMillis GREEN_LED_Fast_Timer;
elapsedMillis RED_LED_Fast_Timer;
elapsedMillis YELLOW_LED_Fast_Timer;

boolean GREEN_LED_slow_blink_state;
boolean RED_LED_slow_blink_state;
boolean YELLOW_LED_slow_blink_state;

boolean GREEN_LED_fast_blink_state;
boolean RED_LED_fast_blink_state;
boolean YELLOW_LED_fast_blink_state;


// Setup a counter to keep track of the filename
char current_file[4];

// Keep track of the CAN Channel (0, 1)
uint8_t current_channel;


// There are 2 data buffers that get switched. We need to know which one
// we are using and where we are in the buffer.
#define CAN_FRAME_SIZE 25
#define MAX_MESSAGES 19
#define BUFFER_POSITION_LIMIT (CAN_FRAME_SIZE * MAX_MESSAGES)
#define BUFFER_SIZE 512
uint8_t data_buffer[BUFFER_SIZE];
uint16_t current_position;
uint32_t buffer_counter;

//Counter and timer to keep track of transmitted messages
#define TX_MESSAGE_TIME 2 //milliseconds
uint32_t TXCount0 = 0;
uint32_t TXCount1 = 0;
uint32_t TXCount2 = 0;
elapsedMillis TXTimer0;
elapsedMillis TXTimer1;
elapsedMillis TXTimer2;

// To use a button to send requests, we need to track it.
#define NUM_REQUEST_PASSES 3
#define REQUEST_TIMING 250 //milliseconds
elapsedMillis send_request_timer;
elapsedMillis send_iso_request_timer;
boolean send_requests;
boolean send_iso_requests;
uint16_t request_index;
uint16_t iso_request_index;
uint8_t send_passes;
uint8_t send_iso_passes;

#define NUM_REQUESTS 27
uint16_t request_pgn[NUM_REQUESTS] = {
  65261, // Cruise Control/Vehicle Speed Setup
  65214, // Electronic Engine Controller 4
  65259, // Component Identification
  65242, // Software Identification
  65244, // Idle Operation
  65260, // Vehicle Identification
  65255, // Vehicle Hours
  65253, // Engine Hours, Revolutions
  65257, // Fuel Consumption (Liquid)
  65256, // Vehicle Direction/Speed
  65254, // Time/Date
  65211, // Trip Fan Information
  65210, // Trip Distance Information
  65209, // Trip Fuel Information (Liquid)
  65207, // Engine Speed/Load Factor Information
  65206, // Trip Vehicle Speed/Cruise Distance Information
  65205, // Trip Shutdown Information
  65204, // Trip Time Information 1
  65200, // Trip Time Information 2
  65250, // Transmission Configuration
  65203, // Fuel Information (Liquid)
  65201, // ECU History
  65168, // Engine Torque History
  64981, // Electronic Engine Controller 5
  64978, // ECU Performance
  64965, // ECU Identification Information
  65165  // Vehicle Electrical Power #2
};

#define NUM_ISO_REQUESTS 9
uint16_t iso_request[NUM_ISO_REQUESTS] = {
  0xF195, //System Supplier ECU Software Version Number
  0xF190, //Vehicle Identfication Number
  0xF193, //System Supplier ECU Hardware Version Number
  0xF18C, //ECU Serial Number
  0xF180, //Boot Software Identfication
  0xF181, //Application Software Identfication
  0xF186, //Active Diagnostic Session
  0xF192, //System Supplier ECU Hardware Number
  0xF197, //System Name or Engine Type
};
    
//Create a counter and timer to keep track of received message traffic
#define RX_TIME_OUT 1000 //milliseconds
elapsedMillis RXTimer;
uint32_t RXCount0 = 0;
uint32_t RXCount1 = 0;
uint32_t RXCount2 = 0;
elapsedMillis lastCAN0messageTimer;
elapsedMillis lastCAN1messageTimer;
elapsedMillis lastCAN2messageTimer;
uint32_t ErrorCount0 = 0;
uint32_t ErrorCount1 = 0;
//uint32_t ErrorCount2 = 0;

// Should we stream the CAN frame to the serial port? Serial 
// can't keep up with heavy CAN bus loads, so this is default to false.
bool stream = false;

// Track the state of the user's desire to log. This can toggle based 
// on user input.
bool recording = true;

// Use the encryption scheme or keep the plain text
bool encrypted_logging = true;

//SHA256
unsigned int hash_counter;
#define SHA256_BLOCK_SIZE 32          // SHA256 outputs a 32 byte digest
byte hash_ciphertext[SHA256_BLOCK_SIZE*2];
byte hash[SHA256_BLOCK_SIZE];
byte before_closing_hash[BUFFER_SIZE];
boolean first_buffer_sent;
Sha256* sha256Instance;
#define SHA_UPDATE_SIZE 64

uint8_t iv_and_key[32];    
unsigned char cipher_text[BUFFER_SIZE];
unsigned char aeskey[16], keysched[4 * 44], in[16], out[16], init_vector[16];
unsigned char encrypted_aeskey[16];

//Generate random iv and key function, 32 bytes number
void iv_key_RNG(){ 
    atecc.updateRandom32Bytes();
    memcpy(&iv_and_key[0],&atecc.random32Bytes[0],sizeof(iv_and_key));
}

#ifdef USE_ENCRYPTION
//AES CBC funtion
void aes_cbc_encrypt(const unsigned char *data, unsigned char *cipher_text){
  //Data length should be a multiple of 16 bytes
  // Need to initialize out with the initialization vector
  for (uint32_t j=0; j < BUFFER_SIZE; j+=16){
    for (uint8_t i = 0; i < 16; i++){
      in[i] = data[j+i] ^ out[i];
    }
    mmcau_aes_encrypt (in, keysched, AES_128_NROUNDS, out); // # 16-byte block
    memcpy(&cipher_text[j],out,16);
  }
}
#endif

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
  //SHA256 last buffer
  if (first_buffer_sent){
    micro_timer=0;
    if ((current_position - 4) % 50 == 0){ // use 50 because it represents 2 messages.
      if (hash_counter < 8){
        for (int z = 0; z < SHA_UPDATE_SIZE; z++){
          hash_ciphertext[z]=cipher_text[hash_counter*SHA_UPDATE_SIZE + z];
        }
        sha256Instance->update(hash_ciphertext,SHA_UPDATE_SIZE);
        hash_counter++;
      }
    }
  }


  // reset the timer
  RXTimer = 0;

  //Toggle the LED
  GREEN_LED_state = !GREEN_LED_state;
  digitalWrite(GREEN_LED, GREEN_LED_state);
    
  data_buffer[current_position] = current_channel;
  current_position += 1;
  
  time_t timeStamp = now();
  memcpy(&data_buffer[current_position], &timeStamp, 4);
  current_position += 4;
  
  memcpy(&data_buffer[current_position], &rxmsg.micros, 4);
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

  if ((rxmsg.id & CAN_ERR_FLAG) == CAN_ERR_FLAG){
    RED_LED_state = !RED_LED_state;
    digitalWrite(RED_LED, RED_LED_state);  
  }
  
  // Check the current position and see if the buffer needs to be written 
  // to the SD card.
  check_buffer();
}

void check_buffer(){
  //Check to see if there is anymore room in the buffer
  if (current_position >= BUFFER_POSITION_LIMIT){ //max number of messages
    //Create a file if it is not open yet
    if (!file_open) {
      open_binFile();
    }  
    uint32_t start_micros = micros();
    // Write the beginning of each line in the 512 byte block
    sprintf(prefix,"CAN2");
    memcpy(&data_buffer[0], &prefix[0], 4);
    current_position = 4;
    
    memcpy(&data_buffer[479], &RXCount0, 4);
    memcpy(&data_buffer[483], &RXCount1, 4);
    memcpy(&data_buffer[487], &RXCount2, 4);
    
    data_buffer[491] = Can0.readREC();
    data_buffer[492] = Can1.readREC();
    //data_buffer[493] = uint8_t(Can2.errorCountRX());
    data_buffer[493] = 0;
    
    data_buffer[494] = Can0.readTEC();
    data_buffer[495] = Can1.readTEC();
    //data_buffer[496] = uint8_t(Can2.errorCountTX());
    data_buffer[496] = 0;
    
    // Write the filename to each line in the 512 byte block
    memcpy(&data_buffer[497], &current_file_name, 8);
  
    uint32_t checksum = CRC32.crc32(data_buffer, CRC32_BUFFER_LOC);
    memcpy(&data_buffer[CRC32_BUFFER_LOC], &checksum, 4);

    write_to_sd();
    
    buffer_counter++;
    if (buffer_counter >= 1843200) {//When file size reaches 900 MB, start a new one
      Serial.print("buffer_counter = ");
      Serial.println(buffer_counter);
      buffer_counter = 0;
      myDoubleClickFunction(); 
    }
    //Reset the record
    memset(&data_buffer,0xFF,BUFFER_SIZE);
    
    //Record write times for the previous frame, since the existing frame was just written
    uint32_t elapsed_micros = micros() - start_micros;
    memcpy(&data_buffer[505], &elapsed_micros, 3);
    
    //Toggle LED to show SD card writing
    YELLOW_LED_state = !YELLOW_LED_state;
    digitalWrite(YELLOW_LED,YELLOW_LED_state);
  }
}

void write_to_sd(){
#ifdef USE_ENCRYPTION
    if (encrypted_logging){
      aes_cbc_encrypt(data_buffer,cipher_text);//Encrypt 512-byte buffer 
      digitalWrite(BLUE_LED,HIGH);
    }
    else {
      memcpy(&cipher_text[0],&data_buffer[0],BUFFER_SIZE);
    }
#else
    memcpy(&cipher_text[0],&data_buffer[0],BUFFER_SIZE);
#endif
    if (BUFFER_SIZE != binFile.write(cipher_text, BUFFER_SIZE)) {
      Serial.println("write failed");
      sdErrorFlash(); 
    }
    //If the first buffer is sent, set first_buffer_sent to true and hash counter to 0 for load_buffer()
    else{
      first_buffer_sent = true;
      hash_counter = 0;  
    }
}

void print_hex(){
  char line[BUFFER_SIZE];
  if (!binFile.isOpen()) close_binFile();
  if (sd.exists(current_file_name)){
    binFile.open(current_file_name, O_READ);
    while (binFile.read(line, sizeof(line)) > 0) {
      for (uint16_t i = 0; i < sizeof(line); i++){
        char disp[4];
        sprintf(disp,"%02X ",line[i]);
        Serial.print(disp);
      }  
      Serial.println();
    }
    binFile.close();
  }
  else
  {
    Serial.println("Current file does not exist");
  }
}

void delete_file(char delete_file_name[]){
  if (!binFile.isOpen()) close_binFile();
  // Remove any existing file.
  if (sd.exists(delete_file_name)) {
    sd.remove(delete_file_name); 
  }
  else
  {
    Serial.println("Current file does not exist");
  }
}

void stream_binary(char stream_file_name[]){
  turn_streaming_off();
  turn_recording_off();
  uint8_t line[BUFFER_SIZE];
  if (sd.exists(stream_file_name)){
    binFile.open(stream_file_name, O_READ);
    digitalWrite(YELLOW_LED,HIGH);
    while (binFile.read(line, BUFFER_SIZE) > 0) 
    { 
      Serial.write(line,BUFFER_SIZE);
      if (Serial.available() >= 2){
        commandString = Serial.readStringUntil('\n');
        if (commandString.equalsIgnoreCase("OFF")) break;
      }
    }
    digitalWrite(YELLOW_LED,LOW);
    binFile.close();
  }
  else
  {
    Serial.println("Current file does not exist");
  }
}

void stream_text(char stream_file_name[]){
  turn_streaming_off();
  FsFile textFile; 
  if (sd.exists(stream_file_name))
  {
    textFile.open(stream_file_name, O_READ);
    while (textFile.available()) 
    {
      Serial.write(textFile.read());
    }
  }
  else
  {
    Serial.println("Current file does not exist");
  }
}

/*
 * Check to make sure the characters are valid in a character array
 * This is used to check the data coming from EEPROM.
 */
bool isFileNameValid( const char *fileName )
{
  char c;
  while ( (c = *fileName++) )
    if ( c != '.' && !isalnum(c) )
      return false;
  return true;
}

/*
 * Reset the microsecond counter and return the realtime clock
 * This function requires the sync interval to be exactly 1 second.
 */
time_t getTeensy3Time(){
  microsecondsPerSecond = 0;
  return Teensy3Clock.get();
}

/*
 * Routine to enable date and time for the sdcard 
 * file system
 */
void dateTime(uint16_t* FSdate, uint16_t* FStime) {
  // Return date using FS_DATE macro to format fields.
  *FSdate = FS_DATE(year(), month(), day());

  // Return time using FS_TIME macro to format fields.
  *FStime = FS_TIME(hour(), minute(), second());
}


/*
 * Routines for the Microchip MCP2515 CAN Controller on Can2
 * This third CAN channel is wired to the J1708 pins (F and G) found
 * on some newer PACCAR trucks.
 *
void send_Can2_message(CAN_message_t &txmsg){
  if (TXTimer2 >= TX_MESSAGE_TIME){
    //Send message in format: ID, Standard (0) or Extended ID (1), message length, txmsg
    Can2.sendMsgBuf(txmsg.id, 1, txmsg.len, txmsg.buf);  
    TXCount2++;
    TXTimer2 = 0;
  }  
}

 * Routines for the FlexCAN Controller on Can0 and Can1
 * Can0 is usually J1939
 */
void send_Can0_message(CAN_message_t &txmsg){
  if (TXTimer0 >= TX_MESSAGE_TIME){    
    //Send message in FlexCAN format
    Can0.write(txmsg);
    TXCount0++;
    TXTimer0 = 0;   
  }
}    

void send_Can1_message(CAN_message_t &txmsg){
  if (TXTimer1 >= TX_MESSAGE_TIME){ // Keep from flooding the bus.   
    //Send message in FlexCAN format
    Can1.write(txmsg);
    TXCount1++;
    TXTimer1 = 0;
  }
}

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

void sdErrorFlash(){
  while(true){
    digitalWrite(RED_LED,HIGH);
    delay(50);
    digitalWrite(RED_LED,LOW);
    delay(50);
    // Try starting the SD Card again
    if (sd.begin(SD_CONFIG)) break; 
  }
}

void get_current_file(){
  current_file[2]++;
  if (current_file[2] == ':')
  {
    current_file[2] = 'A';
  }
  else if (current_file[2] == '[') 
  {
    current_file[2] = '0';
    current_file[1]++;
  }
  else if (current_file[2] < 0x30) current_file[2] = '0';
  else if (current_file[2] > 'Z')  current_file[2] = '0';
  
  if (current_file[1] == ':') current_file[1] = 'A';
  else if (current_file[1] == '[') {
    current_file[1] = '0';
    current_file[0]++;
  }
  else if (current_file[1] < 0x30) current_file[2] = '0';
  else if (current_file[1] > 'Z')  current_file[2] = '0';
  
  if (current_file[0] == ':') current_file[0] = 'A';
  else if (current_file[0] == '[') {
    current_file[0] = '0';
  }
  else if (current_file[0] < 0x30) current_file[2] = '0';
  else if (current_file[0] > 'Z')  current_file[2] = '0';
  
}

void open_binFile(){
  buffer_counter = 1;
  first_buffer_sent = false;
  sha256Instance = new Sha256();

  //Increase file name by 1
  get_current_file();
  
  sprintf(current_file_name,"%s%s.bin",logger_name,current_file);
  if (!binFile.open(current_file_name, O_RDWR | O_CREAT)) {
    YELLOW_LED_fast_blink_state == true;
    Serial.println("Error opening ");
    Serial.println(current_file_name);
  }
  else
  {
    YELLOW_LED_fast_blink_state == false;
    write_initial_meta_data();
  }

  //Move the current position to 4 since the first 4 bytes are taken.
  current_position = 4;
  binFile.truncate(0);
  file_open = true;
  RXTimer = 0;
}

void close_binFile(){
  micro_timer = 0;
  if (first_buffer_sent){
    if (hash_counter < 8){
      memcpy(&before_closing_hash[0],&cipher_text[hash_counter*SHA_UPDATE_SIZE],BUFFER_SIZE-hash_counter*SHA_UPDATE_SIZE);
      sha256Instance->update(before_closing_hash,BUFFER_SIZE-hash_counter*SHA_UPDATE_SIZE);
      //Serial.print("Time to hash the last buffer before closing buffer (us):");
      //Serial.println(micro_timer);
    }
  
    // Add integrity to the last line of the file.
    uint32_t checksum = CRC32.crc32(data_buffer, CRC32_BUFFER_LOC);
    memcpy(&data_buffer[CRC32_BUFFER_LOC], &checksum, 4);
    
    write_to_sd();
    uint32_t file_size = binFile.size();

    binFile.close();
    EEPROM.put(EEPROM_FILE_ID_ADDR,current_file); //Write current file name to EEPROM

    char size_char[11];
    sprintf(size_char,"%10d",file_size);
    memcpy(&filesize_hash_contents[0],&size_char,10);

    sha256Instance->update(cipher_text,BUFFER_SIZE);
    sha256Instance->final(hash);
    delete sha256Instance;
    memcpy(&filesize_hash_contents[10],&hash,32);    
    EEPROM.put(EEPROM_filesize_hash_ADDR, filesize_hash_contents); //Write important metada of current file for backup in case of power loss

    Serial.print(",SIZE:");
    memcpy(&data_file_contents[data_file_index],",SIZE:",6);
    data_file_index+=6;
    Serial.print(size_char);
    memcpy(&data_file_contents[data_file_index],&size_char,10);
    data_file_index+=10;
    
    write_final_meta_data();

    file_open = false;
    //Initialize the CAN channels with autobaud.
    RXCount0 = 0;
    RXCount1 = 0;
    RXCount2 = 0;
    Can0.begin(0);
    Can1.begin(0);
  
    //Set first buffer sent back to false when closing file
    first_buffer_sent = false;
    RAW_voltage_threshold = 0.5 * analogRead(RAW_input);
    Serial.printf("RAW_voltage_threshold = %i\n", RAW_voltage_threshold);  
  }
}

void get_prev_metadata(){
  //Getting previous file metadata from EEPROM
  memset(data_file_contents,0,sizeof(data_file_contents));
  data_file_index=0;
  EEPROM.get(EEPROM_metadata_ADDR,data_file_contents);
  EEPROM.get(EEPROM_filesize_hash_ADDR,filesize_hash_contents);
  memcpy(&data_file_contents[initial_metadata_size],",SIZE:",6);
  int index =0;
  index+=6;
  memcpy(&data_file_contents[initial_metadata_size+index],&filesize_hash_contents[0],10);
  index+=10;
  memcpy(&hash[0],&filesize_hash_contents[10],32);

  //Perform hashing and signing to output metatdata textfile
  memcpy(&data_file_contents[initial_metadata_size+index],",BIN-SHA:",9);
  index+=9;
  for (int n = 0; n < sizeof(hash); n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",hash[n]);
    memcpy(&data_file_contents[initial_metadata_size+index],&hex_digit,2);
    index+=2;
  }
  sha256Instance=new Sha256();
  sha256Instance->update(data_file_contents, strlen((const char*)data_file_contents));
  sha256Instance->final(hash);
  delete sha256Instance;

  memcpy(&data_file_contents[initial_metadata_size+index],",TXT-SHA:",9);
  index+=9;
  for (int n = 0; n < sizeof(hash); n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",hash[n]);
    memcpy(&data_file_contents[initial_metadata_size+index],&hex_digit,2);
    index+=2;
  }
  
  // Compute Signature here
  atecc.createSignature(hash);
  memcpy(&data_file_contents[initial_metadata_size+index],",SIG:",5);
  index+=5;
  
  for (int n = 0; n < sizeof(atecc.signature); n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",atecc.signature[n]);
    memcpy(&data_file_contents[initial_metadata_size+index],&hex_digit[0],2);
    index+=2;
  }
  Serial.println();

  //Write the metadata file
  if (!dataFile.open(data_file_name, O_RDWR | O_CREAT)) {
    YELLOW_LED_fast_blink_state == true;
    Serial.println("Error opening textfile to write previous log metadata");
    Serial.println(data_file_name);
  }
  else{
    for (int i = 0; i<strlen((const char*)data_file_contents);i++){
      dataFile.write(data_file_contents[i]);
    } 
    dataFile.close();
  }
  Serial.println("Previous File Metadata:");
  Serial.println(data_file_contents);
  
  memset(data_file_contents,0,sizeof(data_file_contents));
  data_file_index=0;
}

void led_blink_routines(){
  if (GREEN_LED_fast_blink_state)
  {
    if (GREEN_LED_Fast_Timer >= FAST_BLINK_TIME)
    {
      GREEN_LED_Fast_Timer = 0;
      GREEN_LED_state = !GREEN_LED_state;
      digitalWrite(GREEN_LED,GREEN_LED_state);
    }
  }
  else if (GREEN_LED_slow_blink_state)
  {
    if (GREEN_LED_Slow_Timer >= SLOW_BLINK_TIME)
    {
      GREEN_LED_Slow_Timer = 0;
      GREEN_LED_state = !GREEN_LED_state;
      digitalWrite(GREEN_LED,GREEN_LED_state);
    }
  }
  else
  {
    //Turn on the green LED after receiving.
    if (lastCAN1messageTimer > 200){
      lastCAN1messageTimer = 0;
      GREEN_LED_state = HIGH;
      digitalWrite(GREEN_LED,GREEN_LED_state);
    }
  }

  /*
   * YELLOW LED functions
   * Use the yellow LED for status
   * Flicker the yellow LED for for received messages
   */
  if (YELLOW_LED_fast_blink_state)
  {
    if (YELLOW_LED_Fast_Timer >= FAST_BLINK_TIME)
    {
      YELLOW_LED_Fast_Timer = 0;
      YELLOW_LED_state = !YELLOW_LED_state;
      digitalWrite(YELLOW_LED,YELLOW_LED_state);
    }
  }
  else if (YELLOW_LED_slow_blink_state)
  {
    if (YELLOW_LED_Slow_Timer >= SLOW_BLINK_TIME)
    {
      YELLOW_LED_Slow_Timer = 0;
      YELLOW_LED_state = !YELLOW_LED_state;
      digitalWrite(YELLOW_LED,YELLOW_LED_state);
    }
  }
  else
  {
    //Turn off the yellow LED after receiving.
    if (lastCAN0messageTimer > 200){
      lastCAN0messageTimer = 0;
      YELLOW_LED_state = LOW;
      digitalWrite(YELLOW_LED,YELLOW_LED_state);
    }
  }
  
  /*
   * RED LED functions
   * Use the Red LED for error states
   * Also, flicker the RED LED for transmitted messages
   */
  if (RED_LED_fast_blink_state)
  {
    if (RED_LED_Fast_Timer >= FAST_BLINK_TIME)
    {
      RED_LED_Fast_Timer = 0;
      RED_LED_state = !RED_LED_state;
      digitalWrite(RED_LED,RED_LED_state);
    }
  }
  else if (RED_LED_slow_blink_state)
  {
    if (RED_LED_Slow_Timer >= SLOW_BLINK_TIME)
    {
      RED_LED_Slow_Timer = 0;
      RED_LED_state = !RED_LED_state;
      digitalWrite(RED_LED,RED_LED_state);
    }
  }
  else
  {
    //Turn off the red LED after some time after transmitting.
    if (TXTimer0 >= 300 && TXTimer1 >= 300 && TXTimer2 >= 300)
    {
      RED_LED_state = LOW;
      digitalWrite(RED_LED,RED_LED_state);  
    }
  }
}

void myClickFunction(){
    turn_requests_on();
}

void myDoubleClickFunction(){
    close_binFile();
    setup();
  }

void myLongPressStopFunction(){
  RED_LED_state = LOW;
  digitalWrite(RED_LED,RED_LED_state);
}

void myLongPressStartFunction(){
  
}

void myLongPressFunction(){
  RED_LED_state = HIGH;
  digitalWrite(RED_LED,RED_LED_state);
}


void setup(void) {
#ifdef USE_ENCRYPTION 
  EEPROM.get(ENCRYPTED_LOGGING_ADDR,encrypted_logging);
#endif
  Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, 100000);
  if (atecc.begin() == true)
  {
    Serial.println("Successful wakeUp(). I2C connections are good.");
  }
  else
  {
    Serial.println("Device not found. Check wiring.");
    while (1); // stall out forever
  }
  atecc.readConfigZone(false); // produces a serial number 
  atecc.generatePublicKey(0,false); // Calculate the public key
  Serial.println("Starting CAN logger.");

  first_buffer_sent = false;
 
  commandString.reserve(256);
  
  //setup pin modes for the transeivers
  pinMode(SILENT_0,OUTPUT);
  pinMode(SILENT_1,OUTPUT);
  pinMode(SILENT_2,OUTPUT);
  
  //Set High to prevent transmission for the CAN TXRX
  digitalWrite(SILENT_0,LOW); 
  digitalWrite(SILENT_1,LOW);
  digitalWrite(SILENT_2,LOW);

  // Setup chip select pin for the MCP2515
  // pinMode(CS_CAN, OUTPUT);
  // digitalWrite(CS_CAN,HIGH);
  
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  GREEN_LED_state = HIGH;
  YELLOW_LED_state = HIGH;
  RED_LED_state = HIGH;
  digitalWrite(GREEN_LED,GREEN_LED_state);
  digitalWrite(YELLOW_LED,YELLOW_LED_state);
  digitalWrite(RED_LED,RED_LED_state);
 
  // Setup the button with an internal pull-up
  pinMode(BUTTON_PIN,INPUT_PULLUP);
  
  // Setup button functions 
  button.attachClick(myClickFunction);
  button.attachDoubleClick(myDoubleClickFunction);
  button.attachLongPressStart(myLongPressStartFunction);
  button.attachLongPressStop(myLongPressStopFunction);
  button.attachDuringLongPress(myLongPressFunction);
  button.setDebounceTicks(50);
  button.setPressTicks(2000);
  button.setClickTicks(500);
  
  // Select the CAN on J1939 Pins F and G
  // pinMode(CAN_SWITCH,OUTPUT);
  // digitalWrite(CAN_SWITCH,LOW);

  iv_key_RNG();
 
#ifdef USE_ENCRYPTION  
  for (int i = 0; i < sizeof(init_vector); i++)  init_vector[i] = iv_and_key[i];
  mmcau_aes_set_key(aeskey, 128, keysched);//Set key
  memcpy(out,init_vector,16); //Load IV

  // Derive ECDH and load into TEMPKEY
  atecc.readPublicKey(false);
  Serial.println("Stored Public Key: ");
  for (int i = 0; i<sizeof(atecc.storedPublicKey); i++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",atecc.storedPublicKey[i]);
    Serial.print(hex_digit);
  }
  Serial.println();
  atecc.ECDH(atecc.storedPublicKey, ECDH_OUTPUT_IN_TEMPKEY,0x0000);
  
  // Add the ATECC Encryption Scheme here and update the value of the encrypted_aeskey
  Serial.print("Encrypted AES Session Key: ");
  atecc.AES_ECB_encrypt(aeskey,0xFFFF,false);
  memcpy(&encrypted_aeskey[0],&atecc.AES_buffer[0],16);
  for (int i = 0; i<16; i++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",encrypted_aeskey[i]);
    Serial.print(hex_digit);
  }
  Serial.println();
#endif  

  Can0.setReportErrors(true);
  Can1.setReportErrors(true);
  
  //Initialize the CAN channels with autobaud.
  Can0.begin(0);
  Can1.begin(0);

  Can0.setListenOnly(true);
  Can1.setListenOnly(true);
  
  Can0.setSelfReception(true);
  Can1.setSelfReception(true);
    
  //Flex CAN defaults
  txmsg.ext = 1;
  txmsg.len = 8;

  // Setup MCP CAN
  //if(Can2.begin(MCP_ANY, CAN_250KBPS, MCP_16MHZ) == CAN_OK) Serial.println("MCP2515 Initialized Successfully!");
  //else Serial.println("Error Initializing MCP2515...");
  //Can2.setMode(MCP_NORMAL);

  // Setup timing services
  setSyncProvider(getTeensy3Time);
  if (timeStatus()!= timeSet) {
    Serial.println("Unable to sync with the RTC");
  } else {
    Serial.println("RTC has set the system time");
  }
  setSyncInterval(1);
  
  sprintf(timeString,"%04d-%02d-%02d %02d:%02d:%02d.%06d",year(),month(),day(),hour(),minute(),second(),uint32_t(microsecondsPerSecond));
  Serial.println(timeString);

  // Set callback
  FsDateTime::callback = dateTime;

  // Mount the SD Card
  if (!sd.begin(SD_CONFIG)) {
      // If the SD card is missing, then it will f
      Serial.println("SdFs begin() failed. Please check the SD Card.");
      sdErrorFlash(); 
  }
  
  // Check for free space on the card.
  Serial.print("SD Total Cluster Count: ");
  Serial.println(sd.clusterCount());
  Serial.print("SD Free Cluster Count: ");
  Serial.println(sd.freeClusterCount());
  
  if (sd.freeClusterCount() < 5000){
    Serial.println("There needs to be more free space on the SD card.");
    sdErrorFlash();
  }

  sd.chvol();

  Serial.print("Reading from EEPROM... ");
  EEPROM.get(EEPROM_DEVICE_ID_ADDR,logger_name);
  if (!isFileNameValid(logger_name)) strcpy(logger_name, "CSU__"); 
  // Set the DEVICE ID with the ID command
  
  EEPROM.get(EEPROM_FILE_ID_ADDR,current_file);
  if (!isFileNameValid(current_file)) strcpy(current_file, "000");
  // reset the counter with the count command
  
  Serial.println("Done.");
  
  sprintf(file_name_prefix,"%s%s",brand_name,logger_name);
  Serial.print("The filename prefix is ");
  Serial.println(file_name_prefix);

  //Check if the previous log does not have metadata file, if so retrieve it from EEPROM
  sprintf(data_file_name,"%s%s%s.txt",brand_name,logger_name,current_file);
  sprintf(current_file_name,"%s%s%s.bin",brand_name,logger_name,current_file);
  if (!sd.exists(data_file_name) && sd.exists(current_file_name)){
    get_prev_metadata();
  }
  RAW_voltage_threshold = 0.5 * analogRead(RAW_input);
  Serial.printf("RAW_voltage_threshold = %i\n", RAW_voltage_threshold);
  recording = true;
  //pinMode(POWER_PIN,INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(POWER_PIN), close_binFile, RISING);
}


void write_final_meta_data(){
  
  Serial.print(",BIN-SHA:");
  memcpy(&data_file_contents[data_file_index],",BIN-SHA:",9);
  data_file_index+=9;
  for (int n = 0; n < sizeof(hash); n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",hash[n]);
    Serial.print(hex_digit);
    memcpy(&data_file_contents[data_file_index],&hex_digit,2);
    data_file_index+=2;
  }
    
  sha256Instance=new Sha256();
  sha256Instance->update(data_file_contents, strlen((const char*)data_file_contents));
  sha256Instance->final(hash);
  delete sha256Instance;
  
  Serial.print(",TXT-SHA:");
  memcpy(&data_file_contents[data_file_index],",TXT-SHA:",9);
  data_file_index+=9;
  for (int n = 0; n < sizeof(hash); n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",hash[n]);
    Serial.print(hex_digit);
    memcpy(&data_file_contents[data_file_index],&hex_digit,2);
    data_file_index+=2;
  }
  
  // Compute Signature here
  atecc.createSignature(hash);
  Serial.print(",SIG:");
  memcpy(&data_file_contents[data_file_index],",SIG:",5);
  data_file_index+=5;
  
  for (int n = 0; n < sizeof(atecc.signature); n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",atecc.signature[n]);
    Serial.print(hex_digit);
    memcpy(&data_file_contents[data_file_index],&hex_digit,2);
    data_file_index+=2;
  }
  Serial.println();

  //Write the metadata file
  sprintf(data_file_name,"%s%s%s.txt",brand_name,logger_name,current_file);
  if (!dataFile.open(data_file_name, O_RDWR | O_CREAT)) {
    YELLOW_LED_fast_blink_state == true;
    Serial.println("Error opening ");
    Serial.println(data_file_name);
  }
  else{

    for (int i = 0; i<strlen((const char*)data_file_contents);i++){
      dataFile.write(data_file_contents[i]);
    } 
    dataFile.close();
    
  }
  Serial.println(data_file_contents);
  
  memset(data_file_contents,0,sizeof(data_file_contents));
  data_file_index=0;

  //Serial.print("Total closing time to create metadata textfile (us):");
  //Serial.println(micro_timer);
}

void write_initial_meta_data(){
  sprintf(timeString,"%04d-%02d-%02dT%02d:%02d:%02d,%7d,%7d,",year(),month(),day(),hour(),minute(),second(),Can0.baud_rate,Can1.baud_rate);
  Serial.print(timeString);
  memcpy(&data_file_contents[data_file_index],&timeString[0],sizeof(timeString));
  data_file_index+=sizeof(timeString);
  
  Serial.print(current_file_name);
  memcpy(&data_file_contents[data_file_index],&current_file_name,12);
  data_file_index+=12;

  Serial.print(",SN:");
  memcpy(&data_file_contents[data_file_index],",SN:",4);
  data_file_index+=4;
  for (int n = 0; n < sizeof(atecc.serialNumber); n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",atecc.serialNumber[n]);
    Serial.print(hex_digit);
    memcpy(&data_file_contents[data_file_index],&hex_digit,2);
    data_file_index+=2;
  }

  
  Serial.print(",IV:");
  memcpy(&data_file_contents[data_file_index],",IV:",4);
  data_file_index+=4;
  for (int n = 0; n < sizeof(init_vector); n++){
    char hex_digit[3];
    if (encrypted_logging) sprintf(hex_digit,"%02X",init_vector[n]);
    else sprintf(hex_digit,"%02X",0);
    Serial.print(hex_digit);
    memcpy(&data_file_contents[data_file_index],&hex_digit,2);
    data_file_index+=2;
  }
  
  Serial.print(",KEY:");
  memcpy(&data_file_contents[data_file_index],",KEY:",5);
  data_file_index+=5;
  for (int n = 0; n < sizeof(encrypted_aeskey); n++){
    char hex_digit[3];
    if (encrypted_logging) sprintf(hex_digit,"%02X",encrypted_aeskey[n]);
    else sprintf(hex_digit,"%02X",0);
    Serial.print(hex_digit);
    memcpy(&data_file_contents[data_file_index],&hex_digit,2);
    data_file_index+=2;
  }
  
  Serial.print(",PUB:");
  memcpy(&data_file_contents[data_file_index],",PUB:",5);
  data_file_index+=5;
  for (int n = 0; n < sizeof(atecc.publicKey64Bytes); n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",atecc.publicKey64Bytes[n]);
    Serial.print(hex_digit);
    memcpy(&data_file_contents[data_file_index],&hex_digit,2);
    data_file_index+=2;
  }
  
  EEPROM.put(EEPROM_metadata_ADDR, data_file_contents);
}

void rx_message_routine(uint32_t RXCount){
  if (recording) load_buffer();
  if (stream) printFrame(rxmsg,current_channel,RXCount);
  if ((rxmsg.id & CAN_ERR_FLAG) == CAN_ERR_FLAG){
    if (current_channel == 0) ErrorCount0++;
    else if (current_channel == 1) ErrorCount1++;
    //else if (current_channel == 2) ErrorCount2++;
    RED_LED_state = !RED_LED_state;                       
    digitalWrite(RED_LED,RED_LED_state);  
  }
  else if ((rxmsg.id & 0x00DAF900) == 0x00DAF900){
    if ((rxmsg.buf[0] & 0xF0) == 0x10){ // First frame
      txmsg.len = 3;
      txmsg.id = 0x18DA00F9;
      txmsg.buf[0] = 0x30; //Clear to send
      txmsg.buf[1] = 0x00; // A Block Size of zero indicates the sender does not have to send a flow
                           // control message for subsequent bytes
      txmsg.buf[2] = 0x00; // Separation time in milliseconds
      if (current_channel == 0) send_Can0_message(txmsg);
      else if (current_channel == 1) send_Can1_message(txmsg);
      //else if (current_channel == 2) send_Can2_message(txmsg);
    }
  }
}

void loop(void) {
  //Monitor voltage, close file if drop below ~9V
  //Raw voltage analog range is 0-149 for 0-12.2V
  RAW_voltage = analogRead(RAW_input);
  if (RAW_voltage < RAW_voltage_threshold) {
    close_binFile(); // analog of 110 ~ 9V raw voltage
    Serial.println("Closed file due to loss of power.");
  }
  if (voltage_timer > 1000) {
    //Serial.printf("Voltage = %i\n",RAW_voltage);
    voltage_timer =0;
  }
  
  // monitor the CAN channels
  if (Can0.read(rxmsg)){
    RXCount0++;
    current_channel = 0;
    rx_message_routine(RXCount0);
  }
  if (Can1.read(rxmsg)){
    RXCount1++;
    current_channel = 1;
    rx_message_routine(RXCount1);
  }
  /*if (Can2.readMsgBuf(&rxId, &ext_flag, &len, rxBuf) == CAN_OK){
    RXCount2++;
    memcpy(&rxmsg.buf, rxBuf, 8);
    rxmsg.len = uint8_t(len);
    rxmsg.id = uint32_t(rxId);
    current_channel = 2;
    rx_message_routine(RXCount2);
  } */

  // Close the file if messages stop showing up.
  if (RXTimer >= RX_TIME_OUT && file_open){
    close_binFile();
    RXTimer = 0;
  }
  
  led_blink_routines();
  
  // Send requests for additional data if needed
  if (send_requests){
    if (send_passes < NUM_REQUEST_PASSES){
      if (send_request_timer > REQUEST_TIMING){
        send_request_timer = 0;
        txmsg.len = 3;
        txmsg.id = 0x18EAFFF9;
        txmsg.buf[0] = (request_pgn[request_index] & 0x0000FF);
        txmsg.buf[1] = (request_pgn[request_index] & 0x00FF00) >> 8 ;
        txmsg.buf[2] = (request_pgn[request_index] & 0xFF0000) >> 16; //These are in reverse byte order.
        send_Can0_message(txmsg);
        if (RXCount1 > 0) send_Can1_message(txmsg);
        //if (RXCount2 > 0) send_Can2_message(txmsg);
        //Toggle LED light as messages are sent
        RED_LED_state = !RED_LED_state;                       
        digitalWrite(RED_LED,RED_LED_state);  
        request_index++;
        if (request_index >= NUM_REQUESTS) {
          request_index = 0;
          send_passes++;
          //shuffle
          //Serial.println("Shuffling Requests");
          for (int i = 0; i < NUM_REQUESTS; i++) {
            int j = random(i, NUM_REQUESTS);
            auto temp = request_pgn[i];
            request_pgn[i] = request_pgn[j];
            request_pgn[j] = temp;
          }
        }
      }
    }
    else
    {
      send_passes = 0;
      send_requests = false;
      send_iso_requests = true;
      send_iso_request_timer = 0;
    }
  }
  else if (send_iso_requests)
  {
    if (send_iso_passes < NUM_REQUEST_PASSES){
      if (send_iso_request_timer > REQUEST_TIMING){
        send_iso_request_timer = 0;
        txmsg.len = 8;
        txmsg.id = 0x18DAFFF9;
        txmsg.buf[0] = 0x02;
        txmsg.buf[1] = (iso_request[request_index] & 0x00FF) >> 0 ;
        txmsg.buf[2] = (iso_request[request_index] & 0xFF00) >> 8; //These are in reverse byte order.
        send_Can0_message(txmsg);
        if (RXCount1 > 0) send_Can1_message(txmsg); //only send something if the network is alive
        //if (RXCount2 > 0) send_Can2_message(txmsg); //only send something if the network is alive
        //Toggle LED light as messages are sent
        RED_LED_state = !RED_LED_state;                       
        digitalWrite(RED_LED,RED_LED_state);  
        request_index++;
        if (request_index >= NUM_ISO_REQUESTS) {
          request_index = 0;
          send_iso_passes++;
          //shuffle
          //Serial.println("Shuffling Requests");
          for (int i = 0; i < NUM_ISO_REQUESTS; i++) {
            int j = random(i, NUM_ISO_REQUESTS);
            auto temp = iso_request[i];
            iso_request[i] = request_pgn[j];
            iso_request[j] = temp;
          }
        }
      }
    }
    else
    {
      send_iso_passes = 0;
      send_iso_requests = false;
    }
  }
  
  if (Serial.available() >= 2) {
    commandString = Serial.readStringUntil('\n');
    if      (commandString.equalsIgnoreCase("HEX"))        print_hex();
    else if (commandString.equalsIgnoreCase("VERSION"))    Serial.println(version_number);
    else if (commandString.equalsIgnoreCase("STOP"))       turn_recording_off();
    else if (commandString.equalsIgnoreCase("START"))      turn_recording_on();
    else if (commandString.equalsIgnoreCase("NEW"))        close_binFile();
    else if (commandString.equalsIgnoreCase("DF"))         sd_capacity();
    else if (commandString.equalsIgnoreCase("LS"))         list_files();
    else if (commandString.equalsIgnoreCase("LS A"))       list_files_a();
    else if (commandString.equalsIgnoreCase("FORMAT"))     format_sd_card();
    else if (commandString.equalsIgnoreCase("BAUD"))       display_baud_rate();
    else if (commandString.equalsIgnoreCase("ERRORS"))     display_error_count();
    else if (commandString.equalsIgnoreCase("STREAM ON"))  turn_streaming_on();
    else if (commandString.equalsIgnoreCase("STREAM OFF")) turn_streaming_off();
    else if (commandString.equalsIgnoreCase("REQUEST ON")) turn_requests_on();
    else if (commandString.equalsIgnoreCase("REQUEST OFF"))turn_requests_off();
#ifdef USE_ENCRYPTION
    else if (commandString.equalsIgnoreCase("ENCRYPT ON")) turn_encrypted_logging_on();
    else if (commandString.equalsIgnoreCase("ENCRYPT OFF"))turn_encrypted_logging_off();
#endif
    else if (commandString.startsWith("BIN ")){
      char current_file_name[13];
      commandString.remove(0,4);
      commandString.toCharArray(current_file_name,13);
      stream_binary(current_file_name);
    }
    else if (commandString.startsWith("DEL ")){
      char delete_file_name[13];
      commandString.remove(0,4);
      commandString.toCharArray(delete_file_name,13);
      delete_file(delete_file_name);
    }
    else if (commandString.equalsIgnoreCase("BAUDRATE")){
      stream_text(data_file_name);
    }
    else if (commandString.startsWith("ID ")){
      commandString.remove(0,3);
      if (commandString.length() == 5){
        commandString.toUpperCase();
        commandString.toCharArray(logger_name,sizeof(logger_name));
        EEPROM.put(EEPROM_DEVICE_ID_ADDR,logger_name);
        Serial.print("Set Device ID to ");
        Serial.println(logger_name);
      }
      else {
        Serial.println("Improper ID Length. Expecting 5 characters (CSUXX).");
      }
    }
    else if (commandString.startsWith("COUNT")){
      commandString.remove(0,6);
      commandString.toUpperCase();
      char input_count[4];
      commandString.toCharArray(input_count,4);
      memset(current_file,0,4);
      memset(current_file,'0',3);
      if (commandString.length() == 3) {
        current_file[0] = input_count[0];
        current_file[1] = input_count[1];
        current_file[2] = input_count[2];
      }
      else if (commandString.length() == 2){
        current_file[1] = input_count[0];
        current_file[2] = input_count[1];
      }
      else if (commandString.length() == 1){
        current_file[2] = input_count[0];
      }
      if (isFileNameValid(current_file)){
        EEPROM.put(EEPROM_FILE_ID_ADDR,current_file);
        Serial.print("Set current file to ");
        Serial.println(current_file);
      }
      else
      {
        Serial.println("Not a valid file count.");
      }
    }    
    else if (commandString.equalsIgnoreCase("HELP")){
      Serial.println(F("List of available commands:"));
      Serial.println(F("VERSION     (Display the current software version)"));
      Serial.println(F("HEX         (Stream the latest log file in printable hexadecimal)"));
      Serial.println(F("BIN         (Stream the latest log file in binary format to the serial port)"));
      Serial.println(F("DEL [file-name.bin] (Delete the chosen file in the SD card)"));
      Serial.println(F("STOP        (Turn recording off)"));
      Serial.println(F("START       (Turn recording on)"));
      Serial.println(F("NEW         (Start a new log file)"));
      Serial.println(F("DF          (Show SD card capacity)"));
      Serial.println(F("LS          (List files in the SD card)"));
      Serial.println(F("LS A        (List files in the SD card with time stamp)"));
      Serial.println(F("FORMAT      (Format the SD card)"));
      Serial.println(F("BAUD        (Display current baudrate on the channels)"));
      Serial.println(F("ERRORS      (Display error count on the channels)"));
      Serial.println(F("REQUEST ON  (Turn requests on)"));
      Serial.println(F("REQUEST OFF (Turn request off)"));
      Serial.println(F("STREAM ON   (Start sending interpreted CAN Frames to the Serial port)"));
      Serial.println(F("STREAM OFF  (Stop sending interpreted CAN Frames to the Serial port)"));
      Serial.println(F("BAUDRATE    (Show the baudrate in each log file)"));
      Serial.println(F("COUNT [abc] (Set the file index to a 3 digit alphanumeric code abc)"));
      Serial.println(F("ID [Vxx]    (Change device version [V] and serial number [xx]; e.g. ID 201 means version 2 serial number 01)"));
    }
    else {
      Serial.println(("Unknown Command"));
    }
    Serial.clear();
  }
  
  // keep watching the push button:
  button.tick();
}


void turn_encrypted_logging_off(){
  EEPROM.put(ENCRYPTED_LOGGING_ADDR,false);
  encrypted_logging = false;
  digitalWrite(BLUE_LED,LOW);
  close_binFile();
}

void turn_encrypted_logging_on(){
  EEPROM.put(ENCRYPTED_LOGGING_ADDR,true);
  encrypted_logging = true;
  digitalWrite(BLUE_LED,HIGH);
  close_binFile();
}
  
/*
 * Print information regarding the SD Card to the Serial port.
 */
void sd_capacity(){
  uint32_t sectors_per_cluster = sd.sectorsPerCluster();
  uint32_t cluster_count = sd.clusterCount();
  uint32_t free_cluster_count = sd.freeClusterCount();
   // Check for free space on the card.
  Serial.print(F("SD Total Cluster Count: "));
  Serial.println(cluster_count);
  Serial.print(F("SD Free Cluster Count: "));
  Serial.println(free_cluster_count);
  Serial.print(F("Sectors per cluster: "));
  Serial.println(sectors_per_cluster);
  Serial.print(F("Free Space (MB): "));
  Serial.println(double(free_cluster_count) * double(sectors_per_cluster) * BUFFER_SIZE / 1000000.0);
  Serial.print(F("Total Space (MB): "));
  Serial.println(double(cluster_count) * double(sectors_per_cluster) * BUFFER_SIZE / 1000000.0 );
}

void turn_requests_on(){
  send_request_timer = 0;
  send_requests = true;
  Can0.setListenOnly(false);
  Can1.setListenOnly(false);
  Serial.println("Send Requests On");
}

void turn_requests_off(){
  send_requests = false;
  send_iso_requests = false;
  Can0.setListenOnly(true);
  Can1.setListenOnly(true);
  Serial.println("Send Requests Off");
}

void turn_recording_on(){
  setup();
  Serial.println("Recording On");
}

void turn_recording_off(){
  recording = false;
  //Serial.println("Recording Off");
}

void turn_streaming_on(){
  stream = true;
  Serial.println("Streaming On");
}

void turn_streaming_off(){
  stream = false;
  //Serial.println("Streaming Off");
}

void display_baud_rate(){
  Serial.print("Can0 bitrate is set to ");
  Serial.println(Can0.baud_rate);
  Serial.print("Can1 bitrate is set to ");
  Serial.println(Can1.baud_rate);
  //Serial.print("Can2 bitrate is set to ");
  //Serial.println("250000"); // Change this once it is adjustable.
}

void display_error_count(){
  Serial.print("Can0 error count is ");
  Serial.println(ErrorCount0);
  Serial.print("Can1 error count is ");
  Serial.println(ErrorCount1);
  //Serial.print("Can2 error count is ");
  //Serial.println(ErrorCount2); 
}

void list_files(){
  sd.ls();     
}

void list_files_a(){
  sd.ls(LS_DATE | LS_SIZE | LS_R);     
}

uint32_t cardSectorCount = 0;
uint8_t  sectorBuffer[BUFFER_SIZE];
//------------------------------------------------------------------------------
// SdCardFactory constructs and initializes the appropriate card.
SdCardFactory cardFactory;
// Pointer to generic SD card.
SdCard* m_card = nullptr;

void format_sd_card(){
  close_binFile();
  sd.end();
  delay(100);
  m_card = cardFactory.newCard(SD_CONFIG);
  if (!m_card || m_card->errorCode()) {    
    Serial.println("card init failed.");
    return;
  }
  ExFatFormatter exFatFormatter;
  exFatFormatter.format(m_card, sectorBuffer, &Serial);//:
  Serial.println(F("Done"));
  delay(100);
  sdErrorFlash();
}
