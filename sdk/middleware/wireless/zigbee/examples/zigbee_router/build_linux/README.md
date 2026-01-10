
# 1. General description

<p>The purpose of this demo is to demonstrate the capabilities of the NXP Wireless SoC when used as a Zigbee NCP together with a Zigbee Router application running on the iMX8 (or x86) platform under Linux. </p>

<p>The demo showcases how to connect the Zigbee NCP Router to the network formed by a Zigbee Coordinator and toggle the OnOff cluster available on the device. </p>

<p>A step-by-step guide to the hardware and software configurations are provided, as well as the steps needed to be executed in order to get the boards up and running.</p>

List of supported NXP Wireless SoC to act as coprocessor:

* K32W148

* MCXW71

* K32W061

# 2. Required Hardware

* 1 x iMX8M-EVK board running Linux - Host 

* 1 x K32W148-EVK board or 1 x K32W061 DK6 board or 1 x FRDM-MCXW71 board – Zigbee NCP coprocessor

* 1 x K32W148-EVK board or 1 x K32W061 DK6 board - Zigbee Coordinator

## 2.1. iMX8 board configuration 

<p>Ensure that SW801 on the IMX8 EVK board is configured for SD card boot.
For more information see the following starting guide for iMX8M EVK board: https://www.nxp.com/document/guide/getting-started-with-the-i-mx-8m-plus-evk:GS-iMX-8M-Plus-EVK. </p>

## 2.2. Zigbee NCP coprocessor board configuration

<p>The Zigbee NCP coprocessor can be either of the SoCs listed in the supported platforms. After the board is properly configured it should be connected to the iMX8M board using a 
standard micro USB cable that will be also used for power delivery to the board.</p>

### 2.2.1 K32W148 EVK/FRDM-MCXW71 board configuration

<p>For the detailed board configuration, see the “Getting Started with MCUXpresso SDK for K32W148-EVK.pdf” guide, part of the K32W148 SDK. </p>
<p>Ensure that the debug firmware on the board is J-Link. If this is not the case, follow the steps in chapter 7 of the aforementioned document to update the firmware accordingly.</p>
<p>The board should be updated with the binary image `k32w148evk_zigbee_coprocessor_bm.axf`/`frdmmcxw71_zigbee_coprocessor_bm.axf`, image which contains the Zigbee NCP. This image can be obtained from the Zigbee application wireless_examples/zigbee/zigbee_coprocessor, application that is part of the K32W148/FRDMMCXW71 SDK.</p>

### 2.2.2 K32W061 DK6 board configuration

<p>For the detailed board configuration, see the “Getting Started with MCUXpresso SDK for K32W061.pdf” guide, part of the K32W061 SDK. </p>
<p>Ensure that the debug firmware on the board is DK6 Flash Programmer. For additional information, please you the aforementioned document together with the 
DK6-UG-3127-Production-Flash-Programmer.pdf document.</p>
<p>The board should be updated with the binary image `k32w061dk6_zigbee_coprocessor_bm.axf`, image which contains the Zigbee NCP. This image can be obtained from the Zigbee application wireless_examples/zigbee/zigbee_coprocessor, application that is part of the K32W061 SDK.</p>

## 2.3. K32W061 DK6/ K32W148 EVK/ FRDM-MCXW71 board configuration (Coordinator)

<p>For the detailed K32W061 DK6 board configuration see the “Getting Started with MCUXpresso SDK for K32W061.pdf” guide, part of the K32W061 SDK.</p>
<p>For the detailed K32W148 EVK board configuration, see the “Getting Started with MCUXpresso SDK for K32W148-EVK.pdf” guide, part of the K32W148 SDK. </p>
<p>For the detailed FRDM-MCXW71 board configuration, see the “Getting Started with MCUXpresso SDK for FRDM-MCXW71.pdf” guide, part of the FRDMMCXW71 SDK. </p>

# 3. Building

<p>The building process has small differences depending on the host (iMX8 or x86) on which the Zigbee Router application is running on. The user has also the option to cross-compile the Router application under x86 Linux distribution</p>

### Environment Setup

The NCP Host offers toolchain files that can be used to compile and cross compile the applications. They are available at ZIGBEE_BASE/platform/NCP_HOST/cmake/toolchains and should be provided to cmake `-DCMAKE_TOOLCHAIN_FILE` command line argument:
-   `x86_64-linux-gnu.cmake` - toolchain file for x86 compile
-   `arm-linux` - toolchain file for imx8 cross-compile

