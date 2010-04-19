
CC=gcc
CPPFLAGS = -Wall
CPPFLAGS += -I../../../include
CPPFLAGS += -I../../user/liblwk/include

libwait4net_C_SOURCE = wait4net.c getaddrinfo.c

libwait4net_OBJS=$(libwait4net_C_SOURCE:.c=.o)

libwait4net.a: $(libwait4net_OBJS) 
	ar rcs $@ $(libwait4net_OBJS)

clean:
	rm -rf $(libwait4net_OBJS) libwait4net.a 
