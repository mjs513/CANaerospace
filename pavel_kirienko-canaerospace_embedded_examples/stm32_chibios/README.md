# Application

This example features simple application that periodically broadcasts IDS request and prints each response into the serial port.

# Target platform
This example is for STM32F107/105. It includes ChibiOS v2.6.1 and part of STM32 Standard Peripheral Library.

The hardware is the same as for FreeRTOS example.

# How to build
Clone libcanaerospace and examples into the same directory, then `make`.

Supported toolchains: 
- GCC ARM
- Probably any GCC-based toolchain

Note that the CAN driver relies on the STM32 SPL library, so your application must be linked against it in order
to use this driver. The following components are needed:

- CAN
- RCC
- Timers
