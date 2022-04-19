import socket
import struct

UDP_PORT = 8802
UDP_IP = "::1"

LED_TOGGLE_REQUEST = (0x79)
LED_SET_STATE = (0x7A)
LED_STATE = (0x7C)

LED_GREEN = (0x1)
LED_RED = (0x2)


print ("UDP target IP:", UDP_IP)
print ("UDP target port:", UDP_PORT)
# socket.SOCK_DGRAM = UDP connection
sock = socket.socket(socket.AF_INET6,socket.SOCK_DGRAM)

led_state = 0

def send_led_state():
    msg = struct.pack(">BB", LED_STATE, led_state) # big-endian unsigned char
    sock.sendto(msg, (UDP_IP, UDP_PORT))

def send_toogle_request():
    msg = struct.pack(">B", LED_TOGGLE_REQUEST)
    sock.sendto(msg, (UDP_IP, UDP_PORT))

send_toogle_request()

while True:
    data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
    print ("received message:", data, "from: [", addr[0].strip(),"]:",addr[1])
    offset = 0
    op = struct.unpack_from(">B", data, offset)
    offset += struct.calcsize(">B")
    if op[0] == LED_SET_STATE:
        state = struct.unpack_from(">B", data, offset)
        led_state = state[0]
        print("received LED_SET_STATE ", state[0])
        send_led_state()
