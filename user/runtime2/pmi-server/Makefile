BASE=../..
include $(BASE)/Makefile.header

PROGRAMS  = server

PMI_COMMON_BASE=../pmi-common
server_SOURCES = \
    server.c \
    ${PMI_COMMON_BASE}/common.c \
    ${PMI_COMMON_BASE}/string.c \
    ${PMI_COMMON_BASE}/pmip_pmi_v1.c \
    ${PMI_COMMON_BASE}/pmi_server.c

server_LDADD   	   = -lpmi -lportals_util -lportals -lxpmem -lrt -lpthread

CFLAGS  += -std=gnu99
INCDIRS += -I../pct -I../pmi-common/
LIBDIRS += -L.

# We are building a linux executable
TARGET=linux

include $(BASE)/Makefile.footer