The armgcc toolchain was obtained from official Arm GNU Toolchain website and by default the toolchain file relies on this particular configuration. User can change through environment variables and cmake arguments the default behavior. 
-   `ARMGCC_DIR` - path to installed toolchain
-   `TOOLCHAIN_NAME` - toolchain name

The Mbedtls package is required for the encryption/decryption capabilities needed to obtain a secured Serial Link. There are three options to obtain the mbedlts library, options configurable through cmake command line arguments:
-   `CONFIG_MBEDTLS_SOURCE=SDK` - Mbedtls package is obtained from MCUXPRESSO SDK 
-   `CONFIG_MBEDTLS_SOURCE=GIT` - Mbedtls package is retrieved from git official repository
-   `CONFIG_MBEDTLS_SOURCE=SYSTEM` - Mbedtls package is used as a preinstalled package


Examples for cross-compile:
-   `cmake .. -DCMAKE_TOOLCHAIN_FILE=$PWD/../../../../platform/NCP_HOST/cmake/toolchains/arm-linux.cmake -DCONFIG_MBEDTLS_SOURCE=SYSTEM` - Cross-compile for imx8 with mbedtls as a preinstalled package. The toolchain file provided detects that machine type is imx8
-   `cmake .. -DCMAKE_TOOLCHAIN_FILE=$PWD/../../../../platform/NCP_HOST/cmake/toolchains/arm-linux.cmake -DTOOLCHAIN_NAME=aarch64-linux-gnu` - Cross-compile for imx8 with toolchain available for Ubuntu 22.04


## 3.1. iMX8 platform 

<p>Create a directory `out` under the `build_linux` directory and issue the cmake command with the `MACHINE_TYPE=imx8` option. The mbedtls package is preinstalled in the provided 
Board Support Package (BSP).</p>


```
>$ cd out ; cmake .. -DMACHINE_TYPE=imx8 
-- The C compiler identification is GNU 13.2.1
-- The CXX compiler identification is GNU GNU 13.2.1
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/aarch64-none-linux-gnu-gcc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/aarch64-none-linux-gnu-c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Using preinstalled MbedTLS package /usr/local/mbedtls/cmake
-- Configuring done
-- Generating done
-- Build files have been written to: /home/zb-linux-router/zigbee/examples/zigbee_router/build_linux/out 
```
Issue the command `make` to execute the newly generated Makefile.

```
>[b06830_local@fsr-ub1864-125 out]$ make 

[  0%] Building C object CMakeFiles/pdum_static.dir/home/zb-linux-router/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum.c.o
[  1%] Building C object CCMakeFiles/pdum_static.dir/home/zb-linux-router/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum_apl.c.o
[  2%] Building C object CMakeFiles/pdum_static.dir/home/zb-linux-router/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum_dbg.c.o
[  3%] Building C object CMakeFiles/pdum_static.dir/home/zb-linux-router/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum_nwk.c.o
[  4%] Linking C static library pdum/lib/libpdum.a
[  4%] Built target ncphost-PDUM
[  5%] Building C object CMakeFiles/zb_router_linux.dir/home/zb-linux-router/zigbee/examples/zigbee_router/zigbee/examples/zigbee_router/src/linux/pdum_gen_glue.c.o
[  6%] Building C object CMakeFiles/zb_router_linux.dir/home/zb-linux-router/zigbee/examples/zigbee_router/zigbee/examples/zigbee_router/src/app_router_node_ncp.c.o
. . .
[ 99%] Building C object CMakeFiles/zb_coord_linux.dir/home/zb-linux-coord/zigbee/examples/zigbee_coordinator/zigbee/ZCL/Clusters/General/Source/OnOffCommands.c.o
[100%] Linking C executable zb_router_linux
[100%] Built target zb_router_linux
```

## 3.2. x86 platform 

<p>The Zigbee Router demo application was compiled and verified on a x86 Linux distribution (Ubuntu 22.04.2 LTS). The CMakeFile of the application determines as a prebuild step
if the application was provided as part of a MCUXPRESSO SDK package or as standalone Zigbee module. Depending on the SDK package existence, the Mbedtls can be used either from within the SDK package, as a preinstalled package or it can be obtained from official git repository (version 2.28.0). </p>

