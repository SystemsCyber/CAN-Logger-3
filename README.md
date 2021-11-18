https://systemscyber.github.io/CAN-Logger-3/

# CAN-Logger-3

An open source secure data logging system for heavy vehicle networks. This project is comprensive in that it covers the following design features:
  1. Hardware electronics design using and ARM Cortex M4F 32-bit processor. This includes schematics, bill of materials, and Altium design files. The design is based on the Teensy 3.6 from PJRC.
  2. The embedded software stack using the Arduino and Teensyduino development environments.
  3. A Python 3 application running PyQT5 to interface with the embedded device.
  4. A microservices-based web backend with Amazon Web Services using Python and the serverless frameworks.

Much of the details in the design and application are found in Duy Van's thesis: https://mountainscholar.org/handle/10217/219549 

## Goals
The end use of the device is to securely log all the vehicle network traffic on a heavy vehicle. Some of the stated goals:
  1. Reliably capture all network data when connected to a vehicle on multiple vehicle networks to include 3 CAN channels and J1708.
  2. Securely store all the collected data in non-volitile memory. This means all the data collected will be encrypted before written to the SD card.
  3. The encryption key will be ephemeral, which means each data set has its own key for each session. 
  4. The ephemeral key is envelope encrypted using unique key generated for each device. If one of these keys is compromized, then only one device is compromised.
  5. The keys used to decrypt the records are accountable to users. This places access controls on the data sets. 
  6. The encrypted data and meta-data are authenticated using a secure hash algorithm (SHA-256) and ellipic-curve digital signature algorithm (ECDSA).
  7. The project makes appropriate use of the ATECC608A ECC CryptoAuthentication module from Microchip.
  8. Provide an accessbile, yet accountable, resource for large amounts of CAN data for the fleet of heavy trucks on the road, along with the maintenance actions and edge cases associated with the network traffic.

## Repository Organization
The repository has the following directories:

*docs:* a directory for the hardware source files, pdf prints for the schematics, harware bill of materials, and other hardware design files. There are also diagrams to aide in understanding intended program flow and photographs of the device in use.

*utilities:* a directory for small snippets of code that demonstrate a function or feature of the project.

*Libraries:* a directory with modified libraries that were copied and modified for use in this project.

*clientApp:* the application to run on the local computer using Python 3.

*tests:* small snippets to test the different features of the hardware.

*serverless:* the directory holding the files needed to stand up the Amazon Web Services using the following AWS services:
  * Lambda - functions as a service that run within containers
  * API Gateway - provides access for users to the microservices hosted ont he AWS servers
  * Cognito -  provides access to user managment and authentication
  * S3 - Amazon's simple storage service that's used to keep the data

*static-web:* the data and files needed to host on a static web service. For this project, the static site is hosted at https://systemscyber.github.io/CAN-Logger-3


The docs folder contains schematics, gerber files, and Altium Designer source files. 



## Acknowledgements

This material is based upon work supported by the National Science Foundation under Grant No. 1715409. This project is in collaboration with the National Motor Freight Traffic Association (NMFTA).
