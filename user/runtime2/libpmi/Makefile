BASE=../../
include $(BASE)/Makefile.header

LIBRARIES = libpmi.a
HEADERS   = pmi.h

PROGRAMS  = test_pmi_hello

PMI_COMMON_BASE=../pmi-common
libpmi_SOURCES = pmi.c ${PMI_COMMON_BASE}/simple_pmiutil.c

test_pmi_hello_SOURCES = test_pmi_hello.c
test_pmi_hello_LDADD   = -lpmi -lportals_util -lportals -lxpmem -lrt -lpthread

CFLAGS  += -std=gnu99
INCDIRS += -I../pct -I../pmi-common/
LIBDIRS += -L.

# Install in both kitten and linux install directories
TARGET=both

include $(BASE)/Makefile.footer
