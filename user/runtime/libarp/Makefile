
CC=gcc
CPPFLAGS = -Wall
CPPFLAGS += -I../../../include
CPPFLAGS += -I../../liblwk/include

libinitArp_C_SOURCE = initArp.c 

libinitArp_OBJS=$(libinitArp_C_SOURCE:.c=.o)

libinitArp.a: $(libinitArp_OBJS) 
	ar rcs $@ $(libinitArp_OBJS)

clean:
	rm -rf $(libinitArp_OBJS) libinitArp.a 
