BASE=..
include $(BASE)/Makefile.header

ifdef CONFIG_SCHED_EDF
CFLAGS += -DCONFIG_SCHED_EDF
endif

LIBRARIES = liblwk.a
HEADERS   = liblwk.h

# Put liblwk.h in include/lwk dir
HEADERS_SUBDIR = lwk

liblwk_SOURCES = syscalls.c pmem.c elf.c aspace.c

include $(BASE)/Makefile.footer
