# Interface glue between the LWK and Palacios
obj-y += palacios.o

# The palacios hypervisor requires a BIOS image in order to boot guest OSes
obj-y += bios.o

# Must be relative to the module path $(src)!
PALACIOS_PATH:=$(src)/../../palacios

$(obj)/palacios.o :: EXTRA_CFLAGS = \
	-I$(PALACIOS_PATH)/palacios/include \

$(obj)/bios.o :: EXTRA_AFLAGS = \
	-DROMBIOS_PATH="\"$(PALACIOS_PATH)/palacios/build/rombios\"" \
	-DVGABIOS_PATH="\"$(PALACIOS_PATH)/palacios/build/vgabios\""

# Extra libraries that we will need to copy over
# The leading $(src) will need to be stripped off the Palacios path
LIBS := \
	$(PALACIOS_PATH)/palacios/build/libv3vee.a \
	$(PALACIOS_PATH)/palacios/lib/xed/libxed32e.a \

$(LIBS):
obj-y += $(subst $(src)/,,$(LIBS))




install:
	@if [ -z "$O" ]; then echo >&2 "O must be defined"; exit 1; fi
	$(MAKE) -C $O M=`pwd`
	cp built-in.o $O/modules/palacios-mod.o
