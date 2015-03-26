BASE=..
include $(BASE)/Makefile.header

ifdef CONFIG_SCHED_EDF
PROGRAMS = edf_sched
CFLAGS += -DCONFIG_SCHED_EDF
endif

edf_sched_SOURCES = edf_sched.c
edf_sched_LDADD   = -llwk -lpthread

include $(BASE)/Makefile.footer
