
CC=gcc
CPPFLAGS = -Wall
CPPFLAGS += -DUSE_LOCKS=1 -DHAVE_MMAP=0

libmalloc_C_SOURCE = malloc.c 

libmalloc_OBJS=$(libmalloc_C_SOURCE:.c=.o)

libmalloc.a: $(libmalloc_OBJS) 
	ar rcs $@ $(libmalloc_OBJS)

clean:
	rm -rf $(libmalloc_OBJS) libmalloc.a 
