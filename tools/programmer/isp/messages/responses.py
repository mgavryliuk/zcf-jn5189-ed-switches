from dataclasses import dataclass

from isp.entities import DeviceMemory
from isp.messages.base import ResponseMessage
from isp.enums import MemoryAccessBits, MemoryID, MemoryType, ResponseCommandType


@dataclass
class UnlockISPResponse(ResponseMessage):
    pass


@dataclass
class GetDeviceInfoResponse(ResponseMessage):
    @property
    def internal_chip_id(self) -> str:
        chip_id = self.unpack_value(">I", 1)
        return f"0x{chip_id:08X}"

    @property
    def bootloader_version(self) -> str:
        version = self.unpack_value(">I", 5)
        return f"0x{version:08X}"


@dataclass
class GetMacAddressResponse(ResponseMessage):
    @property
    def mac_address(self) -> str:
        return ":".join(f"{b:02X}" for b in self.payload[1:][::-1])


@dataclass
class GetDeviceIDResponse(ResponseMessage):
    @property
    def device_id(self) -> str:
        return str(self.unpack_value("<I", 1))


@dataclass
class GetMemoryInfoResponse(ResponseMessage):
    @property
    def device_memory(self) -> DeviceMemory:
        memid, base_addr, length, sector_size, mtype, access = self.unpack_value(
            "<BIIIBB", 1
        )
        name = self.payload[16:].decode("ascii")
        access_list = []
        access_byte = self.payload[15]

        if access_byte == 15:  # all access
            access_list.append("ALL_ACCESS")
        else:
            for access_bit in MemoryAccessBits:
                if access_bit.value & access_byte:
                    access_list.append(access_bit.name)

        return DeviceMemory(
            MemoryID(memid),
            name,
            base_addr,
            length,
            sector_size,
            MemoryType(mtype),
            access_list,
        )


def message_from_packet(packet: bytes) -> ResponseMessage:
    mapping = {
        ResponseCommandType.UNLOCK_ISP: UnlockISPResponse,
        ResponseCommandType.GET_DEVICE_INFO: GetDeviceInfoResponse,
        ResponseCommandType.GET_MEMORY_INFO: GetMemoryInfoResponse,
    }

    message = ResponseMessage.from_packet(packet)
    if message.msg_type in mapping:
        msg_cls = mapping[message.msg_type]
        return msg_cls.from_message(message)

    return message


if __name__ == "__main__":
    response_packet = b"\x00\x00\tO\x00\xbe\x12\x9fX"
    print(message_from_packet(response_packet))
