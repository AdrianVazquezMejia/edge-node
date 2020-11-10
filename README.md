# Description

This software is the code for an ESP-32 MCU that will work as an edge node in a network used to monitor energy consumption.

# Installation

* Clone the repo `git clone https://gitlab.com/electr-nica/desarrollohardware/edge-node`

* Go to the project folder `cd edge-node`

* Build menuconfig `make menuconfig`

* Go to  __Serial Flash Cofig__  and set your serial port to flash

* Go to __Custom Config__  to choose the mode and parameters of the RTU

# Hardware specs

## Uart IOs for RS-485

	```c
	#define TXD_PIN     33
	#define RXD_PIN     26
	#define RTS_PIN     27
	#define CTS_PIN     19
	```
	
## Uart IOs for LoRa

	```c
	#define TXD_PIN     14
	#define RXD_PIN     15
	```