import struct

OP_MULTIPLY = (0x22)
OP_DIVIDE = (0x23)
OP_SUM = (0x24)
OP_SUBTRACT = (0x25)

OP_REQUEST = (0x6E)
OP_RESULT = (0x6F)

TYPE_BYTE  = ">B"
TYPE_INT   = ">i"
TYPE_UINT  = ">I"
TYPE_FLOAT = ">f"

def calculate_crc(bytes):
    crc = 0
    for i in range(0,12):
        crc += bytes[i]
    return crc % 256

def byte_size():
    return struct.calcsize(TYPE_BYTE)

def int_size():
    return struct.calcsize(TYPE_INT)

def float_size():
    return struct.calcsize(TYPE_FLOAT)