#
# Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
#

CSRC = src/main.c

UINCDIR = src/sys

UDEFS =

# CANaerospace
include ../../canaerospace/canaerospace/embedded_rules.mk
include ../../canaerospace/drivers/stm32/rules.mk
CANAEROSPACE_DEF += -DCANAEROSPACE_DEBUG=1
CSRC += $(CANAEROSPACE_SRC) $(CAN_SRC)
UINCDIR += $(CANAEROSPACE_INC) $(CAN_INC)
UDEFS += $(CANAEROSPACE_DEF) $(CAN_DEF) -DCAN_CHIBIOS=1

# System stuff
CSRC += src/sys/board.c src/sys/sys.c \
        spl/src/stm32f10x_can.c       \
        spl/src/stm32f10x_rcc.c       \
        spl/src/stm32f10x_tim.c

UINCDIR += spl/inc

UDEFS += -DCORTEX_ENABLE_WFI_IDLE=1 -DSTDOUT_SD=SD2 -DSTDIN_SD=STDOUT_SD

# ---------------------------

PROJECT = canas_demo

USE_OPT = -falign-functions=16 --std=c99
USE_COPT =
USE_CPPOPT = -fno-rtti

RELEASE ?= 0
ifneq ($(RELEASE),0)
    DDEFS += -DRELEASE
    USE_OPT += -O1 -fomit-frame-pointer
else
    DDEFS += -DDEBUG
    USE_OPT += -O0 -ggdb
endif

USE_LINK_GC = yes
USE_THUMB = yes
USE_VERBOSE_COMPILE = no
USE_FWLIB = no

# asm statement fix
DDEFS += -Dasm=__asm

# ---------------------------

CHIBIOS = chibios
include $(CHIBIOS)/os/hal/platforms/STM32F1xx/platform_f105_f107.mk
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/ports/GCC/ARMCMx/STM32F1xx/port.mk
include $(CHIBIOS)/os/kernel/kernel.mk

LDSCRIPT= $(PORTLD)/STM32F107xC.ld

VARIOUSSRC = $(CHIBIOS)/os/various/syscalls.c

CSRC += $(PORTSRC) $(KERNSRC) $(HALSRC) $(PLATFORMSRC) $(VARIOUSSRC)

ASMSRC = $(PORTASM)

INCDIR = $(PORTINC) $(KERNINC) $(HALINC) $(PLATFORMINC) $(CHIBIOS)/os/various

# ---------------------------

MCU  = cortex-m3

TRGT = arm-none-eabi-
CC   = $(TRGT)gcc
CPPC = $(TRGT)g++
LD   = $(TRGT)gcc
#LD   = $(TRGT)g++
CP   = $(TRGT)objcopy
AS   = $(TRGT)gcc -x assembler-with-cpp
OD   = $(TRGT)objdump
HEX  = $(CP) -O ihex
BIN  = $(CP) -O binary

# ARM-specific options here
AOPT =

# THUMB-specific options here
TOPT = -mthumb -DTHUMB

CWARN = -Wall -Wextra -Werror -Wstrict-prototypes
CPPWARN = -Wall -Wextra -Werror

include $(CHIBIOS)/os/ports/GCC/ARMCMx/rules.mk
