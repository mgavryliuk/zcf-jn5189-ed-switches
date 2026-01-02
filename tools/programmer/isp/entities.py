from __future__ import annotations

from dataclasses import dataclass

from rich.table import Table

from isp.enums import MemoryID, MemoryType
from isp.console import console


@dataclass
class Device:
    internal_chip_id: str | None = None
    bootloader_version: str | None = None
    device_id: str | None = None
    mac_address: str | None = None
    memory: list[DeviceMemory] | None = None

    @property
    def flash_memory(self):
        for mem in self.memory:
            if mem.id == MemoryID.FLASH:
                return mem
        raise RuntimeError("Flash memory was not found")

    def print_info(self):
        device_info_table = Table(show_header=False)
        device_info_table.add_row("[bold]Device ID[/bold]", self.device_id)
        device_info_table.add_row("[bold]Bootloader Version[/bold]", self.bootloader_version)
        device_info_table.add_row("[bold]Internal Chip ID[/bold]", self.internal_chip_id)
        device_info_table.add_row("[bold]MAC address[/bold]", self.mac_address)
        console.print(device_info_table)
        console.print("[bold]Memory:[/bold]")
        memory_table = Table(
            "ID",
            "Name",
            "Address range",
            "Length",
            "Sector size",
            "Type",
            "Access",
        )
        for mem in self.memory:
            memory_table.add_row(
                str(mem.id.value),
                mem.name,
                f"0x{mem.base_addr:08X} - 0x{mem.end_addr:08X}",
                hex(mem.length),
                hex(mem.sector_size),
                mem.mtype.name,
                ", ".join(mem.access),
            )
        console.print(memory_table)


@dataclass
class DeviceMemory:
    id: MemoryID
    name: str
    base_addr: int
    length: int
    sector_size: int
    access_byte: int
    mtype: MemoryType
    access: list[str]

    def __str__(self):
        return (
            f"{self.name}(\n"
            f"    Addr: {hex(self.base_addr)} - {hex(self.end_addr)} ({hex(self.length)})\n"
            f"    Sector size: {hex(self.sector_size)}\n"
            f"    Type: {self.mtype.name}\n"
            f"    Access: {', '.join(self.access)}\n"
            f")"
        )

    @property
    def end_addr(self):
        return self.base_addr + self.length - 1