### MCUXPRESSO SDK package

<p>Create a directory `out` under the `build_linux` directory and issue the cmake command. The output will showcase the MCUXPRESSO SDK location and the mbedtls usage from within the SDK package.</p>

```
>$ cd out ; cmake ..
-- The C compiler identification is GNU 11.4.0
-- The CXX compiler identification is GNU 11.4.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Found MCUXPRESSO SDK
-- Using SDK root path /home/mcu-sdk
-- Build mbedtls from SDK source code
-- Configuring done
-- Generating done
-- Build files have been written to: /home/mcu-sdk/middleware/wireless/zigbee/examples/zigbee_router/build_linux/out 
```

Issue the command `make` to execute the newly generated Makefile.

```
>[b06830_local@fsr-ub1864-125 out]$ make 

[  1%] Building C object CMakeFiles/pdum_static.dir/home/mcu-sdk/middleware/wireless/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum.c.o
[  1%] Building C object CMakeFiles/pdum_static.dir/home/mcu-sdk/middleware/wireless/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum_apl.c.o
[  2%] Building C object CMakeFiles/pdum_static.dir/home/mcu-sdk/middleware/wireless/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum_dbg.c.o
[  2%] Building C object CMakeFiles/pdum_static.dir/home/mcu-sdk/middleware/wireless/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum_nwk.c.o
[  3%] Linking C static library pdum/lib/libpdum.a
[  3%] Built target ncphost-PDUM
[  3%] Building C object mbedtls/library/CMakeFiles/ncp-host-mbedcrypto.dir/aes.c.o
. . .
[ 99%] Building C object CMakeFiles/zb_router_linux.dir/home/mcu-sdk/middleware/wireless/zigbee//examples/zigbee_router/zigbee/ZCL/Clusters/General/Source/OnOffCommands.c.o
[100%] Linking C executable zb_router_linux
[100%] Built target zb_router_linux
```

The following error might appear, which indicates that the SDK Mbedtls version is too old: 

```
-- SDK MBEDTLS version is below 2.28, use CONFIG_MBEDTLS_SOURCE as GIT or SYSTEM instead
CMake Error at /home/mcu-sdk/middleware/wireless/zigbee/platform/NCP_HOST/cmake/NxpZbNcpHostConfig.cmake:46 (message):
Call Stack (most recent call first):
  CMakeLists.txt:32 (include)
```

If so, switch the Mbedtls origin to GIT instead, by building with the following commands:

```
>$ cd out ; cmake -DCONFIG_MBEDTLS_SOURCE=GIT ..
-- The C compiler identification is GNU 11.4.0
-- The CXX compiler identification is GNU 11.4.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Found MCUXPRESSO SDK internal
-- Using SDK root path /home/mcu-sdk
-- Populate mbedtls repository
Cloning into 'repo'...
HEAD is now at 8b3f26a5ac Merge pull request #868 from ARMmbed/mbedtls-2.28.0rc0-pr
...

-- Found Python3: /usr/bin/python3.10 (found version "3.10.12") found components: Interpreter
-- Performing Test C_COMPILER_SUPPORTS_WFORMAT_SIGNEDNESS
-- Performing Test C_COMPILER_SUPPORTS_WFORMAT_SIGNEDNESS - Success
-- Looking for pthread.h
-- Looking for pthread.h - found
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD - Success
-- Found Threads: TRUE


-- Configuring done
-- Generating done
-- Build files have been written to: /home/zb-linux-router/zigbee/examples/zigbee_router/build_linux/out 
```

Then issue the command `make` as usual to execute the newly generated Makefile.

### Standalone Zigbee module

<p>The Zigbee Router application can be obtained and compiled as a standalone application, without the presence of a MCUXPRESSO SDK. The mbedtls package is required as a
preinstalled package or can be configured through user environment variables `export MBEDTLS_ORIGIN=GIT` to be obtained from official repository.</p>

Create a directory `out` under the `build_linux` directory and issue the cmake command. 

