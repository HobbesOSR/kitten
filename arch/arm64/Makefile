# x86_64/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
#
# 19990713  Artur Skawina <skawina@geocities.com>
#           Added '-march' and '-mpreferred-stack-boundary' support
# 20000913  Pavel Machek <pavel@suse.cz>
#	    Converted for x86_64 architecture
# 20010105  Andi Kleen, add IA32 compiler.
#           ....and later removed it again....
# 20070816  Kevin Pedretti <ktpedre@sandia.gov>
#           Modifications for Kitten. Remove unneeded stuff.
#

TEXT_OFFSET := 0x00080000

export BINARY_ARCH
export ELF_FORMAT

LDFLAGS		:= -m aarch64elf --whole-archive
OBJCOPYFLAGS	:= -O binary -R .note -R .comment -S
LDFLAGS_vmlinux :=
CHECKFLAGS      += -D__ARM64__ -m64

# Added to export macro during build of vmlwk.lds
CPPFLAGS_vmlwk.lds += -DTEXT_OFFSET=$(TEXT_OFFSET)

cflags-y	:= -D__KERNEL__




cflags-$(CONFIG_GENERIC_CPU) += $(call cc-option,-mtune=generic)
cflags-$(CONFIG_ARM_V8_3_A)  += $(call cc-option,-march=armv8.3-a)

# TODO Ignore warnings, remove this later
cflags-y += -w


cflags-y += -fno-builtin
cflags-y += -pipe
cflags-y += -ffunction-sections
# this makes reading assembly source easier, but produces worse code
# actually it makes the kernel smaller too.
cflags-y += -fno-reorder-blocks
cflags-y += -Wno-sign-compare
cflags-y += -Wno-unused-parameter
# -funit-at-a-time shrinks the kernel .text considerably
# unfortunately it makes reading oopses harder.
cflags-y += $(call cc-option,-funit-at-a-time)
# prevent gcc from generating any FP code by mistake
cflags-y += $(call cc-option,-mno-sse -mno-mmx -mno-sse2 -mno-3dnow,)


aflags-$(CONFIG_ARM_V8_3_A) += $(call cc-option,-march=armv8.3-a)


CFLAGS += $(cflags-y) -DTEXT_OFFSET=$(TEXT_OFFSET) -g

AFLAGS += $(call as-option,-Wa$(comma)--divide)

AFLAGS += $(aflags-y) -DTEXT_OFFSET=$(TEXT_OFFSET) 

#-m64 $(call as-option,-Wa$(comma)--divide)

head-y := arch/arm64/kernel/head.o arch/arm64/kernel/head64.o arch/arm64/kernel/bootstrap.o

libs-y += arch/arm64/lib/
core-y += arch/arm64/kernel/	\
          arch/arm64/mm/

boot := arch/arm64/boot

PHONY += uImage bzImage archmrproper isoimage archclean

#Default target when executing "make"
all: uImage

BOOTIMAGE                     := arch/arm64/boot/bzImage
KBUILD_IMAGE                  := $(BOOTIMAGE)

uImage: vmlwk.bin $(DEFAULT_EXTRA_TARGETS)
	mkimage -A arm -O linux -T kernel -C none -a 0x80000 -e 0x80000 -n LWK -d vmlwk.bin uImage
	

bzImage: vmlwk $(DEFAULT_EXTRA_TARGETS)
	$(Q)$(MAKE) $(build)=$(boot) $(BOOTIMAGE)

isoimage: vmlwk $(DEFAULT_EXTRA_TARGETS)
	$(Q)$(MAKE) $(build)=$(boot) BOOTIMAGE=$(BOOTIMAGE) $@

archclean:
	$(Q)$(MAKE) $(clean)=$(boot)

define archhelp
  echo  '* bzImage	- Compressed kernel image (arch/$(ARCH)/boot/bzImage)'
  echo  '  isoimage     - Create a boot CD-ROM image'
endef

CLEAN_FILES += arch/$(ARCH)/boot/image.iso


