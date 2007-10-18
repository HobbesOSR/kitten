extra-y		:= head.o head64.o init_task.o vmlwk.lds
EXTRA_FLAGS	:= -traditional
obj-y		:= pda.o percpu.o setup.o e820.o cpuinfo.o resource.o \
		   mpparse.o entry.o show.o syscall.o i387.o kevin.o \
		   local_apic.o trampoline.o interrupts.o mpboot.o

obj-$(CONFIG_CRAY_XT) += rca/
