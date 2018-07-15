#
# Usage:
#   make install [INSTALL_PREFIX=<prefix>]
#
# This makefile will install the static library and its header to the directories under <prefix>.
# Default installation prefix is "/usr/local", and it's not recommended to change it.
#

INSTALL_PREFIX ?= /usr/local
install_path_include = $(INSTALL_PREFIX)/include/canaerospace_drivers/socketcan/
install_path_lib     = $(INSTALL_PREFIX)/lib/
install_path_exec    = $(INSTALL_PREFIX)/bin/

FLAGS ?= -O1 -Wall -Wextra -Werror -pedantic -std=gnu99

all:
	gcc $(FLAGS) -c socketcan.c -o socketcan.o
	ar rcs libcanaerospace_socketcan.a socketcan.o
	rm -rf *.o

install: all
	mkdir -p $(install_path_include)
	install -m 0644 *.h $(install_path_include)
	install -m 0755 libcanaerospace_socketcan.a $(install_path_lib)
	install -m 0755 slcan_init $(install_path_exec)

clean:
	- rm -rf *.o *.a

.PHONY: all install clean
