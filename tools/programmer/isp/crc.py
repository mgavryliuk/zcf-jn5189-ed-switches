import crcmod


def calculate_crc(data: bytes):
    crc_func = crcmod.mkCrcFun(
        poly=0x104C11DB7,
        initCrc=0x00000000,
        rev=True,
        xorOut=0xFFFFFFFF,
    )
    return crc_func(data)
