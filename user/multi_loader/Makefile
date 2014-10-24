BASE=..
include $(BASE)/Makefile.header

PROGRAMS = app1 app2 loader

loader_SOURCES = loader.c
loader_LDADD   = -llwk -lpthread

app1_SOURCES = app1.c
app2_SOURCES = app2.c

# Embed the app ELF executables in the loader's ELF image
loader_RAWDATA  = app1
loader_RAWDATA2 = app2

CC_LDFLAGS += -Wl,--section-start -Wl,.rawdata=0x1000000 -Wl,--section-start -Wl,.rawdata2=0x1400000 -Wl,--allow-multiple-definition

include $(BASE)/Makefile.footer
