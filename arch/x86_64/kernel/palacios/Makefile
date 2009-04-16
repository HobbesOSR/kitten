# Interface glue between the LWK and Palacios
obj-$(CONFIG_PALACIOS) += palacios.o
obj-$(CONFIG_PALACIOS_SOCKET) += palacios_socket.o

# Palacios requires a BIOS image in order to boot guest OSes
obj-$(CONFIG_PALACIOS) += bios.o

$(obj)/palacios.o :: EXTRA_CFLAGS = \
	-I$(CONFIG_PALACIOS_PATH)/palacios/include \

$(obj)/palacios_socket.o :: EXTRA_CFLAGS = \
	-I$(CONFIG_PALACIOS_PATH)/palacios/include \

$(obj)/bios.o :: EXTRA_AFLAGS = \
	-DROMBIOS_PATH="\"$(CONFIG_PALACIOS_PATH)/palacios/build/rombios\"" \
	-DVGABIOS_PATH="\"$(CONFIG_PALACIOS_PATH)/palacios/build/vgabios\""
