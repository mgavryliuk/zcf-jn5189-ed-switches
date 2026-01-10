- [Overview](#overview)
- [Getting Started](#getting-started)
- [Devices](#devices)
- [Flashing](#flashing)
- [JN5189 Documentation](#jn5189-documentation)

# Overview
This repository provides custom wireless switch firmware implementations for [JN5189 MCU-based](https://www.nxp.com/products/JN5189_88_T) devices.

# Getting Started
This project uses VS Code and [Dev Containers](https://code.visualstudio.com/docs/devcontainers/containers) for development and building.
1. Install [Visual Studio Code](https://code.visualstudio.com/).
2. Follow the [Dev Containers tutorial](https://code.visualstudio.com/docs/devcontainers/tutorial) and open the project in the remote environment
3. Click the `Build` button to compile the project.

> **Note!** Function `eZCL_ReportAttribute` in `sdk/middleware/wireless/zigbee/ZCIF/Source/zcl.c` was adjusted manually to disable default response.
> ```c
>        // Write command header
>        u16Offset = u16ZCL_WriteCommandHeader(
>              hAPduInst, eFrameType,
>              FALSE, 0, //Not mfr.specific
>              TRUE, //From server to client
>              TRUE, // changed by hands to disable default response. Original: FALSE
>              u8Seq, E_ZCL_REPORT_ATTRIBUTES);
> ```

# Devices
| Device | Description | Documentation | Build Preset |
|--------|-------------|---------------|--------------|
| WXKG15LM | Wireless remote switch H1 (double rocker) | [Documentation](firmwares/WXKG15LM/README.md) | `WXKG15LM` |

# Flashing
> ⚠️ **Important**
> - FTDI **must be set to 3.3V**. Using 5V will permanently damage the device.
> - Do not power the device from any other source while FTDI 3.3V is connected.
> - `MISO → GND` is required **only during bootloader entry**.

1. Connect the FTDI adapter

| Target Pad | FTDI Pin | Notes |
|------------|----------|-------|
| **TDO**    | **RX**   | Data from device to FTDI |
| **TDI**    | **TX**   | Data from FTDI to device |
| **MISO**   | **GND**  | Connect to GND **only during bootloader mode** |
| **DC3V**   | **3.3V** | Power from FTDI (ensure FTDI is set to 3.3V) |
| **RESETN** | **GND (via button)** | Pulling RESETN low resets the device. Use a momentary button, not a permanent short. |
| **GND**    | **GND**  | Common ground |

2. Prepare flashing environment
```bash
cd utils/programmer

# Create virtual environment and install dependencies (only once)
uv venv -p 3.12 .venv
uv pip install -r requirements.txt

# Activate virtual environment
source .venv/bin/activate
```
3. Flash the firmware
```bash
# Find your FTDI port
./flash.py -s <FTDI_PORT> ../../firmwares/<DEVICE_ID>/<FIRMWARE_NAME>.bin

# Example (do not copy blindly):
./flash.py -s /dev/cu.usbserial-A5069RR4 ../../firmwares/DEVBOARD/000001_20260110_DEVBOARD.bin
```
> If flashing fails, check:
> - `MISO → GND` was connected during bootloader entry
> - FTDI is configured for 3.3V
> - FTDI is set to 3.3V
> - The FTDI port path is correct

### Example of successful flash
```text
Starting init sequence
Init sequence finished
2026-01-10 12:48:14,390 [ERROR] isp.interface: Failed to get device ID
2026-01-10 12:48:14,393 [ERROR] isp.interface: Failed to get device MAC-address
┌────────────────────┬────────────┐
│ Device ID          │ unknown    │
│ Bootloader Version │ 0xCC000014 │
│ Internal Chip ID   │ 0x88888888 │
│ MAC address        │ unknown    │
└────────────────────┴────────────┘
Memory:
┏━━━━┳━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━┳━━━━━━━━━━━━━┳━━━━━━━┳━━━━━━━━━━━━━━━━━━┓
┃ ID ┃ Name   ┃ Address range           ┃ Length  ┃ Sector size ┃ Type  ┃ Access           ┃
┡━━━━╇━━━━━━━━╇━━━━━━━━━━━━━━━━━━━━━━━━━╇━━━━━━━━━╇━━━━━━━━━━━━━╇━━━━━━━╇━━━━━━━━━━━━━━━━━━┩
│ 0  │ FLASH  │ 0x00000000 - 0x0009DDFF │ 0x9de00 │ 0x200       │ FLASH │ WRITE, ERASE_ALL │
│ 1  │ PSECT  │ 0x00000000 - 0x000001DF │ 0x1e0   │ 0x10        │ FLASH │ NO_ACCESS        │
│ 2  │ pFlash │ 0x00000000 - 0x000001DF │ 0x1e0   │ 0x10        │ FLASH │ NO_ACCESS        │
│ 3  │ Config │ 0x0009FC00 - 0x0009FDFF │ 0x200   │ 0x200       │ FLASH │ NO_ACCESS        │
│ 4  │ EFUSE  │ 0x00000000 - 0x0000007F │ 0x80    │ 0x2         │ EFUSE │ READ             │
│ 5  │ ROM    │ 0x03000000 - 0x0301FFFF │ 0x20000 │ 0x1         │ ROM   │ READ             │
│ 6  │ RAM0   │ 0x04000000 - 0x04015FFF │ 0x16000 │ 0x1         │ RAM   │ NO_ACCESS        │
│ 7  │ RAM1   │ 0x04020000 - 0x0402FFFF │ 0x10000 │ 0x1         │ RAM   │ NO_ACCESS        │
└────┴────────┴─────────────────────────┴─────────┴─────────────┴───────┴──────────────────┘
Erasing memory with base addr 0x0 size 0x9de00
Memory successfully erased
Uploading... ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ 100% 0:00:00
```

# JN5189 Documentation
[Product page](https://www.nxp.com/products/JN5189_88_T)</br>
[Data Sheet: JN5189(T)/JN5188(T)](docs/JN5189.pdf)</br>
[User Manual: JN5189(T)/JN5188(T)](docs/UM11138.pdf)</br>
[ZigBee 3.0 Stack](docs/JN-UG-3130-Zigbee3-Stack.pdf)</br>
[ZigBee 3.0 Devices](docs/JN-UG-3131-ZigBee3-Devices.pdf)</br>
[ZigBee Cluster Library (for ZigBee 3.0)](docs/JN-UG-3132-ZigBee3-Cluster-Library.pdf)</br>
[JN518x & K32W041/K32W061 Core Utilities](docs/JN-UG-3133-Core-Utilities.pdf)</br>
[ZigBee Green Power (for ZigBee 3.0)](docs/JN-UG-3134-Zigbee3-Green-Power.pdf)</br>

## Support the Project
If you find this project helpful, please consider supporting the development. Due to limited payment options available in Ukraine, these are currently the only ways to support the project:
* becoming a patron on [Patreon](https://www.patreon.com/mgavryliuk) to help fund ongoing development and maintenance
* make a one-time contribution through the [Patreon store](https://www.patreon.com/mgavryliuk/shop)

Your support allows me to dedicate more time to improving and expanding the custom firmware options.

**Thank you!**
