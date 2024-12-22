# Furnace Logger using Arduino Uno

This project implements a microcontroller-based data logger that will
monitor the thermostat control lines of my gas forced-air furnace.
The goal is to collect some information on how much energy is required
to keep my home at a comfortable temperature on the coldest winter days.
The duty cycle of the furnace can be used along with the specified
output capacity of my furnace to get a baseline on the energy efficiency
of the house, along with a starting point for sizing a heat pump
replacement unit.

## Hardware

The
[Arduino Uno](https://docs.arduino.cc/hardware/uno-rev3/)
microcontroller board was chosen due to the low friction present in
implementing a logging system, particularly when combined with the
[Adafruit Data Logger Shield](https://learn.adafruit.com/adafruit-data-logger-shield)
for SD card storage of log files, and a real-time clock for timestamping
of the data.

For interfacing to the 24VAC furnace control signals, I chose a
[3 channel AC optocoupler module](https://www.amazon.com/dp/B0CHJNRZMW)
from Amazon.com.
The board came with 150k-ohm resistors in place to allow sensing of 220VAC
signals, and I replaced those with 15k-ohm 1W resistors to scale the inputs
down to the 24VAC of my system.
The output of the board is then directly interfaced to the Olimexino-STM32.

## Software

The software is based on the Arduino IDE and libraries from Adafruit.

### User Interface

TBD
