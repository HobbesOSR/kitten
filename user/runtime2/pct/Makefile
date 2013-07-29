BASE=../../
include $(BASE)/Makefile.header

PROGRAMS = pct
HEADERS  = pct.h

# Put pct.h in include/lwk dir
HEADERS_SUBDIR = lwk

pct_SOURCES = \
	pct.c \
	malloc.c \
	pmi/common.c \
	pmi/string.c \
	pmi/pmip_pmi_v1.c \
	pmi/pmi_server.c \

CFLAGS  += -std=gnu99 -D_GNU_SOURCE -DHAVE_KITTEN -Wall -pthread
INCDIRS += -Ipmi

pct_LDADD = -lportals -lportals_ppe -lportals_util -lxpmem -llwk -lrt
ifeq ($(PIAPI),y)
CFLAGS    += -DUSING_PIAPI
pct_LDADD += -lpiapi
endif
pct_LDADD += -lpthread

# Embed the app ELF executable in the PCT's ELF image
pct_RAWDATA = ../libpmi/test_pmi_hello
CC_LDFLAGS     += -Wl,--section-start -Wl,.rawdata=0x1000000 -Wl,--allow-multiple-definition

include $(BASE)/Makefile.footer
