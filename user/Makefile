CFLAGS=-O2 -Wall

# Default to building in the current directory, unless
# overridden by the calling Makefile or on the command line.
O=$(shell pwd)

all: liblwk hello_world

liblwk hello_world: FORCE
	@if [ ! -d $O/$@ ]; then mkdir $O/$@; fi
	make O=$O/$@ -C $@

clean:
	make O=$O/liblwk -C liblwk clean
	make O=$O/hello_world -C hello_world clean

FORCE:
