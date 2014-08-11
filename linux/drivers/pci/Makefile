#
# Makefile for the PCI bus specific drivers.
#

obj-y		+= access.o bus.o probe.o remove.o pci.o  \
		   pci-driver.o search.o pci-sysfs.o rom.o setup-res.o \
		   msi.o

# Build PCI Express stuff if needed
obj-$(CONFIG_PCIEPORTBUS) += pcie/

#
# Some architectures use the generic PCI setup functions
#
obj-$(CONFIG_X86) += setup-bus.o

ifeq ($(CONFIG_PCI_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
