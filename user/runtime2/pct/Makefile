BASE=../../
include $(BASE)/Makefile.header

PROGRAMS = pct
HEADERS  = pct.h

# Put pct.h in include/lwk dir
HEADERS_SUBDIR = lwk

PMI_COMMON_BASE=../pmi-common

pct_SOURCES = \
	pct.c \
	malloc.c \
	${PMI_COMMON_BASE}/common.c \
	${PMI_COMMON_BASE}/string.c \
	${PMI_COMMON_BASE}/pmip_pmi_v1.c \
	${PMI_COMMON_BASE}/pmi_server.c

CFLAGS  += -std=gnu99 -D_GNU_SOURCE -DHAVE_KITTEN -Wall -pthread
INCDIRS += -I${PMI_COMMON_BASE}

pct_LDADD = -lportals -lportals_ppe -lportals_util -lxpmem -llwk -lrt
ifeq ($(PIAPI),y)
CFLAGS    += -DUSING_PIAPI
pct_LDADD += -lpiapi
endif
pct_LDADD += -lpthread

# Embed the app ELF executable in the PCT's ELF image
# pct_RAWDATA = ../libpmi/test_pmi_hello
pct_RAWDATA = ../../../init_task
CC_LDFLAGS     += -Wl,--section-start -Wl,.rawdata=0x1000000 -Wl,--allow-multiple-definition

include $(BASE)/Makefile.footer
