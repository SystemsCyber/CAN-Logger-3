import json
import base64
import time

import boto3
from botocore.exceptions import ClientError

from cryptography.fernet import Fernet
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

from utils import lambdaResponse as response
from utils import get_timestamp

def key_dump(event, context):
    """
    
    """
    
    # Lookup the data needed from the unique CAN Logger by its serial number
    dbClient = boto3.resource('dynamodb', region_name='us-east-2')
    table = dbClient.Table("CANLoggers")
    records = table.scan()
    data = records['Items']
    print(data)
    results = {}
    for item in data:
        key = item['id']
        print(f'key = {key}')
        results[key] = item
        print(f'item = {item}')
        # load the device's public key which was stored as a base64 encoded binary
        device_public_key_bytes = bytearray.fromhex(base64.b64decode(item['device_public_key']).decode('ascii'))
        device_bytes = b'\x04' + device_public_key_bytes
        device_public_key = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(),device_bytes)
    
        # Decrypt the data key before using it
        cipher_key = base64.b64decode(item['encrypted_data_key'])
        data_key_plaintext = decrypt_data_key(cipher_key)
        print(f"data_key_plaintext = {data_key_plaintext}")
        results[key]['data_key_plaintext'] = data_key_plaintext.hex().upper()
        # Decrypt the private key for the device
        f = Fernet(data_key_plaintext)
        decrypted_pem = f.decrypt(base64.b64decode(item['encrypted_server_pem_key']))
        results[key]['decrypted_pem'] = decrypted_pem.decode('ascii')
        print(f"decrypted_pem = \n{decrypted_pem}")
        #load the serialized key into an object
        server_key = serialization.load_pem_private_key(decrypted_pem, 
                                                        password=None, 
                                                        backend=default_backend())

        #Derive shared secret
        shared_secret = server_key.exchange(ec.ECDH(),device_public_key)
        print(f"shared_secret = {shared_secret}")
        results[key]['shared_secret'] = shared_secret.hex().upper()
        
    return response(200, results)
    
def decrypt_data_key(data_key_encrypted):
    """Decrypt an encrypted data key

    :param data_key_encrypted: Encrypted ciphertext data key.
    :return Plaintext base64-encoded binary data key as binary string
    :return None if error
    """

    # Decrypt the data key
    kms_client = boto3.client('kms')
    try:
        response = kms_client.decrypt(CiphertextBlob=data_key_encrypted)
    except ClientError as e:
        print(e)
        return None

    # Return plaintext base64-encoded binary data key
    return base64.b64encode((response['Plaintext']))