BASE=..
include $(BASE)/Makefile.header

PROGRAMS = pisces

pisces_SOURCES = pisces.c sched_getcpu.c xpmem-pisces.c
pisces_LDADD   = -llwk -lpthread -lxpmem-ext

include $(BASE)/Makefile.footer

