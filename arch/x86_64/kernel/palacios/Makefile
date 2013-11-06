# Interface glue between the LWK and Palacios
obj-$(CONFIG_PALACIOS) += palacios.o
obj-$(CONFIG_PALACIOS_SOCKET) += palacios_socket.o
obj-$(CONFIG_PALACIOS_GDB) += palacios_gdb.o

$(obj)/palacios.o :: EXTRA_CFLAGS = \
	-I$(CONFIG_PALACIOS_PATH)/palacios/include \

$(obj)/palacios_socket.o :: EXTRA_CFLAGS = \
	-I$(CONFIG_PALACIOS_PATH)/palacios/include \

$(obj)/palacios_gdb.o :: EXTRA_CFLAGS = \
    -I$(CONFIG_PALACIOS_PATH)/palacios/include \
