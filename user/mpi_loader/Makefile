BASE=..
include $(BASE)/Makefile.header

PROGRAMS = app mpi_loader

mpi_loader_SOURCES = mpi_loader.c
mpi_loader_LDADD   = -llwk -lpthread

app_SOURCES = app.c

# Embed the app ELF executable in the mpi_loader's ELF image
# Change this to point to your MPI executable
mpi_loader_RAWDATA = app
#mpi_loader_RAWDATA = /home/ktpedre/kitten_ompi/nvl/src/guests/simple_busybox/src/hpcg/bin/xhpcg

CC_LDFLAGS += -Wl,--section-start -Wl,.rawdata=0x1000000

include $(BASE)/Makefile.footer
