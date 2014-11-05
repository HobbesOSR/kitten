BASE=..
include $(BASE)/Makefile.header

PROGRAMS = hello_world

hello_world_SOURCES = hello_world.c sched_getcpu.c
hello_world_LDADD   = -llwk -lpthread
#ifdef CONFIG_TASK_MEAS
hello_world_LDADD   += -lm
#endif

# Embed an ISO image to use for booting guest OSes in the hello_world
# ELF executable.  The data is placed in the .rawdata ELF section, which
# starts at the address specified in the CC_LDFLAGS passed to the linker.
ifdef CONFIG_GUEST_OS_ISOIMAGE_PATH
hello_world_RAWDATA = $(shell echo $(CONFIG_GUEST_OS_ISOIMAGE_PATH))
CC_LDFLAGS          += -Wl,--section-start -Wl,.rawdata=0x1000000
endif

include $(BASE)/Makefile.footer
