BASE=..
include $(BASE)/Makefile.header

PROGRAMS = powerinsight

powerinsight_SOURCES = powerinsight.c
powerinsight_LDADD   = -llwk -lpthread

# Embed an ISO image to use for booting guest OSes in the powerinsight
# ELF executable.  The data is placed in the .rawdata ELF section, which
# starts at the address specified in the CC_LDFLAGS passed to the linker.
ifdef CONFIG_GUEST_OS_ISOIMAGE_PATH
powerinsight_RAWDATA = $(shell echo $(CONFIG_GUEST_OS_ISOIMAGE_PATH))
CC_LDFLAGS          += -Wl,--section-start -Wl,.rawdata=0x1000000
endif

include $(BASE)/Makefile.footer
