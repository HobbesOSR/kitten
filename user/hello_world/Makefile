CONFIG=../../.config
include ../Makefile.header

PROGRAMS = hello_world

INCLUDES += \
	-I../liblwk/include \
	-I../../include \

hello_world_SOURCES = hello_world.c
hello_world_LDADD   = ../liblwk/liblwk.a -lpthread

# Embed an ISO image to use for booting guest OSes in the hello_world
# ELF executable.  The data is placed in the .rawdata ELF section, which
# starts at the address specified in the CC_LDFLAGS passed to the linker.
ifdef CONFIG_GUEST_OS_ISOIMAGE_PATH
hello_world_RAWDATA = $(shell echo $(CONFIG_GUEST_OS_ISOIMAGE_PATH))
CC_LDFLAGS          += -Wl,--section-start -Wl,.rawdata=0x1000000
endif

include ../Makefile.footer
