
CC=gcc
CPPFLAGS = -Wall
CPPFLAGS += -I../../../include
CPPFLAGS += -I../../liblwk/include

libinit_ompi_C_SOURCE = init_ompi.c 

libinit_ompi_OBJS=$(libinit_ompi_C_SOURCE:.c=.o)

libinit_ompi.a: $(libinit_ompi_OBJS) 
	ar rcs $@ $(libinit_ompi_OBJS)

clean:
	rm -rf $(libinit_ompi_OBJS) libinit_ompi.a 
