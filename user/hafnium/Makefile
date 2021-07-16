BASE=..
include $(BASE)/Makefile.header

PROGRAMS = hafnium

hafnium_SOURCES = hafnium.c sched_getcpu.c
hafnium_LDADD   = -llwk -lpthread

include $(BASE)/Makefile.footer

