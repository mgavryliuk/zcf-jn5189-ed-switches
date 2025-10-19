from dataclasses import dataclass, field

from isp.enums import RequestCommandType
from isp.messages.base import RequestMessage


@dataclass
class UnlockISPRequest(RequestMessage):
    msg_type: RequestCommandType = field(
        default=RequestCommandType.UNLOCK_ISP, init=False
    )
    payload: bytes = field(default=bytes([0x00]), init=False)


@dataclass
class UnlockISPWithDefaultKeyRequest(UnlockISPRequest):
    payload: bytes = field(
        default=bytes(
            [
                0x01,
                0x11,
                0x22,
                0x33,
                0x44,
                0x55,
                0x66,
                0x77,
                0x88,
                0x11,
                0x22,
                0x33,
                0x44,
                0x55,
                0x66,
                0x77,
                0x88,
            ]
        ),
        init=False,
    )


@dataclass
class GetDeviceInfoRequest(RequestMessage):
    msg_type: RequestCommandType = field(
        default=RequestCommandType.GET_DEVICE_INFO, init=False
    )
    payload: bytes = field(default=b"", init=False)


@dataclass
class OpenConfigMemoryRequest(RequestMessage):
    msg_type: RequestCommandType = field(
        default=RequestCommandType.OPEN_MEMORY, init=False
    )
    payload: bytes = field(default=bytes([0x03, 0x01]), init=False)


@dataclass
class GetMacAddressRequest(RequestMessage):
    msg_type: RequestCommandType = field(
        default=RequestCommandType.READ_MEMORY, init=False
    )
    payload: bytes = field(
        default=bytes([0x00, 0x00, 0x70, 0xFC, 0x09, 0x00, 0x08, 0x00, 0x00, 0x00]),
        init=False,
    )


@dataclass
class GetDeviceIDRequest(RequestMessage):
    msg_type: RequestCommandType = field(
        default=RequestCommandType.READ_MEMORY, init=False
    )
    payload: bytes = field(
        default=bytes([0x00, 0x00, 0x60, 0xFC, 0x09, 0x00, 0x04, 0x00, 0x00, 0x00]),
        init=False,
    )


@dataclass
class CloseMemoryRequest(RequestMessage):
    msg_type: RequestCommandType = field(
        default=RequestCommandType.CLOSE_MEMORY, init=False
    )
    payload: bytes = field(default=bytes([0x00]), init=False)


@dataclass
class GetMemoryInfoRequest(RequestMessage):
    msg_type: RequestCommandType = field(
        default=RequestCommandType.GET_MEMORY_INFO, init=False
    )


@dataclass
class OpenFlashMemoryRequest(RequestMessage):
    msg_type: RequestCommandType = field(
        default=RequestCommandType.OPEN_MEMORY, init=False
    )
    payload: bytes = field(default=bytes([0x00, 0x0F]), init=False)


@dataclass
class EraseMemoryRequest(RequestMessage):
    msg_type: RequestCommandType = field(
        default=RequestCommandType.ERASE_MEMORY, init=False
    )


@dataclass
class BlankCheckMemoryRequest(RequestMessage):
    msg_type: RequestCommandType = field(
        default=RequestCommandType.BLANK_CHECK_MEMORY, init=False
    )


if __name__ == "__main__":
    print(GetDeviceInfoRequest())
