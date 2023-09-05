import socket

DESTINATION_ADDR = '192.168.30.2' # The IP address of the sender
DESTINATION_PORT = 2390 #This is the local port of the sender
SOURCE_PORT = 2390 #The port the sender is sending the data too


sock =  socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(10)
sock.bind(('', SOURCE_PORT))
sock.connect((DESTINATION_ADDR, DESTINATION_PORT))
#sock.send(b'stream')
print(sock)
for i in range(10):
    data = sock.recv(512)
    print(data[:40])