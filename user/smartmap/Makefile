BASE=..
include $(BASE)/Makefile.header

PROGRAMS = smartmap_app smartmap_loader

smartmap_loader_SOURCES = loader.c
smartmap_loader_LDADD   = -llwk -lpthread

smartmap_app_SOURCES = smartmap_app.c

# Embed the smartmap_app ELF executable in the loader's ELF image
smartmap_loader_RAWDATA = smartmap_app
CC_LDFLAGS += -Wl,--section-start -Wl,.rawdata=0x1000000

include $(BASE)/Makefile.footer
