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

LDFLAGS		:= -m elf_x86_64
OBJCOPYFLAGS	:= -O binary -R .note -R .comment -S
LDFLAGS_vmlinux :=
CHECKFLAGS      += -D__x86_64__ -m64

cflags-y	:=
cflags-$(CONFIG_MK8) += $(call cc-option,-march=k8)
cflags-$(CONFIG_MPSC) += $(call cc-option,-march=nocona)
cflags-$(CONFIG_GENERIC_CPU) += $(call cc-option,-mtune=generic)

cflags-y += -m64
cflags-y += -mno-red-zone
cflags-y += -mcmodel=kernel
cflags-y += -pipe
cflags-y += -ffunction-sections
# this makes reading assembly source easier, but produces worse code
# actually it makes the kernel smaller too.
cflags-y += -fno-reorder-blocks
cflags-y += -Wno-sign-compare
# -funit-at-a-time shrinks the kernel .text considerably
# unfortunately it makes reading oopses harder.
cflags-y += $(call cc-option,-funit-at-a-time)
# prevent gcc from generating any FP code by mistake
cflags-y += $(call cc-option,-mno-sse -mno-mmx -mno-sse2 -mno-3dnow,)

CFLAGS += $(cflags-y)
AFLAGS += -m64

head-y := arch/x86_64/kernel/head.o arch/x86_64/kernel/head64.o arch/x86_64/kernel/init_task.o

libs-y += arch/x86_64/lib/
core-y += arch/x86_64/kernel/

boot := arch/x86_64/boot

PHONY += bzImage archmrproper isoimage archclean

#Default target when executing "make"
all: bzImage

BOOTIMAGE                     := arch/x86_64/boot/bzImage
KBUILD_IMAGE                  := $(BOOTIMAGE)

bzImage: vmlwk
	$(Q)$(MAKE) $(build)=$(boot) $(BOOTIMAGE)

isoimage: vmlwk
	$(Q)$(MAKE) $(build)=$(boot) BOOTIMAGE=$(BOOTIMAGE) $@

archclean:
	$(Q)$(MAKE) $(clean)=$(boot)

define archhelp
  echo  '* bzImage	- Compressed kernel image (arch/$(ARCH)/boot/bzImage)'
  echo  '  isoimage     - Create a boot CD-ROM image'
endef

CLEAN_FILES += arch/$(ARCH)/boot/image.iso


