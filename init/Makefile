#
# Makefile for the LWK.
#

obj-y				:= main.o version.o

# files to be removed upon make clean
clean-files := ../include/lwk/compile.h

# dependencies on generated files need to be listed explicitly

$(obj)/version.o: include/lwk/compile.h


# compile.h changes depending on hostname, generation number, etc,
# so we regenerate it always.
# mkcompile_h will make sure to only update the
# actual file if its content has changed.

include/lwk/compile.h: FORCE
	@echo '  CHK     $@'
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkcompile_h $@ \
	"$(UTS_MACHINE)" "$(CC) $(CFLAGS)"
	@echo -n "Build time: "; grep  UTS_VERSION $@ | cut -d' ' -f3-
