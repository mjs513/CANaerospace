
can_dir := $(dir $(lastword $(MAKEFILE_LIST)))

CAN_SRC := $(can_dir)can_driver.c \
           $(can_dir)can_filter.c \
           $(can_dir)can_timer.c  \
           $(can_dir)can_selftest.c

CAN_INC := $(can_dir)
