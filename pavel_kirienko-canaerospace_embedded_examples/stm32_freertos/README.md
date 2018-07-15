# Target platform
This example is for STM32F107/105. It includes FreeRTOS v7.3 and part of STM32 Standard Peripheral Library.

If you are lucky enough you even can get a description for the target hardware [here][1],
otherwise consider the schematics in the PDF file attached.

# How to build
Clone the main repository and the examples at the same directory, otherwise you will need to adjust the relative
pathes in the makefile.

Supported toolchains: 
- Sourcery CodeBench Lite
- GCC ARM
- Probably any GCC-based toolchain

# CAN driver
This example uses the STM32 CAN driver from the main repository.
The driver itself supports a more complete list of STM32 microcontrollers:

- STM32F105
- STM32F107
- STM32F4x
- Probably any STM32 with CAN1, CAN2 and TIM2

Note that the driver relies on the STM32 SPL library, so your application must be linked against it in order
to use this driver. The following components are needed:

- CAN
- RCC
- Timers
- misc (for NVIC)

[1]: http://www.terraelectronica.ru/news_made.php?ID=15
