- [Overview](#overview)
- [Getting Started](#getting-started)
- [Devices](#devices)
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

# JN5189 Documentation
[Product page](https://www.nxp.com/products/JN5189_88_T)</br>
<!-- [Support Resources for JN516x MCUs](https://www.nxp.com/products/wireless-connectivity/zigbee/support-resources-for-jn516x-mcus:SUPPORT-RESOURCES-JN516X-MCUS)</br>
[JN516x/7x Zigbee 3.0](https://www.nxp.com/pages/jn516x-7x-zigbee-3-0:ZIGBEE-3-0)</br>
[Data Sheet: JN516x](docs/JN516X.pdf)</br>
[Data Sheet: JN5169](docs/JN5169.pdf)</br>
[IEEE 802.15.4 Stack (JN-UG-3024)](docs/JN-UG-3024.pdf)</br>
[ZigBee 3.0 Stack (JN-UG-3113)](docs/JN-UG-3113.pdf)</br>
[ZigBee 3.0 Devices (JN-UG-3114)](docs/JN-UG-3114.pdf)</br>
[ZigBee Cluster Library (for ZigBee 3.0) (JN-UG-3115)](docs/JN-UG-3115.pdf)</br>
[JN51xx Core Utilities (JN-UG-3116)](docs/JN-UG-3116.pdf)</br>
[JN516x Integrated Peripherals API (JN-UG-3087)](docs/JN-UG-3087.pdf)</br>
[JN51xx Production Flash Programmer (JN-UG-3099)](docs/JN-UG-3099.pdf)</br>
[JN51xx Boot Loader Operation (JN-AN-1003)](docs/JN-AN-1003.pdf) -->

## Support the Project
If you find this project helpful, please consider supporting the development. Due to limited payment options available in Ukraine, these are currently the only ways to support the project:
* becoming a patron on [Patreon](https://www.patreon.com/mgavryliuk) to help fund ongoing development and maintenance
* make a one-time contribution through the [Patreon store](https://www.patreon.com/mgavryliuk/shop)

Your support allows me to dedicate more time to improving and expanding the custom firmware options.

**Thank you!**
