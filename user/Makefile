CFLAGS=-O2 -Wall

all: liblwk hello_world

liblwk hello_world: FORCE
	@if [ ! -d $O/$@ ]; then mkdir $O/$@; fi
	make O=$O/$@ -C $@

clean:
	make O=$O/liblwk -C liblwk clean
	make O=$O/hello_world -C hello_world clean

FORCE:
