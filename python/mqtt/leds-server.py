import socket
import struct
import random


HOST = '' #all interfaces
UDP_PORT = 8802

LED_TOGGLE_REQUEST = (0x79)
LED_SET_STATE = (0x7A)
LED_GET_STATE = (0x7B)
LED_STATE = (0x7C)

LED_GREEN = (0x1)
LED_RED = (0x2)

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM) # UDP IPv6
sock.bind((HOST, UDP_PORT))

while True:
    data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
    print ("received message:", data, "from: [", addr[0].strip(),"]:",addr[1])
    offset = 0
    op = struct.unpack_from(">B", data, offset)
    offset += struct.calcsize(">B")
    if op[0] == LED_STATE:
        state = struct.unpack_from(">B", data, offset)
        print("received LED_STATE ", state[0])
    elif op[0] == LED_TOGGLE_REQUEST:
        print("received LED_TOGGLE_REQUEST")
        msg = struct.pack(">BB", LED_SET_STATE, LED_GREEN)
        sock.sendto(msg, (addr[0], addr[1]))
