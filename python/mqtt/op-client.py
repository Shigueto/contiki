import socket
import struct
import opdefines

UDP_PORT = 8802
UDP_IP = "::1"

""" struct mathopreq {
uint8_t opRequest;
int32_t op1;
uint8_t operation;
int32_t op2;
float fc;
}  __attribute__((packed)); """
def pack_request(op1, operation, op2, fc):
    return struct.pack(">BiBif", opdefines.OP_REQUEST, op1, operation, op2, fc)

""" struct mathopreply {
  uint8_t opResult;
  int32_t intPart;
  uint32_t fracPart;
  float fpResult;
  uint8_t crc;
}  __attribute__((packed)); """
def unpack_reply(data):
    offset = 0
    opResult = struct.unpack_from(opdefines.TYPE_BYTE, data, offset)
    offset += opdefines.byte_size()
    intPart = struct.unpack_from(opdefines.TYPE_INT, data, offset)
    offset += opdefines.int_size()
    fracPart = struct.unpack_from(opdefines.TYPE_UINT, data, offset)
    offset += opdefines.int_size()
    fpResult = struct.unpack_from(opdefines.TYPE_FLOAT, data, offset)
    offset += opdefines.float_size()
    crc = struct.unpack_from(opdefines.TYPE_BYTE, data, offset)
    return opResult, intPart, fracPart, fpResult, crc

print ("UDP target IP:", UDP_IP)
print ("UDP target port:", UDP_PORT)
# socket.SOCK_DGRAM = UDP connection
sock = socket.socket(socket.AF_INET6,socket.SOCK_DGRAM)

led_state = 0

def send_multipy_request():
    msg = pack_request(2, opdefines.OP_SUBTRACT, -10, -3.14)
    sock.sendto(msg, (UDP_IP, UDP_PORT))

send_multipy_request()

while True:
    data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
    print ("received message:", data, "from: [", addr[0].strip(),"]:",addr[1])
    opResult, intPart, fracPart, fpResult, crc = unpack_reply(data)
    calculated_crc = opdefines.calculate_crc(data)
    print("=============")
    print("opResult=", hex(opResult[0]))
    print("intPart=", intPart)
    print("fracPart=", fracPart)
    print("fpResult=", fpResult)
    print("crc=", crc)
    print("crc calculado=", calculated_crc)
    print("=============")