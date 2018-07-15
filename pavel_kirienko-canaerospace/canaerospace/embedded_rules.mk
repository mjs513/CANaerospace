#
# This makefile should be included in order to build the libcanaerospace within the client application.
#

_thisdir := $(dir $(lastword $(MAKEFILE_LIST)))

CANAEROSPACE_SRC := $(_thisdir)/src/core.c    \
                    $(_thisdir)/src/list.c    \
                    $(_thisdir)/src/marshal.c \
                    $(_thisdir)/src/service.c \
                    $(_thisdir)/src/util.c    \
                    $(_thisdir)/src/generic_redundancy_resolver.c \
                    \
                    $(_thisdir)/src/services/std_data_upload_download.c \
                    $(_thisdir)/src/services/std_flashprog.c \
                    $(_thisdir)/src/services/std_identification.c \
                    $(_thisdir)/src/services/std_nodesync.c

CANAEROSPACE_INC := $(_thisdir)/include/

CANAEROSPACE_DEF :=
