from __future__ import annotations

from dataclasses import dataclass
import struct
from typing import Any

from isp.crc import calculate_crc
from isp.enums import (
    ResponseStatus,
    CommandType,
    RequestCommandType,
    ResponseCommandType,
)


@dataclass
class Message:
    """
    Packet Byte Structure
    +------------+---------+------------------------------------------------------+
    | Bytes      | Field   | Description                                          |
    +------------+---------+------------------------------------------------------+
    | Byte 0     | Flags   | Bit meanings:                                        |
    |            |         |   • Bit 0 - Undefined (leave as 0)                   |
    |            |         |   • Bit 1 - If 1, packet includes SHA-256 signature  |
    |            |         |   • Bit 2 - If 1, packet includes “next hash”        |
    |            |         |   • Bits 3-7 - Undefined (leave as 0)                |
    +------------+---------+------------------------------------------------------+
    | Bytes 1-2  | Length  | Total packet length = n + 1                          |
    +------------+---------+------------------------------------------------------+
    | Byte 3     | Type    | Packet type identifier                               |
    +------------+---------+------------------------------------------------------+
    | Byte 4     | Payload | Start of payload data                                |
    +------------+---------+------------------------------------------------------+
    | Bytes      | Checksum| Checksum of the packet                               |
    | n-4  to n  |         |                                                      |
    +------------+---------+------------------------------------------------------+
    """

    msg_type: CommandType
    payload: bytes
    flags: int = 0
    checksum: bytes | None = None
    packet: bytes | None = None

    def __str__(self) -> str:
        length_bytes = self.packet[1:3]
        length = int.from_bytes(length_bytes, "big")
        return (
            f"{self.__class__.__name__}(\n"
            f"    flags: {self.flags:02X}\n"
            f"    length: {length_bytes.hex(' ').upper()} (int: {length})\n"
            f"    type: {self.msg_type:02X} ({self.msg_type.name})\n"
            f"    payload: {self.payload.hex(' ').upper()}\n"
            f"    checksum: {self.checksum.hex(' ').upper()}\n"
            f"    packet: {self.packet.hex(' ').upper()}\n"
            f")"
        )

    @classmethod
    def from_message(cls, message: Message) -> Message:
        return cls(
            flags=message.flags,
            msg_type=message.msg_type,
            payload=message.payload,
            checksum=message.checksum,
            packet=message.packet,
        )


@dataclass
class RequestMessage(Message):
    def __post_init__(self) -> None:
        length = (
            1 + 2 + 1 + len(self.payload) + 4
        )  # flags (1), length (2), type (1), payload (n), checksum (4)

        crc_data = struct.pack(
            f">BHB{len(self.payload)}B",
            self.flags,
            length,
            self.msg_type.value,
            *self.payload,
        )
        checksum = calculate_crc(crc_data)
        checksum_bytes = struct.pack(">I", checksum)
        self.checksum = checksum_bytes
        self.packet = crc_data + checksum_bytes


@dataclass
class ResponseMessage(Message):
    def __str__(self) -> str:
        length_bytes = self.packet[1:3]
        length = int.from_bytes(length_bytes, "big")
        status = ResponseStatus(self.payload[0])
        return (
            f"{self.__class__.__name__}(\n"
            f"    flags: {self.flags:02X}\n"
            f"    length: {length_bytes.hex(' ').upper()} (int: {length})\n"
            f"    type: {self.msg_type:02X} ({self.msg_type.name})\n"
            f"    status: {status:02X} ({status.name})\n"
            f"    payload: {self.payload[1:].hex(' ').upper()}\n"
            f"    checksum: {self.checksum.hex(' ').upper()}\n"
            f"    packet: {self.packet.hex(' ').upper()}\n"
            f")"
        )

    @classmethod
    def from_packet(cls, packet: bytes) -> ResponseMessage:
        flags, _, msg_type = struct.unpack(">BHB", packet[:4])
        payload, checksum = packet[4:-4], packet[-4:]
        msg = cls(
            flags=flags,
            msg_type=ResponseCommandType(msg_type),
            payload=payload,
            checksum=checksum,
            packet=packet,
        )

        msg.validate_status()
        msg.validate_crc()
        return msg

    def unpack_value(self, format, offset=0) -> Any:
        res = struct.unpack_from(format, self.payload, offset)
        if len(res) == 1:
            return res[0]
        return res

    def validate_status(self) -> None:
        status = self.payload[0]
        if status != ResponseStatus.SUCCESS:
            raise RuntimeError("Command failed!. %s" % self)

    def validate_crc(self) -> None:
        msg_without_crc = self.packet[:-4]
        checksum = calculate_crc(msg_without_crc)
        checksum_bytes = struct.pack(">I", checksum)
        if checksum_bytes != self.checksum:
            raise RuntimeError(
                "Wrong checksum! Expected: %s.\n%s"
                % (
                    checksum_bytes.hex(" ").upper(),
                    self,
                )
            )


if __name__ == "__main__":
    print(RequestMessage(RequestCommandType.GET_MEMORY_INFO, bytes([0])))
    response_packet = b"\x00\x00\x1dM\x00\x00\x00\x00\x00\x00\x00\xde\t\x00\x00\x02\x00\x00\x01\nFLASH>x\x9b\xf3"
    print(ResponseMessage.from_packet(response_packet))
