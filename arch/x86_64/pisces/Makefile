obj-$(CONFIG_PISCES) += pisces_init.o \
			pisces_lock.o \
			pisces_ctrl.o \
			pisces_xbuf.o \
			pisces_lcall.o \
			pisces_file.o \
			pisces_irq_proxy.o

obj-$(CONFIG_XPMEM)  += pisces_xpmem.o

$(obj)/pisces_guest.o :: EXTRA_CFLAGS = \
	-I$(PWD)/drivers/xpmem/include

