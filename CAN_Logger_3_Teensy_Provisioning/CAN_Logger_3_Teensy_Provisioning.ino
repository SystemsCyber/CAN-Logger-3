/*
 * NMFTA CAN Logger 3 Project   
 * 
 * Arduino Sketch for provisioning CAN Logger 3 by sending the device serial number and
 * exchanging public keys along with their signatures with the Amazon Web Services server
 * 
 * Written By Duy Van
 * Colorado State University
 * Department of Systems Engineering
 * 
 * 7 Janurary 2019
 * 
 * Released under the MIT License
 *
 * Copyright (c) 2019        Jeremy S. Daily, Duy Van
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
 */

#include <SparkFun_ATECCX08a_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_ATECCX08a
#include <i2c_t3.h> //use to communicate with the ATECC608a cryptographic coprocessor
#include <sha256.h> //Hash device public key 

ATECCX08A atecc;

#define SHA256_BLOCK_SIZE 32          // SHA256 outputs a 32 byte digest
Sha256* sha256Instance;
byte hash[SHA256_BLOCK_SIZE];

byte message[64];
uint8_t server_public_key[64];
uint8_t encrypted_pass[16];
#define GREEN_LED 6
#define RED_LED 14
#define YELLOW_LED 5
#define BLUE_LED 39
String serial_string;

void setup() {
  // put your setup code here, to run once:
  pinMode(GREEN_LED,OUTPUT);
  pinMode(RED_LED,OUTPUT);
  pinMode(YELLOW_LED,OUTPUT);
  pinMode(BLUE_LED,OUTPUT);
  Serial.begin(9600);
  //Initiate ATECC608A connection
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
   digitalWrite(GREEN_LED,HIGH);
   digitalWrite(RED_LED,HIGH);
   digitalWrite(YELLOW_LED,HIGH);
   digitalWrite(BLUE_LED,HIGH);
}

void send_data(){
  atecc.readConfigZone(false); // produces a serial number
  atecc.generatePublicKey(0,false); //compute public key from slot 0 private key
  
  //Send serial number to python through local serial
  for (int n = 0; n < sizeof(atecc.serialNumber);n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",atecc.serialNumber[n]);
    Serial.write(hex_digit);
  }
  Serial.write('\n');
  
  //Send device public key to python through local serial
  for (int n = 0; n < sizeof(atecc.publicKey64Bytes);n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",atecc.publicKey64Bytes[n]);
    Serial.write(hex_digit);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  
  //Wait for Python to initiate the commands
  while(Serial.available() == 0);//wait for Python input
  serial_string = Serial.readStringUntil('\n');

  //Provisioning process
  if (serial_string.equalsIgnoreCase("KEY")) 
  {
    atecc.writeProvisionConfig(); //Write and Lock Configuration made specifically for the CAN Logger 3 application, please see library for more detail
    atecc.lockConfig(); //Lock Configuration zone
    atecc.createNewKeyPair(); //Create ECC key pair on slot 0
    atecc.lockDataSlot(0); //Lock private key on slot 0
    send_data();
    
    //Wait for server to send its public key
    while (Serial.available() == 0);//wait for Python input again
    
    for (int i = 0; i < 64; i++){
      byte c = Serial.read();
      server_public_key[i] = c;
    }
    
    atecc.loadPublicKey(server_public_key,false); //Load the received public key to slot 10 on the ATECC
    atecc.lockDataAndOTP(); //Lock Data and OTP zone in order to read the server public key later for ECDH
    atecc.readPublicKey(false);//Read the stored server public key
    for (int j =0;j<sizeof(atecc.storedPublicKey);j++){
      char hex_digit[3];
      sprintf(hex_digit,"%02X", atecc.storedPublicKey[j]);
      Serial.print(hex_digit);
    }
    
  }

  //Decrypt password process
  if (serial_string.equalsIgnoreCase("PASSWORD")) 
  {
    send_data();

    //Wait for server to send its public key
    while (Serial.available() == 0);//wait for Python input again
    
    for (int i = 0; i < 16; i++){
      byte c = Serial.read();
      encrypted_pass[i] = c;
      }
      
    atecc.readPublicKey(false);
    atecc.ECDH(atecc.storedPublicKey, ECDH_OUTPUT_IN_TEMPKEY,0x0000);
    atecc.AES_ECB_decrypt(encrypted_pass);
    for (int n = 0; n < 16;n++){
    char hex_digit[3];
    sprintf(hex_digit,"%02X",atecc.AES_buffer[n]);
    Serial.write(hex_digit);
  }
    
  }
  
  else {};
}
