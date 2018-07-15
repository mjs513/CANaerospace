
dir_freertos := $(dir $(lastword $(MAKEFILE_LIST)))

FREERTOS_SRC := $(dir_freertos)/Source/list.c   \
                $(dir_freertos)/Source/queue.c  \
                $(dir_freertos)/Source/tasks.c  \
                $(dir_freertos)/Source/timers.c \
                $(dir_freertos)/Source/portable/GCC/ARM_CM3/port.c \
                $(dir_freertos)/Source/portable/MemMang/heap_3.c

FREERTOS_INC := -I$(dir_freertos) \
                -I$(dir_freertos)/Source/include

FREERTOS_DEF := -DGCC_ARMCM3