```
>$ cd out ; cmake ..
-- The C compiler identification is GNU 11.4.0
-- The CXX compiler identification is GNU 11.4.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Populate mbedtls repository
Cloning into 'repo'...
HEAD is now at 8b3f26a5ac Merge pull request #868 from ARMmbed/mbedtls-2.28.0rc0-pr
...


-- Found Python3: /usr/bin/python3.10 (found version "3.10.12") found components: Interpreter
-- Performing Test C_COMPILER_SUPPORTS_WFORMAT_SIGNEDNESS
-- Performing Test C_COMPILER_SUPPORTS_WFORMAT_SIGNEDNESS - Success
-- Looking for pthread.h
-- Looking for pthread.h - found
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD - Success
-- Found Threads: TRUE


-- Configuring done
-- Generating done
-- Build files have been written to: /home/zb-linux-router/zigbee/examples/zigbee_router/build_linux/out 
```

Issue the command `make` to execute the newly generated Makefile.

```
>[b06830_local@fsr-ub1864-125 out]$ make 

[  0%] Building C object CMakeFiles/pdum_static.dir/home/zb-linux-router/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum.c.o
[  1%] Building C object CCMakeFiles/pdum_static.dir/home/zb-linux-router/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum_apl.c.o
[  2%] Building C object CMakeFiles/pdum_static.dir/home/zb-linux-router/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum_dbg.c.o
[  3%] Building C object CMakeFiles/pdum_static.dir/home/zb-linux-router/zigbee/platform/NCP_HOST/framework/PDUM/Source/pdum_nwk.c.o
[  4%] Linking C static library pdum/lib/libpdum.a
[  4%] Built target ncphost-PDUM
[  5%] Building C object CMakeFiles/zb_coord_linux.dir/home/zb-linux-router/zigbee/examples/zigbee_router/zigbee/examples/zigbee_router/src/linux/pdum_gen_glue.c.o
[  6%] Building C object CMakeFiles/zb_coord_linux.dir/home/zb-linux-router/zigbee/examples/zigbee_router/zigbee/examples/zigbee_router/src/app_router_node_ncp.c.o
. . .
[ 99%] Building C object CMakeFiles/zb_coord_linux.dir/home/zb-linux-coord/zigbee/examples/zigbee_coordinator/zigbee/ZCL/Clusters/General/Source/OnOffCommands.c.o
[100%] Linking C executable zb_router_linux
[100%] Built target zb_router_linux
```

# 4. Running the application

## 4.1. Forming the network

On the Zigbee Coordinator, issue the form command on the serial port:

```
eOTA_NewImageLoaded status = 1Recovered Application State 0 On Network 0
form
APP-EVT: Event 8, NodeState=0
APP-EVT: Request Nwk Formation 00
APP-BDB: NwkFormation Success 
APP-ZDO: Network started Channel = 12
```
## 4.2. Steering the network

On the Zigbee Coordinator, issue the steer command on the serial port:

```
steer
APP-EVT: Event 6, NodeState=10
APP-BDB: NwkSteering Success
APP-EVT: Request Nwk Steering 00
```

## 4.3. Starting the Router

<p>Make sure that the iMX8 board is connected to the Zigbee NCP coprocessor board as in the picture below. The two boards are connected through a USB cable (micro-USB for K32W148 EVK board and mini-USB for K32W061 DK6 board), between the MCU-Link connected on the K32W148/K32W061 board (marked as such on the silk screen), and the OTG connector on the iMX8 board.</p>

<p>The Linux Zigbee Router allows the user to specify the serial port on which the NCP coprocessor board is connected. Usually on the iMX8 board, this is /dev/ttyACM0 for K32W148 and /dev/ttyUSB0 for K32W061. For the K32W061 based NCP coprocessor, the user should pay attention and modify the default latency timer (255msec) to a smaller value (it should be below 16msec). </p>

Command to display default latency timer for ttyUSB interface: `cat /sys/bus/usb-serial/devices/ttyUSB0/latency_timer`

Command to modify default latency timer for ttyUSB interface: `echo 1 | sudo tee /sys/bus/usb-serial/devices/ttyUSB0/latency_timer`.

Example to start the Zigbee NCP Router:

