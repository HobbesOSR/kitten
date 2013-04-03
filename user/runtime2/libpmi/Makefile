BASE=../../
include $(BASE)/Makefile.header

LIBRARIES = libpmi.a
HEADERS   = pmi.h

PROGRAMS  = test_pmi_hello

libpmi_SOURCES = pmi.c simple_pmiutil.c

test_pmi_hello_SOURCES = test_pmi_hello.c
test_pmi_hello_LDADD   = -lpmi -lportals_util -lportals -lxpmem -lrt -lpthread

CFLAGS  += -std=gnu99
INCDIRS += -I../pct
LIBDIRS += -L.

include $(BASE)/Makefile.footer
