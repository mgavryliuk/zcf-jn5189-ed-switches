from __future__ import annotations

from dataclasses import dataclass
import logging
import pathlib
import struct

import serial
from rich.progress import Progress

from isp.console import console
from isp.entities import Device
from isp.enums import MemoryID, RequestCommandType
from isp.messages.base import RequestMessage, ResponseMessage
from isp.messages.requests import (
    BlankCheckMemoryRequest,
    EraseMemoryRequest,
    GetMemoryInfoRequest,
    OpenFlashMemoryRequest,
    UnlockISPRequest,
    UnlockISPWithDefaultKeyRequest,
    GetDeviceInfoRequest,
    OpenConfigMemoryRequest,
    GetDeviceIDRequest,
    GetMacAddressRequest,
    CloseMemoryRequest,
)
from isp.messages.responses import (
    GetDeviceIDResponse,
    GetDeviceInfoResponse,
    GetMacAddressResponse,
    GetMemoryInfoResponse,
    message_from_packet,
)

LOGGER = logging.getLogger(__name__)


@dataclass
class ISPInterface:
    port: str
    baudrate: int = 115200
    parity: str = serial.PARITY_NONE
    bytesize: int = serial.EIGHTBITS
    stopbits: int = serial.STOPBITS_ONE
    _client: serial.Serial = None

    def __post_init__(self):
        self.device = Device()

    def __enter__(self) -> ISPInterface:
        self._client = serial.Serial(
            port=self.port,
            baudrate=self.baudrate,
            parity=self.parity,
            bytesize=self.bytesize,
            stopbits=self.stopbits,
        )
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self._client and self._client.is_open:
            self._client.close()

    def send_message(self, msg: RequestMessage) -> ResponseMessage:
        LOGGER.debug("Sending message: %s", msg)
        self._client.write(msg.packet)

        raw_resp = self._client.read(4)  # flags, length (2), type
        _, length, _ = struct.unpack(">BHB", raw_resp)
        LOGGER.debug("Reading response %d size", length)

        length_left_to_read = length - 4
        raw_resp += self._client.read(length_left_to_read)

        resp_msg = message_from_packet(raw_resp)
        LOGGER.debug("Response message: %s", resp_msg)
        return resp_msg

    def initialize(self) -> None:
        console.print("[bold]Starting init sequence[/bold]")
        self.send_message(UnlockISPRequest())
        response: GetDeviceInfoResponse = self.send_message(GetDeviceInfoRequest())
        self.device.internal_chip_id = response.internal_chip_id
        self.device.bootloader_version = response.bootloader_version
        self.send_message(UnlockISPWithDefaultKeyRequest())
        console.print("[bold][green]Init sequence finished[/green][/bold]")

    def get_info(self) -> None:
        mac_address = "unknown"
        device_id = "unknown"
        try:
            self.send_message(OpenConfigMemoryRequest())
            device_id_resp = GetDeviceIDResponse.from_message(self.send_message(GetDeviceIDRequest()))
            device_id = device_id_resp.device_id
            self.send_message(CloseMemoryRequest())
        except:
            LOGGER.error("Failed to get device ID")
        
        try:
            self.send_message(OpenConfigMemoryRequest())
            mac_addr_resp = GetMacAddressResponse.from_message(self.send_message(GetMacAddressRequest()))
            mac_address = mac_addr_resp.mac_address
            self.send_message(CloseMemoryRequest())
        except:
            LOGGER.error("Failed to get device MAC-address")

        self.device.mac_address = mac_address
        self.device.device_id = device_id

        self.get_memory_info()
        self.device.print_info()

    def get_memory_info(self) -> None:
        memory_list = []
        for memid in MemoryID:
            resp: GetMemoryInfoResponse = self.send_message(GetMemoryInfoRequest(payload=bytes([memid])))
            memory_list.append(resp.device_memory)
        self.device.memory = memory_list

    def erase(self) -> None:
        flash_memory = self.device.flash_memory
        self.send_message(OpenFlashMemoryRequest(payload= bytes([0x00, flash_memory.access_byte])))
        console.print(
            "[bold]Erasing memory with base addr %s size %s[/bold]"
            % (hex(flash_memory.base_addr), hex(flash_memory.length))
        )
        payload = struct.pack("<BBII", 0, 0, flash_memory.base_addr, flash_memory.length)
        self.send_message(EraseMemoryRequest(payload=payload))
        self.send_message(BlankCheckMemoryRequest(payload=payload))
        self.send_message(CloseMemoryRequest())
        console.print("[bold][green]Memory successfully erased[/green][/bold]")

    def flash(self, fpath: pathlib.Path) -> None:
        firmware_size = fpath.stat().st_size
        progress = Progress()
        progress.start()
        upload_task = progress.add_task("[yellow]Uploading...", total=firmware_size)

        try:
            flash_memory = self.device.flash_memory
            self.send_message(OpenFlashMemoryRequest(payload= bytes([0x00, flash_memory.access_byte])))
            start_memory = 0
            sector_size = flash_memory.sector_size
            with fpath.open("rb") as f:
                while fdata := f.read(sector_size):
                    payload = struct.pack(f"<BBII{len(fdata)}B", 0, 0, start_memory, sector_size, *fdata)
                    self.send_message(RequestMessage(RequestCommandType.WRITE_MEMORY, payload))
                    start_memory += sector_size
                    progress.update(upload_task, advance=sector_size)

            self.send_message(CloseMemoryRequest())
        finally:
            progress.stop()

    def restart(self) -> None:
        self.send_message(RequestMessage(RequestCommandType.RESET, b""))
