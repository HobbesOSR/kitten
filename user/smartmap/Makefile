CONFIG=../../.config
include ../Makefile.header

PROGRAMS = smartmap_app loader

INCLUDES += \
	-I../liblwk/include \
	-I../../include \
	-I$O/../../include \
	-I$O/../../include2 \

loader_SOURCES = loader.c
loader_LDADD   = ../liblwk/liblwk.a

smartmap_app_SOURCES = smartmap_app.c

# Embed the smartmap_app ELF executable in the loader's ELF image
loader_RAWDATA = smartmap_app
CC_LDFLAGS     += -Wl,--section-start -Wl,.rawdata=0x1000000

include ../Makefile.footer
