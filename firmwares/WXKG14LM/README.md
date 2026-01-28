- [Overview](#overview)
  - [DIOs definition](#dios-definition)
  - [Board](#board)
  - [Circuit diagrams](#circuit-diagrams)
- [Build](#build)

# Overview
[WXKG14LM](https://www.zigbee2mqtt.io/devices/WXKG14LM.html) - Wireless remote switch H1 (single rocker)

## DIOs definition
TODO: UPDATE PINS andd add photos + circuit diagrams
- LED - PIO 0
- Button - PIO 12

## Board
![Board Front](/images/WXKG14LM/board_front.png)
![Board Back](/images/WXKG14LM/board_back.png)

## Circuit diagrams
LEDs circuit: </br>
![LEDs circuit](/images/WXKG14LM/leds_circuit.png)

Buttons circuit:</br>
![Buttons circuit](/images/WXKG14LM/buttons_circuit.png)

# Build
To build firmware for this device, select preset `WXKG14LM` in CMake configuration.
