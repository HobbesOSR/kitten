
CC=gcc
CPPFLAGS = -Wall
CPPFLAGS += -I../../../include
CPPFLAGS += -I../../liblwk/include

libinit_dapl_C_SOURCE = init_dapl.c 

libinit_dapl_OBJS=$(libinit_dapl_C_SOURCE:.c=.o)

libinit_dapl.a: $(libinit_dapl_OBJS) 
	ar rcs $@ $(libinit_dapl_OBJS)

clean:
	rm -rf $(libinit_dapl_OBJS) libinit_dapl.a 
