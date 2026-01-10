- [Overview](#overview)
  - [DIOs definition](#dios-definition)
  - [Board](#board)
  - [Circuit diagrams](#circuit-diagrams)
- [Build](#build)

# Overview
[WXKG15LM](https://www.zigbee2mqtt.io/devices/WXKG15LM.html) - Wireless mini switch

## DIOs definition
- Left LED - PIO 0
- Right LED - PIO 15
- Left button - PIO 12
- Right button - PIO 14

## Board
![Board Front](/images/WXKG15LM/board_front.png)
![Board Back](/images/WXKG15LM/board_back.png)

## Circuit diagrams
LEDs circuit: </br>
![LEDs circuit](/images/WXKG15LM/leds_circuit.png)

Buttons circuit:</br>
![Buttons circuit](/images/WXKG15LM/buttons_circuit.png)

# Build
To build firmware for this device, select preset `WXKG15LM` in CMake configuration.
