BASE=..
include $(BASE)/Makefile.header

ifdef CONFIG_SCHED_EDF
PROGRAMS = coop_sched
CFLAGS += -DCONFIG_SCHED_EDF
endif

coop_sched_SOURCES = coop_sched.c
coop_sched_LDADD   = -llwk -lpthread

include $(BASE)/Makefile.footer
