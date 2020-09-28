# !/bin/env/python


from PyQt5.QtWidgets import (QMainWindow,
                             QWidget,
                             QTreeView,
                             QMessageBox,
                             QFileDialog,
                             QLabel,
                             QSlider,
                             QCheckBox,
                             QLineEdit,
                             QVBoxLayout,
                             QApplication,
                             QPushButton,
                             QTableWidget,
                             QTableView,
                             QTableWidgetItem,
                             QScrollArea,
                             QAbstractScrollArea,
                             QAbstractItemView,
                             QSizePolicy,
                             QGridLayout,
                             QGroupBox,
                             QComboBox,
                             QAction,
                             QDockWidget,
                             QDialog,
                             QFrame,
                             QDialogButtonBox,
                             QInputDialog,
                             QProgressDialog,
                             QTabWidget,
                             QFormLayout,
                             QPlainTextEdit,
                             QLineEdit,
                             QComboBox)
from PyQt5.QtCore import Qt, QTimer, QAbstractTableModel, QCoreApplication, QSize
from PyQt5.QtGui import QIcon

# import boto3
# import botocore
# from botocore.config import Config

import requests
import threading
import queue
import datetime
import time
import base64
import zipfile
import sys
import struct
import json
import random
import os
import traceback
import logging
import csv



sys.path.insert(1, '../serverless')
from utils import verify_meta_data_text, decode_jwt

import serial
import serial.tools.list_ports

from ecdsa import VerifyingKey, BadSignatureError, NIST256p
import hashlib
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend

logger = logging.getLogger()
logger.addHandler(logging.StreamHandler(sys.stdout))
logger.setLevel(logging.DEBUG)

AWS_REGION = "us-east-2"
API_ENDPOINT = r"https://47tzdaoo6k.execute-api.us-east-2.amazonaws.com/dev/"
APP_CLIENT_ID = "58tl1drhvqtjkmhs69inh7l1t3"
USER_POOL_ID = "us-east-2_fiNazAdBU"
IDENTITY_TOKEN_NAME = "identity_token.json"
ACCESS_TOKEN_NAME = "access_token.json"

class SerialListener(threading.Thread):
    def __init__(self, rx_queue, serial_port):
        threading.Thread.__init__(self)
        self.rx_queue = rx_queue
        self.ser = serial_port
        self.ser.timeout = None
        self.runSignal = True
        logger.debug("Started Serial Listening Thread on {}".format(self.ser.port))

    def run(self):
        while self.runSignal:
            i = max(1, min(2048, self.ser.in_waiting))
            data = self.ser.read(i)
            print(data)
            self.rx_queue.put(data) 
            # if len(data) > 1:
            #     print(data)
            #     for b in data:
            #         self.rx_queue.put(bytes(b))
            # else:
            #     self.rx_queue.put(data)    

        logger.debug("Serial Listener Thread is finished.")

