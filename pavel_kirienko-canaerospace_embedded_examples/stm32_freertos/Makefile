#
# Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
#

include FreeRTOS/rules.mk
include ../../canaerospace/canaerospace/embedded_rules.mk
include ../../canaerospace/drivers/stm32/rules.mk

CANAEROSPACE_DEF += -DCANAEROSPACE_DEBUG=1

SPL_SRC = periphery/src/stm32f10x_gpio.c periphery/src/stm32f10x_rcc.c periphery/src/stm32f10x_tim.c \
          periphery/src/stm32f10x_can.c periphery/src/stm32f10x_usart.c periphery/src/misc.c

SRC = $(SPL_SRC) $(FREERTOS_SRC) $(CAN_SRC) $(CANAEROSPACE_SRC) \
      cmsis/core_cm3.c cmsis/system_stm32f10x.c cmsis/cm3_exceptions.c \
      src/main.c src/sys.c src/newlib_stubs.c src/util.c \
      src/srvport/srvport.c src/sensors.c

ASMFILES = cmsis/startup_stm32f10x_cl.s

INC = $(FREERTOS_INC) -I$(CANAEROSPACE_INC) $(addprefix -I,$(CAN_INC)) -Icmsis -Iperiphery/inc/
LIB = -lgcc -lc -lm

FLAGS  = -mthumb -mcpu=cortex-m3 -g3
AFLAGS = $(FLAGS)
CFLAGS = $(FLAGS) -ffunction-sections -fdata-sections -Wall -Wextra -Werror -pedantic -std=c99

DEF = $(FREERTOS_DEF) $(CAN_DEF) $(CANAEROSPACE_DEF) \
      -DCAN_FREERTOS -DUSE_STDPERIPH_DRIVER -DSTM32F10X_CL -DHSE_VALUE=25000000 \
      -DFREERTOS -DUSE_FULL_ASSERT -DDEBUG_BUILD

LDFLAGS = $(FLAGS) $(LIB) -nostartfiles -Tstm32.ld -Xlinker --gc-sections

# ---------------

COBJ = $(SRC:.c=.o)
SOBJ = $(ASMFILES:.s=.o)
OBJ  = $(COBJ) $(SOBJ)

CROSS_COMPILE = arm-none-eabi-
CC   = $(CROSS_COMPILE)gcc
AS   = $(CROSS_COMPILE)as
LD   = $(CROSS_COMPILE)gcc
CP   = ${CROSS_COMPILE}objcopy
SIZE = ${CROSS_COMPILE}size

# ---------------

all: output.elf output.bin size

output.bin: output.elf
	$(CP) -O binary output.elf $@

output.elf: $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $@

$(COBJ): %.o: %.c
	$(CC) -c $(DEF) $(INC) $(CFLAGS) $< -o $@

$(SOBJ): %.o: %.s
	$(AS) -c $(AFLAGS) $< -o $@

clean:
	rm -f output.elf output.bin $(OBJ)

size:
	@echo $(MAKEFILE_LIST)
	@if [ -f output.elf ]; then echo; $(SIZE) $(SOBJ) $(COBJ) -t; echo; fi;

sizex:
	$(SIZE) $(SOBJ) $(COBJ) -t
	@echo
	$(SIZE) output.elf -A

# If you're using ST-Link:
stflash: output.bin
	st-flash write output.bin 8000000

.PHONY: all clean size sizex stflash