```
>root@ubuntu:~ ./zb_router_linux /dev/ttyACM0 
[0] Created NCP Host Task with pid 451116
[0] MAIN
[0] ZQ: Initialised a queue: Handle=ad045b60 Length=30 ItemSize=104
[0] ZQ: Initialised a queue: Handle=ad0460c0 Length=30 ItemSize=8
[0] ZQ: Initialised a queue: Handle=ad046100 Length=35 ItemSize=8
[0] ZQ: Initialised a queue: Handle=ad046080 Length=2 ItemSize=64
[0] ZQ: Initialised a queue: Handle=ad04cac0 Length=20 ItemSize=8
[0] ZQ: Initialised a queue: Handle=ad04cb10 Length=38 ItemSize=8
[0] serial Link initialised
[0] eStatusReportReload = 1
[211] New max process gap 1
[214] New max process gap 2
[228] Pkt Type 003a Set New Max Response Time 16
[276] Pkt Type 9094 Set New Max Response Time 17
[819] Pkt Type 0012 Set New Max Response Time 47
[1029] New max process gap 210
[1379] Start Up State 0 On Network 0
>
```

## 4.4. Joining the network

Once the network is created by the Zigbee Coordinator, the Zigbee NCP Router should join without any additional commands.

```
[5777] BDB: vNsTryNwkJoin - try 0 index 0 of 1 Nwks 
[5792] BDB: Try To join 0060377791d75e0f on Ch 21
[5825] APP-ZDO: Discovery Complete 00
[6463] Nwk formation/joining took 6463 MS
...
[7104] Nwk Join Success
[7135] APP: NwkSteering Success 
[7135] APP: BDB_EVENT_REJOIN_SUCCESS 
[7135] APP-ZDO: Trust Center Status 00
```

## 4.5. Find and Bind

<p>Since the Zigbee Router in this demo is behaving as a light bulb, we need to bind its On/Off cluster (server) to the Zigbee Coordinator On/Off cluster (client) in order to receive reports and be able to toggle it. This is done based on the BDB Find & Bind procedure, where the Zigbee Coordinator is the initiator and the Zigbee Router is the target. For the first step we’ll use the `find` command, while the `bind` is going to be automatically done by the Zigbee Coordinator, since the On/Off cluster is the cluster of interest.</p>

<p>To kick off the F&B procedure, the user needs to enter the `find` command (case insensitive) into the Zigbee Coordinator serial port and the corresponding output:</p>

```
find 
APP-EVT: Event 7, NodeState=10
APP-EVT: Find and Bind initiate 00
```

<p>After this, the find command needs to be issued on the Router console: </p>

```
[43121] Find
[43121] ZPR: App event 7, NodeState=10
[43129] APP: NwkSteering Success 
[43129] APP: BDB_EVENT_REJOIN_SUCCESS 
[43593] Update Id 0003
[44617] Update Id 0003
[44617] Update Id 0003
...
```

<p> Once this is done, the coordinator will output the following: </p>

```
APP-BDB: F&B Simple Desc response From cd7f Profle 0104 Device 0000 Ep 1 Version 0
APP-BDB: Check For Binding Cluster 0006
APP-BDB: Bind Created for cluster 0006
Remote Bind Dst addr cd7f, Ieee Dst Addr 0060377791d75e0f Ieee Src 00158d00031f1224
Sending a remote bind request Status =0
APP-BDB: Bind Created for target EndPt 1
```

<p> In turn, the Router will output the following, which indicates that the bind took place. </p>

```
[45472] BDB: APP_vGenCallback [0 17] 
[45472] APP-ZDO: Zdo Bind event
```
## 4.6. Toggle commands

<p>The Zigbee Router is sending periodic reports regarding the state of the cluster (on/off), as well as reports when there’s a change request state from the initiator.</p>

```
ZCL Attribute Report: Cluster 0006 Attribute 0000 Value 0
ZCL Attribute Report: Cluster 0006 Attribute 0000 Value 0
...
```
<p>In order to change the state of the cluster, the user needs to enter the command `toggle` (case insensitive) in the Zigbee Coordinator console, as per the example below: </p>

```
...
ZCL Attribute Report: Cluster 0006 Attribute 0000 Value 0
toggle
APP-EVT: Event 5, NodeState=10
APP-EVT: Send Toggle Cmd
ZCL Attribute Report: Cluster 0006 Attribute 0000 Value 1
ZCL Attribute Report: Cluster 0006 Attribute 0000 Value 1
```

<p> Notice in the log of the Coordinator that the value field now became 1 as a result of the toggle. </p>