class CANLogger(QMainWindow):
    def __init__(self):
        super(CANLogger, self).__init__()
        self.home_directory = os.getcwd()
        try:
            self.API_KEY = os.environ["CANLogger_API_KEY"]
        except:
            logger.critical(traceback.format_exc())
            QMessageBox.warning(self,"Missing API Key","Please contact Jeremy Daily at Colorado State University to obtain an API key for this application.")
            sys.exit()
        self.statusBar().showMessage("Welcome to the CANLogger App.")

         # Build common menu options
        menubar = self.menuBar()

        #####################
        # USER
        #####################
        user_menu = menubar.addMenu('&User')
        logger_menu = menubar.addMenu('&Logger')
        server_menu = menubar.addMenu('&Server')
        util_menu = menubar.addMenu('U&tility')

        user_toolbar = self.addToolBar("User")
        logger_toolbar = self.addToolBar("Logger")
        server_toolbar = self.addToolBar("Server")
        util_toolbar = self.addToolBar("Utility")
        
        login = QAction(QIcon(r'icons/new_icon.png'), '&Login', self)
        login.setShortcut('Ctrl+L')
        login.setStatusTip('User login.')
        login.triggered.connect(self.login)
        user_menu.addAction(login)
        user_toolbar.addAction(login)

        hello = QAction(QIcon(r'icons/test_icon.png'), 'Connection &Test', self)
        hello.setShortcut('Ctrl+H')
        hello.setStatusTip('Test Endpoint Authorization.')
        hello.triggered.connect(self.hello)
        user_menu.addAction(hello)
        user_toolbar.addAction(hello)

        #####################
        # LOGGER
        #####################
        connect_logger = QAction(QIcon(r'icons/connect_icon.png'), 'Connect To &Logger', self)
        connect_logger.setShortcut('Ctrl+L')
        connect_logger.setStatusTip('Connect a CAN Logger through USB.')
        connect_logger.triggered.connect(self.connect_logger_by_usb)
        logger_menu.addAction(connect_logger)
        logger_toolbar.addAction(connect_logger)

        save_file = QAction(QIcon(r'icons/save_icon.png'), '&Save File', self)
        save_file.setShortcut('Ctrl+S')
        save_file.setStatusTip('Save Local Copy of the File.')
        save_file.triggered.connect(self.save_file)
        logger_menu.addAction(save_file)
        logger_toolbar.addAction(save_file)

        new_file = QAction(QIcon(r'icons/upload_icon.png'), '&Upload File', self)
        new_file.setShortcut('Ctrl+U')
        new_file.setStatusTip('Upload current file.')
        new_file.triggered.connect(self.upload_file)
        logger_menu.addAction(new_file)
        logger_toolbar.addAction(new_file)

        get_key = QAction(QIcon(r'icons/get_key.png'), 'Get &Key/Decrypt File', self)
        get_key.setShortcut('Ctrl+K')
        get_key.setStatusTip('Get the plaintext session key and decrypt log file.')
        get_key.triggered.connect(self.get_session_key)
        logger_menu.addAction(get_key)
        logger_toolbar.addAction(get_key)

        format_logger = QAction(QIcon(r'icons/format_icon.png'), '&Format SD', self)
        format_logger.setShortcut('Ctrl+F')
        format_logger.setStatusTip('Format the SD Card on the Data Logger')
        format_logger.triggered.connect(self.format_sd_card)
        logger_menu.addAction(format_logger)
        logger_toolbar.addAction(format_logger)

        #####################
        # Server
        #####################
        connect_server = QAction(QIcon(r'icons/connect_server_icon.png'), 'Connect To &Server', self)
        connect_server.setShortcut('Ctrl+S')
        connect_server.setStatusTip('Connect to server and get files.')
        connect_server.triggered.connect(self.list_server_file)
        server_menu.addAction(connect_server)
        server_toolbar.addAction(connect_server)

        server_file = QAction(QIcon(r'icons/download_icon.png'), '&Download File', self)
        server_file.setShortcut('Ctrl+D')
        server_file.setStatusTip('Download the selected log file from server.')
        server_file.triggered.connect(self.download_server_file)
        server_menu.addAction(server_file)
        server_toolbar.addAction(server_file)

        share = QAction(QIcon(r'icons/share_icon.png'), '&Share Access', self)
        share.setShortcut('Ctrl+A')
        share.setStatusTip('Give file access to specific user.')
        share.triggered.connect(self.share_access)
        server_menu.addAction(share)
        server_toolbar.addAction(share)

        read_info = QAction(QIcon(r'icons/read_icon.png'), '&Read File Info', self)
        read_info.setShortcut('Ctrl+R')
        read_info.setStatusTip('Read user note and download log from selected file.')
        read_info.triggered.connect(self.read_file_info)
        server_menu.addAction(read_info)
        server_toolbar.addAction(read_info)

        #####################
        # UTILITY
        #####################
        get_password = QAction(QIcon(r'icons/get_password.png'), 'Get &Password', self)
        get_password.setShortcut('Ctrl+P')
        get_password.setStatusTip('Decrypt the server private key password.')
        get_password.triggered.connect(self.decrypt_password)
        util_menu.addAction(get_password)
        util_toolbar.addAction(get_password)

        provision_logger = QAction(QIcon(r'icons/provision_icon.png'), '&Provision', self)
        provision_logger.setShortcut('Ctrl+V')
        provision_logger.setStatusTip('Register important data with the server.')
        provision_logger.triggered.connect(self.provision)
        util_menu.addAction(provision_logger)
        #util_toolbar.addAction(provision_logger)

        default_access = QAction(QIcon(r'icons/list_icon.png'), '&Save Default Access List', self)
        default_access.setShortcut('Ctrl+E')
        default_access.setStatusTip('Create a default access list from input emails for file upload.')
        default_access.triggered.connect(self.save_default_access)
        util_menu.addAction(default_access)
        util_toolbar.addAction(default_access)


        self.setWindowTitle("CAN Logger Client Application")
        
        self.meta_data_dict       = None
        self.server_meta_data_dict= None
        self.access_token         = None
        self.identity_token       = None
        self.refresh_token        = None
        self.connected            = False
        self.encrypted_log_file   = None
        self.session_key          = None
        self.connection_type      = None
        self.list_file 			  = True

        self.character1 = ' '
        self.character2 = ','


        initial_message = QLabel("Connect to a CAN Logger (Ctrl+L) or to AWS server (Ctrl+S) to see files.")
        self.grid_layout = QGridLayout()
        self.grid_layout.addWidget(initial_message,0,0,1,1)
        main_widget = QWidget()
        main_widget.setLayout(self.grid_layout)
        self.setCentralWidget(main_widget)
        self.setWindowIcon(QIcon(r'icons/csu_icon.png'))

        self.show() 
        if not self.load_tokens():
            self.login()
    


    def provision(self):
        buttonReply = QMessageBox.question(self,"Provision Process",
            "Are you performing provisioning and does your device has the provisioning firmware?\nPlease unplug and replug the device.",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if buttonReply == QMessageBox.Yes:
            self.list_file = False
            while not self.connected:
                if self.connect_logger_by_usb() is None:
                    return
            self.list_file = True
            # empty the queue
            while not self.serial_queue.empty():
                self.serial_queue.get_nowait()
            time.sleep(0.05)
            self.ser.write(b'KEY\n')
            time.sleep(0.5)
            
            start_time = time.time()
            ret_val = b''
            while len(ret_val) < (128+18+1): # Key Length, Serial Number, newline
                if (time.time() - start_time) > 1:
                    break
                character = self.serial_queue.get()
                ret_val += character
                print(ret_val)
            
            response=ret_val.split(b'\n')
            logger.debug(response)
            try:
                serial_number = response[0]
                device_public_key = response[1]
                if(len(response[len(response)-2]) == 18):
                    serial_number = response[len(response)-2]
                else:
                    QMessageBox.warning(
                        self, "Key Error", "There was an issue getting the Key over serial.")
                    return
                if(len(response[len(response)-1]) == 128):
                    device_public_key = response[len(response)-1]
                else:
                    QMessageBox.warning(
                        self, "Key Error", "There was an issue getting the Key over serial.")
                    return
                print("Serial ID", serial_number)
                print(device_public_key)
            except IndexError:
                QMessageBox.warning(self,"Key Error","There was an issue getting the Key over serial.")
                return

            device_label, ok = QInputDialog.getText(self, 'Device Label',
                                        'Enter 5 characters for the device label:',
                                        QLineEdit.Normal,
                                        "CSUXX"
                                        )
            if not ok:
                return
            try:
                self.device_label_g = device_label[:5]
                data = {'serial_number': base64.b64encode(serial_number).decode("ascii"),
                        'device_label': device_label[:5],
                        'device_public_key': base64.b64encode(device_public_key).decode("ascii"),
                       }
            except TypeError:
                logger.warning("Must have data to get key.")
                return
            url = API_ENDPOINT + "provision"
            header = {}
            header["x-api-key"] = self.API_KEY #without this header, the API Gateway will return a 403: Forbidden message.
            header["Authorization"] = self.identity_token #without this header, the API Gateway will return a 401: Unauthorized message
            try:
                r = requests.post(url, json=data, headers=header)
            except requests.exceptions.ConnectionError:
                QMessageBox.warning(self,"Connection Error","The there was a connection error when connecting to\n{}\nPlease try again once connection is established".format(url))
                return
            print(r.status_code)
            print(r.text)
            if r.status_code == 200: #This is normal return value
                data_dict = r.json()
                server_public_key=base64.b64decode(data_dict["server_public_key"]).hex().upper()
                server_pem_key_pass=base64.b64decode(data_dict["server_pem_key_pass"]).decode('ascii')
                encrypted_rand_pass=data_dict["encrypted_rand_pass"] #base64 format in string type
                self.server_pem = server_pem_key_pass
                self.rand_pass = encrypted_rand_pass
                self.serial_id = serial_number.decode('ascii')


                assert len(server_public_key)==128
                print("uint8_t server_public_key[64] = {")
                for i in range(0,len(server_public_key),2):
                    print("0x{}{},".format(server_public_key[i],server_public_key[i+1]),end='')
                print("};")

                # Visual key hash confirmation before sending the server public key to the device
                device_pub_key_bytes = bytearray.fromhex(device_public_key.decode("ascii"))
                server_public_key_bytes = base64.b64decode(data_dict["server_public_key"])   
                device_public_key_hash = hashlib.sha256(device_pub_key_bytes).digest().hex().upper()
                server_public_key_hash = hashlib.sha256(server_public_key_bytes).digest().hex().upper()
                
                #Key Comparision 
                buttonReply = QMessageBox.question(self, 'Do the keys match?', "Device Serial Number: {}\nDevice public key provisioning hash: {}\nServer public key provisioning hash: {}".format(serial_number.decode('ascii'),device_public_key_hash[:10],server_public_key_hash[:10]), QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
                if buttonReply == QMessageBox.Yes:
                    self.ser.write(bytearray.fromhex(server_public_key))
                    time.sleep(1)
                    ret_val = b''
                    while not self.serial_queue.empty():
                        character = self.serial_queue.get()
                        ret_val += character
                    if ret_val == bytes(server_public_key,'ascii'):
                        QMessageBox.information(self,"Provisioning Process","Server Public Key has been stored and locked in device {}".format(self.serial_id))
                        self.ask_to_save()
                    else:
                        QMessageBox.warning(self,"Error","Key is already locked!")

                else:
                    QMessageBox.warning(self,"Keys do not match","Server public key has NOT been stored and locked!\nDelete the device info in CANLoggers table and provision again.")
            
            else:
                QMessageBox.warning(self,"Error",r.text)

    #Ask the operator if they want to save the server_pem_key and encrypted_rand_pass
    def ask_to_save(self):
        buttonReply = QMessageBox.question(self, 'Save File', "Would you like to save the server private key and its encrypted password?", QMessageBox.Yes | QMessageBox.No, QMessageBox.Yes)
        if buttonReply == QMessageBox.Yes:
            self.save_security_list()


    def save_security_list(self):
        #Save the server pem key with pass and encrypted password to a text file
        options = QFileDialog.Options()
        options |= QFileDialog.Detail
        self.data_file_name, data_file_type = QFileDialog.getSaveFileName(self,
                                            "Save File",
                                                                          self.home_directory + "/" + "CAN Logger 3 Security List " + self.device_label_g,
                                            "JSON Files (*.json);;All Files (*)",
                                            options = options)
        if self.data_file_name:
            if os.path.exists(self.data_file_name) == True:
            #if os.path.getsize(self.data_file_name) >0:
                with open(self.data_file_name,'r') as file:
                    data = json.load(file)
                data[self.serial_id] = {'server_pem_key':self.server_pem,'encrypted_password':self.rand_pass}
                with open(self.data_file_name,'w') as file:
                    json.dump(data,file, indent=4)
                
            else:
                with open(self.data_file_name,'w') as file:
                    data = {self.serial_id:{'server_pem_key':self.server_pem,'encrypted_password':self.rand_pass}}
                    json.dump(data,file, indent=4)
            QMessageBox.information(self,"Save File","File is successfully saved!")
                

    #Send the encrypted server pem key password to the device for encryption
    #Must be done after the provisioning process
    def decrypt_password(self):
        QMessageBox.information(self,"Deccrypt Encrypted Password","Make sure your device has Provisioning firmware.\nPlease choose the security list JSON file from Provisioning step.")
        options = QFileDialog.Options()
        options |= QFileDialog.Detail
        self.data_file_name, data_file_type = QFileDialog.getOpenFileName(self,
                                            "Open JSON File",
                                            self.home_directory,
                                            "JSON Files (*.json);;All Files (*)",
                                            options = options)

        if self.data_file_name:
            with open(self.data_file_name,'r') as file:
                data = json.load(file)

            self.list_file = False
            #Open serial COM port if not connected
            while not self.connected:
                if self.connect_logger_by_usb() is None:
                    return
            self.list_file = True
            # empty the queue
            while not self.serial_queue.empty():
                self.serial_queue.get_nowait()
            time.sleep(0.5)
            self.ser.write(b'PASSWORD\n')
            time.sleep(0.5)
            
            ret_val = b''
            while not self.serial_queue.empty():
                character = self.serial_queue.get()
                ret_val += character
            response = ret_val.split(b'\n')
            serial_number =response[0].decode('ascii')
            encrypted_pass = base64.b64decode(bytes(data[serial_number]["encrypted_password"],'ascii'))
            print(encrypted_pass)
            self.ser.write(encrypted_pass)
            time.sleep(1)

            ret_val = b''
            while not self.serial_queue.empty():
                character = self.serial_queue.get()
                ret_val += character
            typable_pass = bytes.fromhex(ret_val.decode('ascii')).decode('ascii')

            #Display the decrypted password
            msg=QMessageBox(self)
            msg.setIcon(QMessageBox.Information)
            msg.setText("The plain text password is:\n{}".format(typable_pass))
            msg.setWindowTitle("Decrypt Password")
            msg.setTextInteractionFlags(Qt.TextSelectableByMouse)
            msg.exec_()



    def get_session_key(self):

        if self.connection_type != 'USB':
            QMessageBox.warning(self,"Connection Type","Please connect a device and select a file.")
            return

        if self.meta_data_dict is None:
            QMessageBox.warning(self,"Select File","Please connect a device and select a file.")
            return

        if not decode_jwt(self.identity_token):
            message = "A valid webtoken is not available to get data. Please login."
            logger.warning(message)
            QMessageBox.warning(self,"Invalid Token",message)
            return


        url = API_ENDPOINT + "auth"
        header = {}
        header["x-api-key"] = self.API_KEY #without this header, the API Gateway will return a 403: Forbidden message.
        header["Authorization"] = self.identity_token #without this header, the API Gateway will return a 401: Unauthorized message
        try:
            data = {'serial_number': self.meta_data_dict["serial_num"],
                'digest': base64.b64decode(self.meta_data_dict["binary_sha_digest"]).hex().upper()
               }
            logger.debug(data)
        except TypeError:
            logger.warning("Must have data to get key.")
            return
        except KeyError:
            logger.warning("Must upload file first.")
            return
        try:
            r = requests.post(url, json=data, headers=header)
        except requests.exceptions.ConnectionError:
            QMessageBox.warning(self,"Connection Error","The there was a connection error when connecting to\n{}\nPlease try again once connection is established".format(url))
            return
        print(r.status_code)
        print(r.text)
        if r.status_code == 200: #This is normal return value
            self.session_key = base64.b64decode(r.text)
            print("session_key = {}".format(self.session_key))
            session_key_hex = self.session_key.hex().upper()

            #Display the AES session key for user
            msg=QMessageBox(self)
            msg.setIcon(QMessageBox.Information)
            msg.setText("The Session Key was recovered from the secure server.\nThe AES session key for the file is:\n{}".format(session_key_hex))
            msg.setWindowTitle("Session Key")
            msg.setTextInteractionFlags(Qt.TextSelectableByMouse)
            msg.exec_()

            self.cont = True
            buttonReply = QMessageBox.question(self,"Log File","Do you want to save the decrypted version of selected file?", QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
            if buttonReply == QMessageBox.Yes:
                if not os.path.exists('Log Files/'+ self.meta_data_dict['filename']):
                    QMessageBox.information(self,"File Info","{} will need to be downloaded to your PC for decryption.".format(self.meta_data_dict['filename']))
                self.download_file()
                if self.download_status == False:
                    return

                self.decrypt_file()

                if self.cont == False:
                    return
                #Save decrypted file
                options = QFileDialog.Options()
                options |= QFileDialog.Detail
                self.data_file_name, data_file_type = QFileDialog.getSaveFileName(self,
                                                    "Save File",
                                                    self.home_directory + "/" + self.meta_data_dict["filename"][:-4] + "_plaintext",
                                                    "BIN Files (*.bin);;All Files (*)",
                                                    options = options)
                if self.data_file_name:
                    with open(self.data_file_name, 'wb') as file:
                        file.write(self.decrypted_log)
                        file.close()
        else:
            QMessageBox.information(self,"Error","The server returned a status code {}.\n{}".format(r.status_code,r.text))  

    def format_sd_card(self):
        buttonReply = QMessageBox.question(self,"Are you sure?","Formatting will erase all the data on the SD Card. ", QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if buttonReply == QMessageBox.Yes:
        	#Open serial COM port if not connected
            self.list_file = False
            while not self.connected:
                if self.connect_logger_by_usb() is None:
                    return
            time.sleep(0.1)
            self.ser.write(b'0\n')
            time.sleep(0.1)
            self.list_file = True
            # empty the queue
            while not self.serial_queue.empty():
                self.serial_queue.get_nowait()
            time.sleep(0.5)
            self.ser.write(b'FORMAT\n')
            time.sleep(0.5)
            ret_val = b''
            while not self.serial_queue.empty():
            	character = self.serial_queue.get()
            	ret_val += character
            if len(ret_val) == 102:
            	QMessageBox.information(self,"Format SD Card", "SD card is successfully formatted!")
            else:
            	QMessageBox.warning(self,"Format SD Card", "Fail to format SD card!")
        else:
            return
            

    def decrypt_file(self):
        if self.session_key is None:
            logger.debug("Decryption Needs a Session Key")

        # Calculate SHA of data
        # compare SHA If SHA is the same, Proceed
        #logger.debug(self.meta_data_dict["init_vect"])
        iv = base64.b64decode(self.meta_data_dict["init_vect"])
        #Change this. The key is hard coded for testing
        #self.session_key = bytearray.fromhex('CB3944D1881FB2A0AFF350D51FB0D802')
        cipher = Cipher(algorithms.AES(self.session_key), 
                        modes.CBC(iv), 
                        backend=default_backend())
        decryptor = cipher.decryptor()
        self.decrypted_log = decryptor.update(self.encrypted_log_file) + decryptor.finalize()
        #logger.debug("Decrypted Log: {}".format(self.decrypted_log[:1024]))
        logger.debug(len(self.decrypted_log))


    def connect_logger_by_usb(self):
        self.connection_type = 'USB'

        items =[] 
        for device in serial.tools.list_ports.comports():
            items.append("{} - {}".format(device.device, device.description))
        logger.debug(items)
        com_port, okPressed = QInputDialog.getItem(self, "Select COM Port","CAN Logger USB Serial Port:", items, 0, False)
        if okPressed and com_port:
            logger.debug("Selected: {}".format(com_port))
            self.comport = com_port.split('-')[0].strip()
        else:
            return
        logger.debug("Trying to connect USB serial.")
        try:
            self.ser.close()
            del self.ser
        except AttributeError:
            pass

        try:
            self.ser = serial.Serial(self.comport, timeout=0.1)
            self.ser.set_buffer_size(rx_size = 2048, tx_size = 2000)
            self.connected = True
            logger.debug("Connected to Serial Port.")
            self.serial_queue = queue.Queue()
            self.serial_thread = SerialListener(self.serial_queue,self.ser)
            self.serial_thread.setDaemon(True) #needed to close the thread when the application closes.
            self.serial_thread.start()
            logger.debug("Started Serial Thread.")

            if self.list_file:
            	self.list_device_files()
            return True
        except serial.serialutil.SerialException:
            logger.debug(traceback.format_exc())
            self.connected = False
            if "PermissionError" in repr(traceback.format_exc()):
                QMessageBox.information(self,"USB Status","The port {} is already in use. Please unplug and replug the unit.".format(self.comport))
            else:
                self.connected = False
                return False
    
    def download_file(self):
        row = self.device_file_table.currentRow()
        filename = str(self.device_file_table.item(row, 3).text()) # select the filename entry
        expected_size = int(self.device_file_table.item(row, 8).text())

        download = True 
        self.download_status = True

        if not os.path.exists('Log Files/'+filename):
            if expected_size >26214400:
            	buttonReply = QMessageBox.question(self,"File Information","File size is more than 25 MB!\nTransferring {} through device serial may take up to 15 minutes.\nPress Yes to continue or No to transfer through SD card to your local PC yourself.".format(filename),QMessageBox.Yes | QMessageBox.No|QMessageBox.Cancel, QMessageBox.No)

            	if buttonReply == QMessageBox.No:
            		QMessageBox.information(self,"File Information","Please copy {} to {}.\nPress OK when file is in directory.".format(filename, self.home_directory + "\\Log Files\\"))
            		download = False
            	elif buttonReply == QMessageBox.Yes:
            		download = True
            	else:
            		self.download_status = False
            		return
        else:
            download = False


        if download == True:
	        logger.debug("Downloading file {}".format(filename))
	        # empty the queue
	        while not self.serial_queue.empty():
	            self.serial_queue.get_nowait()
	        time.sleep(0.1)

	        self.ser.write(b'BIN ' + bytes(filename,'ascii') + b'\n')
	        time.sleep(0.5)
	        ret_val = b''
	        #start_time = time.time()
	        #timeout = 1000
	        count = 0
	        
	        if not os.path.exists('Log Files'):
	            os.makedirs('Log Files')

	        try:
	            with open (self.home_directory + '/Log Files/' + self.meta_data_dict['filename'],'wb') as file:
	                try:
	                    while count < expected_size:
	                        character = self.serial_queue.get()
	                        file.write(character)
	                        count += len(character)
	                        print(count/expected_size*100)
	                    file.close()
	                except:
	                    traceback.format_exc()   
	                #current_time = (time.time() - start_time)
	                #if  current_time > timeout:
	                #    logger.debug("Download timed out.")
	                #    break
	                    
	        except: 
	            logger.debug(traceback.format_exc())


        try:
	        with open(self.home_directory + '/Log Files/' + self.meta_data_dict['filename'],'rb') as file:
	            ret_val = file.read()
        except FileNotFoundError: #If file is not on local PC
            QMessageBox.warning(self,"Error","There is no {} in {}!".format(filename, self.home_directory + '\\Log Files\\'))
            self.download_status = False
            return


        downloaded_size = len(ret_val)
        logger.debug("Downloaded {} bytes of {}".format(downloaded_size,expected_size))
        self.encrypted_log_file = ret_val
        
        downloaded_bin_hash = hashlib.sha256(self.encrypted_log_file).digest()
        logger.debug("Calculated Hash:\n{}".format(downloaded_bin_hash))

        stored_hash = base64.b64decode(self.meta_data_dict["binary_sha_digest"])
        logger.debug("Stored Hash:\n{}".format(stored_hash))

        if downloaded_bin_hash == stored_hash:
            logger.debug("SHA-256 digests match. Log File is authenticate.")
        else:
            logger.debug("Mismatch of SHA-256 digests. Log File is not authenticated.")
            self.download_status = False
            QMessageBox.warning(self,"Error","SHA hash of {} in directory does not match with hash from metadata!\nPlease delete the file and download the file again.".format(self.meta_data_dict['filename']))

        
    
    def load_meta_data(self):
        self.meta_data_dict = {}
        row = self.device_file_table.currentRow()
        self.meta_data_dict["datetime"] = str(self.device_file_table.item(row, 0).text()) #
        self.meta_data_dict["CAN0"] = int(self.device_file_table.item(row, 1).text()) # select the filename entry
        self.meta_data_dict["CAN1"] = int(self.device_file_table.item(row, 2).text()) # select the filename entry
        self.meta_data_dict["filename"] = str(self.device_file_table.item(row, 3).text()) #
        self.meta_data_dict["serial_num"] = str(self.device_file_table.item(row, 4).text()) #
        self.meta_data_dict["init_vect"] = base64.b64encode(bytearray.fromhex(str(self.device_file_table.item(row, 5).text()))).decode('ascii') #string of base64 encoded bytes
        self.meta_data_dict["session_key"] = base64.b64encode(bytearray.fromhex(str(self.device_file_table.item(row, 6).text()))).decode('ascii') #string of base64 encoded bytes
        self.meta_data_dict["device_public_key"] = base64.b64encode(bytearray.fromhex(str(self.device_file_table.item(row, 7).text()))).decode('ascii') #string of base64 encoded bytes
        self.meta_data_dict["filesize"] = int(self.device_file_table.item(row, 8).text()) #
        self.meta_data_dict["binary_sha_digest"] = base64.b64encode(bytearray.fromhex(str(self.device_file_table.item(row, 9).text()))).decode('ascii') #string of base64 encoded bytes
        self.meta_data_dict["text_sha_digest"] = base64.b64encode(bytearray.fromhex(str(self.device_file_table.item(row, 10).text()))).decode('ascii') #string of base64 encoded bytes
        self.meta_data_dict["signature"] = base64.b64encode(bytearray.fromhex(str(self.device_file_table.item(row, 11).text()))).decode('ascii') #string of base64 encoded bytes
        self.meta_data_dict["base64"] = str(self.device_file_table.item(row, 13).text())
        for k,v in self.meta_data_dict.items():
            logger.debug("{}: {}".format(k,v))

    def list_device_files(self):
        while not self.connected:
            if self.connect_logger_by_usb() is None:
                return
        time.sleep(0.1)
        self.ser.write(b'0\n')
        time.sleep(0.1)
        # empty the queue
        while not self.serial_queue.empty():
            self.serial_queue.get_nowait()
        time.sleep(0.1)
        self.ser.write(b'LS A\n')
        time.sleep(0.1)
        file_meta_data_list=[]
        file_raw_meta_data_list=[]
        
        ret_val = b''
        while not self.serial_queue.empty():
            character = self.serial_queue.get()
            ret_val += character
        
        response=ret_val.split(b'\r\n')
        print("LS A:")
        logger.debug(response)
        
        for line in response:
            try:
                logger.debug(line)
                line_data = line.strip().split()
                latest_file_name = str(line_data[3],'ascii')
                latest_file_size = int(line_data[2])
                if latest_file_name[-3:] == 'txt' and latest_file_size > 500:
                    file_meta_data_list.append(line_data)
            except IndexError:
                logger.debug(traceback.format_exc())

        header_labels = ["Date & Time",
                         "CAN0 Bitrate",
                         "CAN1 Bitrate",
                         "Filename",
                         "Logger Serial Number",
                         "Initialization Vector",
                         "Encrypted Session Key",
                         "Device Public Key",
                         "Size",
                         "Binary File SHA-256 Hash Digest",
                         "Text File SHA-256 Hash Digest",
                         "Digital Signature of Text SHA Digest",
                         "Verification",
                         "Base64 encoded meta data"]
        NUM_COLS = len(header_labels)
        NUM_ROWS = len(file_meta_data_list)
        self.device_file_table = QTableWidget(NUM_ROWS,NUM_COLS,self)
        self.device_file_table.setHorizontalHeaderLabels(header_labels)
        self.device_file_table.setSelectionBehavior(QTableView.SelectRows);
        self.device_file_table.doubleClicked.connect(self.download_file)
        self.device_file_table.itemSelectionChanged.connect(self.load_meta_data)
        self.device_file_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.grid_layout.addWidget(self.device_file_table ,0,0,1,1)
        row = 0

        #Add progress bar
        if len(file_meta_data_list) !=0:
	        loading_progress = QProgressDialog(self)
	        loading_progress.setMinimumWidth(300)
	        loading_progress.setWindowTitle("Retrieving {} Log Files Metadata".format(len(file_meta_data_list)))
	        loading_progress.setLabelText("This may take a while...")
	        loading_progress.setMinimumDuration(0)
	        loading_progress.setMaximum(len(file_meta_data_list))
	        loading_progress.setWindowModality(Qt.ApplicationModal)
        
        index = 0

        for line_data in file_meta_data_list:
            index +=1
            loading_progress.setValue(index)
            if loading_progress.wasCanceled():
            	break

            latest_file_name = str(line_data[3],'ascii')
            latest_file_size = int(line_data[2])
            # empty the queue
            time.sleep(.1)
            while not self.serial_queue.empty():
                self.serial_queue.get_nowait()
            logger.debug("Downloading {} bytes from {}".format(latest_file_size,latest_file_name))    
            #send the command to the Logger to download data
            self.ser.write(b'BIN ' + bytes(latest_file_name[:-3]+"txt",'ascii') + b'\n') 
            time.sleep(0.1);
            ret_val=b''
            while not self.serial_queue.empty():
                character = self.serial_queue.get()
                ret_val += character    
            bytes_in_file = ret_val[:latest_file_size]
            if verify_meta_data_text(bytes_in_file):
                self.meta_data = bytes_in_file.decode('ascii','replace').split(",")
                self.meta_data.append("Verified")
                self.meta_data.append(base64.b64encode(bytes_in_file).decode('ascii','replace'))
                logger.debug("file meta_data has {} elements:".format(len(self.meta_data))) 
                logger.debug("{}".format(self.meta_data))
                # Insert the time first, because it has a bunch of colons 
                col = 0
                self.device_file_table.setItem(row,col,QTableWidgetItem(self.meta_data[0]))
                col += 1
                for entry in self.meta_data[col:]:
                    try:
                        d = entry.split(":")[1] #Try to take the value (i.e. everything after the colon) as opposed to the label
                    except IndexError:
                        d = entry #There was no colon, so 
                    self.device_file_table.setItem(row,col,QTableWidgetItem(d))
                    col+=1
                row+=1

            else:
                self.device_file_table.removeRow(row)
                logger.debug("Removing row {}".format(row))
                continue    
        self.device_file_table.resizeColumnsToContents()
        self.device_file_table.setSortingEnabled(True)
        return
         

    def hello(self):
        """
        This is a test routine to test the connectivity and authentication for the AWS API.
        Success brings up a dialog box when the correct API key is used.
        """
        # define the endpoint URL corresponding the the AWS API Gateway
        url = API_ENDPOINT + "hello"
        header = {"x-api-key": self.API_KEY}
        try:
            r = requests.get(url, headers=header)
        except requests.exceptions.ConnectionError:
            QMessageBox.warning(self,"Connection Error","The there was a connection error when connecting to\n{}\nPlease try again once connection is established".format(url))
            return
        logger.debug(r.status_code)
        if r.status_code == 200: #This is normal return value    
            logger.debug(r.json())
            QMessageBox.information(self,"Success","The server responded with code 200:\n{}".format(r.json()))
        else: #Something went wrong
            logger.debug(r.text)
            QMessageBox.warning(self,"Connection Error","The there was an error:\n{}".format(r.text))


    def login(self):
        """
        Get a password from the user with a dialog box and submit it.
        Returns a token for further user authentication.
        """
        try:
            with open('username.txt','r') as f:
                stored_user = f.read()
            username_saved = True
        except:
            stored_user = ''
            username_saved = False
        self.user, okPressed = QInputDialog.getText(self, "Username","Username (e-mail):", QLineEdit.Normal, stored_user)
        
        # validate input
        if not okPressed:
            return
        if self.user == '':
            return
        
        if stored_user == self.user:
            username_saved == True
        else:
            username_saved == False

        try:
            with open('password.txt','r') as f:
                stored_password = f.read()
            password_saved = True
        except:
            stored_password = ''
            password_saved = False
        password, okPressed = QInputDialog.getText(self, "Password","Input password for \n{}".format(self.user), QLineEdit.Password, stored_password)
        
        # Validate Input
        if not okPressed:
            return
        if password == '':
            return
        
        if stored_password == password:
            password_saved == True
        else:
            password_saved == False

        #https://boto3.amazonaws.com/v1/documentation/api/latest/reference/services/cognito-idp.html#CognitoIdentityProvider.Client.initiate_auth
        post_data={
            "AuthParameters" : {
                "USERNAME" : self.user,
                "PASSWORD" : password
               },
               "AuthFlow" : "USER_PASSWORD_AUTH",
               "ClientId" : APP_CLIENT_ID
            }
        url = "https://cognito-idp.us-east-2.amazonaws.com"
        header = {}
        header["Content-Type"]= "application/x-amz-json-1.1"
        header["X-Amz-Target"]= "AWSCognitoIdentityProviderService.InitiateAuth"
        try:
            r = requests.post(url, json=post_data, headers=header)
        except requests.exceptions.ConnectionError:
            QMessageBox.warning(self,"Connection Error","The there was a connection error when connecting to\n{}\nPlease try again once connection is established".format(url))
            return
        logger.debug(r.status_code)
        if r.status_code == 200: #This is normal return value
            response_data = r.json()
            for k,v in response_data["AuthenticationResult"].items():
                logger.debug("{}: {}".format(k,v))
            self.access_token = response_data["AuthenticationResult"]["AccessToken"]
            with open(ACCESS_TOKEN_NAME,'w') as fp:
                json.dump(self.access_token,fp)
            self.identity_token = response_data["AuthenticationResult"]["IdToken"]
            with open(IDENTITY_TOKEN_NAME,'w') as fp:
                json.dump(self.identity_token,fp)
            self.refresh_token = response_data["AuthenticationResult"]["RefreshToken"]
            self.load_tokens()

            if not (password_saved and username_saved):
                _password, okPressed = QInputDialog.getText(self, "Save Password","DANGER: Do you want to save your clear text username and password for \n{}? Press OK to save.".format(self.user), QLineEdit.Password, password)
                if okPressed:
                    with open('password.txt','w') as f:
                        f.write(password)
                    with open('username.txt','w') as f:
                        f.write(self.user) 
        elif r.status_code == 400: #Incorrect username or password
            message = r.json()["message"]
            logger.warning(message)
            QMessageBox.warning(self,"Incorrect Username or Password",message)
            self.login()
        else: #Something went wrong
            logger.warning("There was an issue with the web response.")
            logger.debug(r.text)
            

    def load_tokens(self):
        try:
            with open(IDENTITY_TOKEN_NAME,'r') as fp:
                self.identity_token = json.load(fp)
            with open(ACCESS_TOKEN_NAME,'r') as fp2:
                self.access_token = json.load(fp2)
            user_token = decode_jwt(self.identity_token)
            for k,v in user_token.items():
                logger.debug("{}: {}".format(k,v))
            return 
        except:
            logger.debug(traceback.format_exc())
            self.access_token = None
            self.identity_token = None
            return False

    def save_file(self):
        if self.connection_type != 'USB':
            QMessageBox.warning(self,"Connection Type","Please connect a device and select a file.")
            return

        if self.meta_data_dict is None:
            QMessageBox.warning(self,"Select File","Please connect a device and select a file.")
            return   
        self.download_file()
        

    def upload_file(self):
        if self.connection_type != 'USB':
            QMessageBox.warning(self,"Connection Type","Please connect a device and select a file.")
            return

        if self.meta_data_dict is None:
            QMessageBox.warning(self,"Select File","Please connect a device and select a file.")
            return

        if not decode_jwt(self.identity_token):
            message = "A valid webtoken is not available to upload."
            logger.warning(message)
            QMessageBox.warning(self,"Invalid Token",message)
            return


        url = API_ENDPOINT + "upload"
        header = {}
        header["x-api-key"] = self.API_KEY #without this header, the API Gateway will return a 403: Forbidden message.
        header["Authorization"] = self.identity_token #without this header, the API Gateway will return a 401: Unauthorized message
        logger.debug("Using the following header:\n{}".format(header))
        self.body_dict ={}
        self.upload_user_input()
        if self.cont == False:
            return
        self.download_file()
        if self.download_status == False:
            return
        self.body_dict['device_data']=self.meta_data_dict["base64"]
        self.body_dict['user_input_data']=self.user_input_dict
        
        try:
            r = requests.post(url, json=self.body_dict, headers=header)
        except requests.exceptions.ConnectionError:
            QMessageBox.warning(self,"Connection Error","The there was a connection error when connecting to\n{}\nPlease try again once connection is established".format(url))
            return
        print(r.status_code)
        if r.status_code == 200: #This is normal return value
            response_dict = r.json()
            logger.debug(response_dict['upload_link'])
            
            r1 = requests.post( response_dict['upload_link']['url'], 
                                data=response_dict['upload_link']['fields'], 
                                files={'file': self.encrypted_log_file}
                               )
            logger.debug(r1.status_code)
            logger.debug(r1.text)
            if r1.status_code == 204:
                self.meta_data_dict['file_uid']=response_dict['upload_link']['fields']['key']
                self.verify_upload()

        else: #Something went wrong
            logger.debug(r.text)
            QMessageBox.warning(self,"Connection Error","The there was an error:\n{}".format(r.text))
            return
		

    def verify_upload(self):
    	'''
    	This function will get a response from a server for verifying if the uploaded file is authentic
    	'''
    	url = API_ENDPOINT + "verify_upload"
    	header = {}
    	header["x-api-key"] = self.API_KEY #without this header, the API Gateway will return a 403: Forbidden message.
    	header["Authorization"] = self.identity_token #without this header, the API Gateway will return a 401: Unauthorized message
    	try:
    		r = requests.post(url, json=self.body_dict, headers=header)
    	except requests.exceptions.ConnectionError:
    		QMessageBox.warning(self,"Connection Error","The there was a connection error when connecting to\n{}\nPlease try again once connection is established".format(url))
    		return
    	print(r.status_code)
    	if r.status_code == 200: #This is normal return value
    		print(r.text)
    		QMessageBox.information(self,"Success", "Successfully uploaded binary file.\nDigest: {}".format(self.meta_data_dict['file_uid']))
    		self.statusBar().showMessage("Successfully uploaded binary file.")
    	else:
    		logger.debug(r.text)
    		QMessageBox.warning(self,"Connection Error","The there was an error:\n{}".format(r.text))
    		return

    def upload_user_input(self):
        self.cont = False #User has to input data in order to complete upload_file() function
        formGroupBox = QGroupBox("Log File Information")
        layout=QFormLayout()
        
        self.name = QLineEdit()
        self.company = QLineEdit()
        self.make = QComboBox()
        self.model = QLineEdit()
        self.year = QComboBox()
        self.note = QPlainTextEdit()
        self.access = QComboBox()

        self.access.addItem("No")
        self.access.addItem("Yes")

        make_list = [' ','Kenworth','Peterbilt','Freightliner','Volvo','Mack','International','Other']
        for i in make_list:
            self.make.addItem(str(i))

        today = datetime.datetime.today()
        year_list = range(1995,today.year+2)
        self.year.addItem(' ')
        for i in reversed(year_list):
            self.year.addItem(str(i))

        layout.addRow(QLabel('Name:'),self.name)
        layout.addRow(QLabel('Company:'),self.company)
        layout.addRow(QLabel('Make:'),self.make)
        layout.addRow(QLabel('Model:'),self.model)
        layout.addRow(QLabel('Year:'),self.year)
        layout.addRow(QLabel('Note:'),self.note)
        layout.addRow(QLabel('Default Access:'),self.access)

        formGroupBox.setLayout(layout)
        buttonBox = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttonBox.accepted.connect(self.get_data)
        buttonBox.rejected.connect(self.reject)

        self.window = QDialog(self)
        mainLayout = QVBoxLayout(self.window)
        mainLayout.addWidget(formGroupBox)
        mainLayout.addWidget(buttonBox)
        self.window.setLayout(mainLayout)
        self.window.setWindowTitle("User Input")
        self.window.exec()


    def get_data(self):
    	if self.access.currentText() =='Yes':
    		options = QFileDialog.Options()
    		options |= QFileDialog.Detail
    		self.data_file_name, data_file_type = QFileDialog.getOpenFileName(self,
                                            "Open CSV File",
                                            self.home_directory,
                                            "CSV Files (*.csv);;All Files (*)",
                                            options = options)
    		if self.data_file_name:
    			with open(self.data_file_name,'r') as file:
    				data = csv.reader(file)
    				temp_list = []
    				for i in data:
    					temp_list.append(i)
    				self.body_dict['access_list'] = temp_list[0]

    				#check value in accless_list
    				if self.body_dict['access_list'] ==[]:
    					QMessageBox.warning(self,"Error","Invalid share access list format! Please check your file and try again. ")
    					return
    				for j in self.body_dict['access_list']:
    					if self.character1 in j or self.character2 in j or j =='':
    						QMessageBox.warning(self,"Error","Invalid share access list format! Please try again. ")
    						return
    		else:
    			return
    	else:
    		self.body_dict['access_list'] = None


    	self.user_input_dict = {}
    	self.user_input_dict['Name'] = self.name.text()
    	self.user_input_dict['Company'] =self.company.text()
    	self.user_input_dict['Make'] =self.make.currentText()
    	self.user_input_dict['Model'] =self.model.text()
    	self.user_input_dict['Year'] =self.year.currentText()
    	self.user_input_dict['Note'] =self.note.toPlainText()

    	for i in self.user_input_dict:
    		if self.user_input_dict[i] =="":
    			self.user_input_dict[i] = " "
    	self.cont = True #User has clicked OK and now proceed to complete upload_file() 
    	self.window.accept()

    def reject(self):  
        self.window.reject()

    def list_server_file(self):
        if not decode_jwt(self.identity_token):
            message = "A valid webtoken is not available to connect to server. Please login."
            logger.warning(message)
            QMessageBox.warning(self,"Invalid Token",message)
            return

        self.connection_type = 'Server'

        url = API_ENDPOINT + "list"
        header = {}
        header["x-api-key"] = self.API_KEY #without this header, the API Gateway will return a 403: Forbidden message.
        header["Authorization"] = self.identity_token #without this header, the API Gateway will return a 401: Unauthorized message
        try:
            r = requests.get(url, headers=header)
        except requests.exceptions.ConnectionError:
            QMessageBox.warning(self,"Connection Error","The there was a connection error when connecting to\n{}\nPlease try again once connection is established".format(url))
            return
        logger.debug(r.status_code)
        if r.status_code == 200: #This is normal return value    
            self.server_data = r.json()
            logger.debug(self.server_data)
        else: #Something went wrong
            logger.debug(r.text)
            QMessageBox.warning(self,"Connection Error","The there was an error:\n{}".format(r.text))
            return


        header_labels = ["Upload Date",
                        "Verification Status",
                        "Filename",
                        "Log Date",
                        "File Size",
                        "CAN0 Bitrate",
                        "CAN1 Bitrate",
                        "Uploader",
                        "Access List",
                        "Logger Serial Number",
                        "Initialization Vector",
                        "Encrypted Session Key",
                        "Binary File SHA-256 Hash Digest",
                        "Text File SHA-256 Hash Digest",
                        "Digital Signature of Text SHA Digest",
                        "User Note",
                        "Download Log"]

        NUM_COLS = len(header_labels)
        NUM_ROWS = len(self.server_data['Items'])

        self.server_file_table = QTableWidget(NUM_ROWS,NUM_COLS,self)
        self.server_file_table.setHorizontalHeaderLabels(header_labels)
        self.server_file_table.setSelectionBehavior(QTableView.SelectRows);
        self.server_file_table.doubleClicked.connect(self.read_file_info)
        self.server_file_table.itemSelectionChanged.connect(self.load_server_meta_data)
        self.server_file_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.grid_layout.addWidget(self.server_file_table ,0,0,1,1)
        row = 0
        for i in range(NUM_ROWS):
            self.server_file_table.setItem(row,0,QTableWidgetItem(self.server_data['Items'][i]['upload_date']))
            self.server_file_table.setItem(row,1,QTableWidgetItem(self.server_data['Items'][i]['verify_status']))
            self.server_file_table.setItem(row,2,QTableWidgetItem(self.server_data['Items'][i]['filename']))
            self.server_file_table.setItem(row,3,QTableWidgetItem(self.server_data['Items'][i]['datetime']))
            self.server_file_table.setItem(row,4,QTableWidgetItem(self.server_data['Items'][i]['filesize']))
            self.server_file_table.setItem(row,5,QTableWidgetItem(self.server_data['Items'][i]['CAN0']))
            self.server_file_table.setItem(row,6,QTableWidgetItem(self.server_data['Items'][i]['CAN1']))
            self.server_file_table.setItem(row,7,QTableWidgetItem(self.server_data['Items'][i]['uploader']))
            self.server_file_table.setItem(row,8,QTableWidgetItem(str(self.server_data['Items'][i]['access_list'])))
            self.server_file_table.setItem(row,9,QTableWidgetItem(self.server_data['Items'][i]['serial_num']))
            self.server_file_table.setItem(row,10,QTableWidgetItem(self.server_data['Items'][i]['init_vect']))
            self.server_file_table.setItem(row,11,QTableWidgetItem(self.server_data['Items'][i]['session_key']))
            self.server_file_table.setItem(row,12,QTableWidgetItem(self.server_data['Items'][i]['digest']))
            self.server_file_table.setItem(row,13,QTableWidgetItem(self.server_data['Items'][i]['text_sha_digest']))
            self.server_file_table.setItem(row,14,QTableWidgetItem(self.server_data['Items'][i]['signature']))
            self.server_file_table.setItem(row,15,QTableWidgetItem(str(self.server_data['Items'][i]['meta_data'])))
            self.server_file_table.setItem(row,16,QTableWidgetItem(str(self.server_data['Items'][i]['download_log'])))

            row +=1

        self.server_file_table.resizeColumnsToContents()
        self.server_file_table.setSortingEnabled(True)
        return

    def load_server_meta_data(self):
        self.server_meta_data_dict = {}
        row = self.server_file_table.currentRow()
        self.server_meta_data_dict["digest"] = self.server_file_table.item(row,12).text()
        self.server_meta_data_dict["serial_num"] = self.server_file_table.item(row,9).text()
        self.server_meta_data_dict["filename"] = self.server_file_table.item(row,2).text()
        self.server_meta_data_dict["init_vect"] = self.server_file_table.item(row,10).text()
        
        for k,v in self.server_meta_data_dict.items():
            logger.debug("{}: {}".format(k,v))

        try:
            self.user_note = json.loads(self.server_file_table.item(row,15).text().replace("\'","\""))
        except:
            logger.debug(traceback.format_exc())
        self.access_list = self.server_file_table.item(row,8).text()[1:-1]
        self.download_list = self.server_file_table.item(row,16).text()[1:-1].replace("\"","")
        self.uploader = self.server_file_table.item(row,7).text()

    def download_server_file(self):
        if self.connection_type != 'Server':
            QMessageBox.warning(self,"Connection Type","Please connect to server and select a file.")
            return

        if self.server_meta_data_dict is None:
            QMessageBox.warning(self,"Select File","Please connect to server and select a file.")
            return

        if not decode_jwt(self.identity_token):
            message = "A valid webtoken is not available to download. Please login."
            logger.warning(message)
            QMessageBox.warning(self,"Invalid Token",message)
            return

        url = API_ENDPOINT + "download"
        header = {}
        header["x-api-key"] = self.API_KEY #without this header, the API Gateway will return a 403: Forbidden message.
        header["Authorization"] = self.identity_token #without this header, the API Gateway will return a 401: Unauthorized message

        try:
            r = requests.post(url, json = self.server_meta_data_dict, headers=header)
        except requests.exceptions.ConnectionError:
            QMessageBox.warning(self,"Connection Error","The there was a connection error when connecting to\n{}\nPlease try again once connection is established".format(url))
            return
        print(r.status_code)

        if r.status_code ==200: #This is normal return value
            data_dict = r.json()
            log_file_url = data_dict['log_file']
            session_key = base64.b64decode(data_dict['session_key'])

            r1 = requests.get(log_file_url)

            if r1.status_code == 200:
                log_file = r1.content

            else:
                QMessageBox.warning(self,"Error","There was an error trying to download the log file.")
                return

            #Ask user to save log file as encrypted or plaintext version
            msg = QMessageBox(self)
            msg.setText("What version do you want to save your log file on your local computer?")
            msg.setWindowTitle("Save Option")
            msg.setIcon(QMessageBox.Question)
            button1 = msg.addButton(str('Encrypted Log File'),QMessageBox.ActionRole)
            button2 = msg.addButton(str('Plaintext Log File'),QMessageBox.ActionRole)
            msg.setStandardButtons(QMessageBox.Cancel)
            msg.exec_()

            #If choose encrypted
            if msg.clickedButton() == button1:
                output = log_file
                output_name = self.server_meta_data_dict["filename"][:-4]

            #If choose plaintext
            elif msg.clickedButton() == button2:
                iv = bytearray.fromhex(self.server_meta_data_dict["init_vect"])
                cipher = Cipher(algorithms.AES(session_key),
                                    modes.CBC(iv),
                                    backend = default_backend())
                decryptor = cipher.decryptor()
                output = decryptor.update(log_file) + decryptor.finalize()
                output_name = self.server_meta_data_dict["filename"][:-4] +'_plaintext'
            else:
                self.list_server_file()
                return

            options = QFileDialog.Options()
            options |= QFileDialog.Detail
            self.data_file_name, data_file_type = QFileDialog.getSaveFileName(self,
                                                "Save File",
                                                self.home_directory + "/" + output_name,
                                                "BIN Files (*.bin);;All Files (*)",
                                                options = options)
            if self.data_file_name:
                with open(self.data_file_name, 'wb') as file:
                    file.write(output)
                    file.close()

            self.list_server_file()

        else: #Something went wrong
            logger.debug(r.text)
            QMessageBox.warning(self,"Error","The there was an error:\n{}".format(r.text))
            return

    def share_access(self):
        if self.connection_type != 'Server':
            QMessageBox.warning(self,"Connection Type","Please connect to server and select a file.")
            return

        if self.server_meta_data_dict is None:
            QMessageBox.warning(self,"Select File","Please connect to server and select a file.")
            return

        if not decode_jwt(self.identity_token):
            message = "A valid webtoken is not available to download. Please login."
            logger.warning(message)
            QMessageBox.warning(self,"Invalid Token",message)
            return
        self.status = False
        body_dict = {}
        body_dict["digest"] = self.server_meta_data_dict["digest"]
        #Ask user to save log file as encrypted or plaintext version
        msg = QMessageBox(self)
        msg.setText("Do you want to share or revoke access?")
        msg.setWindowTitle("Share Access")
        msg.setIcon(QMessageBox.Question)
        button1 = msg.addButton(str('Share'),QMessageBox.ActionRole)
        button2 = msg.addButton(str('Revoke'),QMessageBox.ActionRole)
        msg.setStandardButtons(QMessageBox.Cancel)
        msg.exec_()

        if msg.clickedButton() == button1:
            body_dict["option"] = 'Share'
            button_status = True
        elif msg.clickedButton() == button2:
            body_dict["option"] = 'Revoke'
            button_status = True
        else:
            button_status = False

        if button_status:
            formGroupBox = QGroupBox("User Input")
            layout = QFormLayout()
            self.email_access = QLineEdit()
            layout.addRow(QLabel('Email:'),self.email_access)

            formGroupBox.setLayout(layout)
            buttonBox = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
            buttonBox.accepted.connect(self.get_email)
            buttonBox.rejected.connect(self.reject)

            self.window = QDialog(self)
            mainLayout = QVBoxLayout(self.window)
            mainLayout.addWidget(formGroupBox)
            mainLayout.addWidget(buttonBox)
            self.window.setLayout(mainLayout)
            self.window.setWindowTitle("Share Access")
            self.window.exec()

        if self.status:
            body_dict["email_access"] = self.email_input

            url = API_ENDPOINT + "share"
            header = {}
            header["x-api-key"] = self.API_KEY #without this header, the API Gateway will return a 403: Forbidden message.
            header["Authorization"] = self.identity_token #without this header, the API Gateway will return a 401: Unauthorized message
            try:
                r = requests.post(url, json = body_dict, headers=header)
            except requests.exceptions.ConnectionError:
                QMessageBox.warning(self,"Connection Error","The there was a connection error when connecting to\n{}\nPlease try again once connection is established".format(url))
                return
            print(r.status_code)

            if r.status_code == 200: #This is normal return value    
                logger.debug(r.text)
                QMessageBox.information(self,"Success",r.text)
            else: #Something went wrong
                logger.debug(r.text)
                QMessageBox.warning(self,"Error",(r.text))

            #Refresh server file table
            self.list_server_file()


    def get_email(self):
        self.email_input = self.email_access.text()
        if self.email_input =="":
            QMessageBox.warning(self,"Input Error","Cannot leave the input blank")
            return
        elif (self.character1 in self.email_input):
            QMessageBox.warning(self,"Input Error","Cannot have a space in the input")
            return
        elif (self.character2 in self.email_input):
            QMessageBox.warning(self,"Input Error","Cannot have a , in the input")
            return
        self.status = True
        self.window.accept()

    def read_file_info(self):
        if self.connection_type != 'Server':
            QMessageBox.warning(self,"Connection Type","Please connect to server and select a file.")
            return

        if self.server_meta_data_dict is None:
            QMessageBox.warning(self,"Select File","Please connect to server and select a file.")
            return

        if not decode_jwt(self.identity_token):
            message = "A valid webtoken is not available to download. Please login."
            logger.warning(message)
            QMessageBox.warning(self,"Invalid Token",message)
            return

        QMessageBox.information(self,"File Information","Uploader: {}\nShare Access: {}\n\nName: {}\nCompany: {}\nMake: {}\nModel: {}\nYear: {}\nNote: {}\n\nDownload Log: {}".format(
                                    self.uploader,self.access_list,self.user_note['Name'],self.user_note['Company'],
                                    self.user_note['Make'],self.user_note['Model'],
                                    self.user_note['Year'],self.user_note['Note'],self.download_list))

    def save_default_access(self):
        QMessageBox.information(self,"Create Default Share Access List","Please input the emails for default access list, with one email for each line. ")
        formGroupBox = QGroupBox("User Input")
        layout = QFormLayout()
        self.access_input = QPlainTextEdit()
        layout.addRow(QLabel('Email List:'),self.access_input)

        formGroupBox.setLayout(layout)
        buttonBox = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttonBox.accepted.connect(self.get_email_list)
        buttonBox.rejected.connect(self.reject)

        self.window = QDialog(self)
        mainLayout = QVBoxLayout(self.window)
        mainLayout.addWidget(formGroupBox)
        mainLayout.addWidget(buttonBox)
        self.window.setLayout(mainLayout)
        self.window.setWindowTitle("Create Default Share Access List")
        self.window.exec()

        return

    def get_email_list(self):
    	self.default_email_list = self.access_input.toPlainText().split("\n")
    	if self.default_email_list[len(self.default_email_list)-1] =='':
    		self.default_email_list.pop(len(self.default_email_list)-1)
    		if self.default_email_list ==[]:
    			QMessageBox.warning(self,"Input Error","Cannot have a blank input!\nPlease input one email per line.")
    			return

    	#Check user input for incorrect format
    	for i in self.default_email_list:
    		if self.character2 in i:
    			QMessageBox.warning(self,"Input Error","Cannot have a , in the input!\nPlease input one email per line.")
    			return
    		elif self.character1 in i:
    			QMessageBox.warning(self,"Input Error","Cannot have a space in the input!\nPlease input one email per line.")
    			return
    		elif i =='':
    			QMessageBox.warning(self,"Input Error","Cannot have a blank line!\nPlease input one email per line.")
    			return
    	try: 
    		options = QFileDialog.Options()
    		options |= QFileDialog.Detail
    		self.data_file_name, data_file_type = QFileDialog.getSaveFileName(self,
	                                                "Save File",
	                                                self.home_directory + "/" + "default_access_list",
	                                                "CSV Files (*.csv);;All Files (*)",
	                                                options = options)
    		if self.data_file_name:
    			with open(self.data_file_name, 'w') as file:
    				wr = csv.writer(file,dialect='excel')
    				wr.writerow(self.default_email_list)

    	except PermissionError:
	    	QMessageBox.warning(self,"Writing Error","Cannot write to file! Please make sure the file is closed.")
	    	return

    	

    	self.window.accept()


if __name__.endswith('__main__'):
    app = QApplication(sys.argv)
    execute = CANLogger()
    sys.exit(app.exec_())
    

