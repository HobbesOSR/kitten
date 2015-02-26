CFLAGS=-O2 -Wall

# Default to building in the current directory, unless
# overridden by the calling Makefile or on the command line.
O=$(shell pwd)

all: liblwk libxpmem hello_world powerinsight smartmap test_app pisces edf_sched multi_loader coop_sched

liblwk libxpmem hello_world powerinsight smartmap test_app pisces edf_sched multi_loader coop_sched: FORCE
	@if [ ! -d $O/$@ ]; then mkdir $O/$@; fi
	make O=$O/$@ -C $@
	make O=$O/$@ -C $@ install

clean:
	make O=$O/liblwk -C liblwk clean
	make O=$O/libxpmem -C libxpmem clean
	make O=$O/hello_world -C hello_world clean
	make O=$O/powerinsight -C powerinsight clean
	make O=$O/test_app -C test_app clean
	make O=$O/smartmap -C smartmap clean
	make O=$O/pisces -C pisces clean
	make O=$O/edf_sched -C edf_sched clean
	make O=$O/coop_sched -C coop_sched clean
	make O=$O/multi_loader -C multi_loader clean
	rm -rf $O/install

FORCE:
