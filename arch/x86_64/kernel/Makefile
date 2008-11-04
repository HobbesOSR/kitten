extra-y		:= head.o head64.o init_task.o vmlwk.lds
EXTRA_FLAGS	:= -traditional
obj-y		:= percpu.o setup.o e820.o cpuinfo.o resource.o \
		   mpparse.o entry.o show.o syscall.o i387.o cpu.o \
		   lapic.o ioapic.o trampoline.o interrupts.o mpboot.o \
		   time.o sys_arch_prctl.o vsyscall.o xcall.o \
		   task.o sched.o maccess.o
obj-$(CONFIG_KGDB) += kgdb.o

obj-$(CONFIG_CRAY_XT) += rca/

ifdef CONFIG_PALACIOS
# The palacios hypervisor requires a BIOS image in order to boot guest OSes
obj-$(CONFIG_PALACIOS) += bios.o
$(obj)/bios.o :: EXTRA_AFLAGS = \
	-DROMBIOS_PATH="\"$(CONFIG_PALACIOS_PATH)/palacios/build/rombios\"" \
	-DVGABIOS_PATH="\"$(CONFIG_PALACIOS_PATH)/palacios/build/vgabios\""

# Interface glue between the LWK and Palacios
obj-$(CONFIG_PALACIOS) += palacios.o
endif
