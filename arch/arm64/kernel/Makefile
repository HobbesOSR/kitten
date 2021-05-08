extra-y		:= head.o head64.o bootstrap.o vmlwk.lds
EXTRA_FLAGS	:= -traditional -nostdlib -nodefaultlibs -ffreestanding
obj-y		:= setup.o memblock.o cpuinfo.o resource.o \
		   mpparse.o entry.o show.o syscall.o cpu.o \
		   trampoline.o interrupts.o mpboot.o \
		   time.o sys_arch_prctl.o xcall.o \
		   task.o sched.o maccess.o tlbflush.o signal.o acpi.o \
		   compat.o bitops.o traps.o ptrace.o \
		   entry-fpsimd.o 
		   
		   # i387.o lapic.o ioapic.o

obj-$(CONFIG_KGDB) += kgdb.o

#obj-$(CONFIG_CRAY_XT) += rca/

#obj-$(CONFIG_PALACIOS) += palacios/

#obj-$(CONFIG_ARM_MCE) += mcheck/

obj-y += pci/