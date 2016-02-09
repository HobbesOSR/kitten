#
# lwk/arch/x86_64/boot/compressed/Makefile
#
# create a compressed vmlwk image from the original vmlwk
#
# Note all the files here are compiled/linked as 32bit executables.
#

targets		:= vmlwk vmlwk.bin vmlwk.bin.gz head.o misc.o piggy.o
EXTRA_AFLAGS	:= -traditional

# cannot use EXTRA_CFLAGS because base CFLAGS contains -mkernel which conflicts with
# -m32
CFLAGS := -m64 -D__KERNEL__ -Iinclude -O2  -fno-strict-aliasing -fPIC -mcmodel=small -fno-builtin -fno-stack-protector
LDFLAGS := -m elf_$(UTS_MACHINE)

LDFLAGS_vmlwk := -T
$(obj)/vmlwk: $(src)/vmlwk.lds $(obj)/head.o $(obj)/misc.o $(obj)/piggy.o FORCE
	$(call if_changed,ld)
	@:

$(obj)/vmlwk.bin: vmlwk FORCE
	$(call if_changed,objcopy)

$(obj)/vmlwk.bin.gz: $(obj)/vmlwk.bin FORCE
	$(call if_changed,gzip)

LDFLAGS_piggy.o := -r --format binary --oformat $(CONFIG_OUTPUT_FORMAT) -T

$(obj)/piggy.o: $(obj)/vmlwk.scr $(obj)/vmlwk.bin.gz FORCE
	$(call if_changed,ld)
