extra-y		:= head.o head64.o bootstrap.o vmlwk.lds
EXTRA_FLAGS	:= -traditional -nostdlib -nodefaultlibs -ffreestanding

obj-y		:= setup.o memblock.o cpuinfo.o resource.o \
               entry.o show.o syscall.o cpu.o \
               interrupts.o mpboot.o \
               time.o sys_arch_prctl.o xcall.o \
               task.o sched.o maccess.o tlbflush.o signal.o acpi.o \
               bitops.o traps.o ptrace.o \
               entry-fpsimd.o \
               irqchip.o \
               smccc-call.o cpu_ops.o smp.o \
               smp_spin_table.o psci.o reboot.o \
               topology.o



obj-$(CONFIG_ARM64_ACPI_PARKING_PROTOCOL) += acpi_parking_protocol.o

obj-$(CONFIG_EFI)     += efi-header.o

obj-$(CONFIG_HAFNIUM) += hafnium/

obj-y                 += pci/
obj-y                 += irqchips/
obj-y                 += timers/