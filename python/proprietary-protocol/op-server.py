import socket
import struct
import opdefines

HOST = '' #all interfaces
UDP_PORT = 8802

""" struct mathopreply {
  uint8_t opResult;
  int32_t intPart;
  uint32_t fracPart;
  float fpResult;
  uint8_t crc;
}  __attribute__((packed)); """

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM) # UDP IPv6
sock.bind((HOST, UDP_PORT))

""" struct mathopreq {
uint8_t opRequest;
int32_t op1;
uint8_t operation;
int32_t op2;
float fc;
}  __attribute__((packed)); """
def unpack_opreq(data):
    offset = 0
    opRequest = struct.unpack_from(opdefines.TYPE_BYTE, data, offset)
    offset += opdefines.byte_size()
    op1 = struct.unpack_from(opdefines.TYPE_INT, data, offset)
    offset += opdefines.int_size()
    operation = struct.unpack_from(opdefines.TYPE_BYTE, data, offset)
    offset += opdefines.byte_size()
    op2 = struct.unpack_from(opdefines.TYPE_INT, data, offset)
    offset += opdefines.int_size()
    fc = struct.unpack_from(opdefines.TYPE_FLOAT, data, offset)
    return opRequest, op1, operation, op2, fc

""" struct mathopreply {
  uint8_t opResult;
  int32_t intPart;
  uint32_t fracPart;
  float fpResult;
  uint8_t crc;
}  __attribute__((packed)); """
def pack_reply(intPart, fracPart, fpResult):
    format = ">BiIf"
    msg = struct.pack(format, opdefines.OP_RESULT, intPart, fracPart, fpResult)
    bytesMsg = bytearray(msg)
    crc = opdefines.calculate_crc(bytesMsg)
    bytesMsg.append(crc)
    #crc_offset = struct.calcsize(format)
    return bytes(bytesMsg)


while True:
    data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
    print ("received message:", data, "from: [", addr[0].strip(),"]:",addr[1])
    opRequest, op1, operation, op2, fc = unpack_opreq(data)
    print("=============")
    print("opRequest=", hex(opRequest[0]))
    print("op1=", op1)
    print("operation=", hex(operation[0]))
    print("op2=", op2)
    print("fc=", fc)
    print("=============")
    result = 0
    if operation[0] == opdefines.OP_MULTIPLY:
        print("received OP_MULTIPLY")
        result = (op1[0]*op2[0])*fc[0]
    elif operation[0] == opdefines.OP_SUBTRACT:
        print("received OP_SUBTRACT")
        result = (op1[0]-op2[0])*fc[0]
    elif operation[0] == opdefines.OP_DIVIDE:
        print("received OP_DIVIDE")
        result = (op1[0]/op2[0])*fc[0]
    elif operation[0] == opdefines.OP_SUM:
        print("received OP_SUM")
        result = (op1[0]+op2[0])*fc[0]

    print("result=", result)
    absolute_value = abs(result)
    intPart = int(absolute_value)
    fracPart = int((absolute_value - intPart)*10000)
    msg = pack_reply(intPart, fracPart, result)

    sock.sendto(msg, (addr[0], addr[1]))
