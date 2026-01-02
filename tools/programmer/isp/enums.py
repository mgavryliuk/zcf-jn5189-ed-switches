from enum import IntEnum


class RequestCommandType(IntEnum):
    RESET = 0x14
    EXECUTE = 0x21
    SET_BAUDRATE = 0x27
    GET_DEVICE_INFO = 0x32
    OPEN_MEMORY = 0x40
    ERASE_MEMORY = 0x42
    BLANK_CHECK_MEMORY = 0x44
    READ_MEMORY = 0x46
    WRITE_MEMORY = 0x48
    CLOSE_MEMORY = 0x4A
    GET_MEMORY_INFO = 0x4C
    UNLOCK_ISP = 0x4E
    USE_CERTIFICATE = 0x50
    START_ENCRYPTED_TRANSFER = 0x52


class ResponseCommandType(IntEnum):
    RESET = 0x15
    EXECUTE = 0x22
    SET_BAUDRATE = 0x28
    GET_DEVICE_INFO = 0x33
    OPEN_MEMORY = 0x41
    ERASE_MEMORY = 0x43
    BLANK_CHECK_MEMORY = 0x45
    READ_MEMORY = 0x47
    WRITE_MEMORY = 0x49
    CLOSE_MEMORY = 0x4B
    GET_MEMORY_INFO = 0x4D
    UNLOCK_ISP = 0x4F
    USE_CERTIFICATE = 0x51
    START_ENCRYPTED_TRANSFER = 0x53


class ResponseStatus(IntEnum):
    SUCCESS = 0x00
    MEMORY_INVALID_MODE = 0xEF
    MEMORY_BAD_STATE = 0xF0
    MEMORY_TOO_LONG = 0xF1
    MEMORY_OUT_OF_RANGE = 0xF2
    MEMORY_ACCESS_INVALID = 0xF3
    MEMORY_NOT_SUPPORTED = 0xF4
    MEMORY_INVALID = 0xF5
    NO_RESPONSE = 0xF6
    NOT_AUTHORIZED = 0xF7
    TEST_ERROR = 0xF8
    READ_FAIL = 0xF9
    USER_INTERRUPT = 0xFA
    ASSERT_FAIL = 0xFB
    CRC_ERROR = 0xFC
    INVALID_RESPONSE = 0xFD
    WRITE_FAIL = 0xFE
    NOT_SUPPORTED = 0xFF


class MemoryID(IntEnum):
    FLASH = 0
    PSECT = 1
    pFlash = 2
    Config = 3
    EFUSE = 4
    ROM = 5
    RAM0 = 6
    RAM1 = 7


class MemoryType(IntEnum):
    ROM = 0
    FLASH = 1
    RAM = 2
    EFUSE = 5


class MemoryAccessBits(IntEnum):
    READ = 1
    WRITE = 1 << 1
    ERASE = 1 << 2
    ERASE_ALL = 1 << 3
    BLANK_CHECK = 1 << 4


CommandType = RequestCommandType | ResponseCommandType
