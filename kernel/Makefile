obj-y := \
	main.o \
	version.o \
	console.o \
	printk.o \
	spinlock.o \
	params.o \
	driver.o \
	cpuinfo.o \
	panic.o \
	resource.o \
	kallsyms.o \
	extable.o \
	show.o \
	elf.o \
	time.o \
	xcall.o \
	elf_liblwk.o \
	sched.o \
	sched_rr.o \
	sched_control.o \
	completion.o \
	waitq.o \
	timer.o \
	init_task.o \
	kfs.o \
	interrupt.o \
	semaphore.o \
	random.o \
	in_mem_fs.o \
	fifo.o \
	task.o \
	kthread.o \
	signal.o \
	fdTable.o \
	workq.o \
	percpu.o \

obj-$(CONFIG_SCHED_EDF) += sched_edf.o
obj-$(CONFIG_KGDB) += kgdb.o
obj-$(CONFIG_KGDB_SERIAL_CONSOLE) += kgdboc.o
obj-$(CONFIG_DEBUG_HW_NOISE) += noise.o
obj-$(CONFIG_NETWORK) += netdev.o
obj-$(CONFIG_BLOCK_DEVICE) += blkdev.o
obj-$(CONFIG_PALACIOS_GDB) += \
	gdb_target_desc.o \
	gdb.o \
	gdbio.o \

obj-y += mm/
obj-y += lwk_syscalls/
obj-y += linux_syscalls/
obj-y += pci/

# additional files to be removed upon make clean
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
