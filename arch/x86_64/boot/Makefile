#
# arch/x86_64/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
#
# 2007 by Linus Torvalds
#
# 20070816  Kevin Pedretti <ktpedre@sandia.gov>
#           Modifications for Kitten. Remove unneeded stuff.
#

# ROOT_DEV specifies the default root-device when making the image.
# This can be either FLOPPY, CURRENT, /dev/xxxx or empty, in which case
# the default of FLOPPY is used by 'build'.

ROOT_DEV := CURRENT

targets		:= vmlwk.bin bootsect bootsect.o \
		   setup setup.o bzImage

EXTRA_CFLAGS := -m32

hostprogs-y	:= tools/build
HOST_EXTRACFLAGS += $(LWKINCLUDE)
subdir-		:= compressed/	#Let make clean descend in compressed/
# ---------------------------------------------------------------------------

$(obj)/bzImage: IMAGE_OFFSET := 0x100000
$(obj)/bzImage: EXTRA_AFLAGS := -traditional -D__BIG_KERNEL__
$(obj)/bzImage: BUILDFLAGS   := -b

quiet_cmd_image = BUILD   $@
cmd_image = $(obj)/tools/build $(BUILDFLAGS) $(obj)/bootsect $(obj)/setup \
	    $(obj)/vmlwk.bin $(ROOT_DEV) > $@

$(obj)/bzImage: $(obj)/bootsect $(obj)/setup \
			      $(obj)/vmlwk.bin $(obj)/tools/build FORCE
	$(call if_changed,image)
	@echo 'Kernel: $@ is ready' ' (#'`cat .version`')'

$(obj)/vmlwk.bin: $(obj)/compressed/vmlwk FORCE
	$(call if_changed,objcopy)

LDFLAGS_bootsect := -Ttext 0x0 -s --oformat binary
LDFLAGS_setup	 := -Ttext 0x0 -s --oformat binary -e begtext

$(obj)/setup $(obj)/bootsect: %: %.o FORCE
	$(call if_changed,ld)

$(obj)/compressed/vmlwk: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/compressed IMAGE_OFFSET=$(IMAGE_OFFSET) $@

# Arguments to pass to the kernel
# The '$(shell echo ...)' is needed to get rid of the surrounding quotes
# that Kconfig insists on adding to string types. Same trick used below.
KERNEL_ARGS = $(shell echo $(CONFIG_KERNEL_ARGS))

# The initrd is the ELF image to use for the initial user-level task
# or an ISO image to use for the guest OS booted by Palacios
FDINITRD = $(shell echo $(CONFIG_INIT_TASK_IMAGE))

# Set this if you want to pass append arguments to the isoimage kernel
FDARGS = $(KERNEL_ARGS) init_argv=$(CONFIG_INIT_TASK_ARGS) init_envp=$(CONFIG_INIT_TASK_ENVS)

image_cmdline = default lwk $(FDARGS) $(if $(FDINITRD),initrd=initrd.img,)
gimage_cmdline = default 0\ntimeout 5\n\ntitle Kitten\nroot (cd)\nkernel /boot/lwk ro $(FDARGS)\n$(if $(FDINITRD),initrd /boot/initrd.img,)

# This requires being root or having syslinux 2.02 or higher installed
isoimage: $(BOOTIMAGE) $(FDINITRD)
	-rm -rf $(obj)/isoimage
	mkdir $(obj)/isoimage

ifeq ($(CONFIG_BOOT_SYSLINUX),y)
	for i in lib lib64 share end ; do \
		if [ -f /usr/$$i/syslinux/isolinux.bin ] ; then \
			cp /usr/$$i/syslinux/isolinux.bin $(obj)/isoimage ; \
			break ; \
		fi ; \
		if [ $$i = end ] ; then exit 1 ; fi ; \
	done
	cp $(BOOTIMAGE) $(obj)/isoimage/lwk
	echo '$(image_cmdline)' > $(obj)/isoimage/isolinux.cfg
	@if [ -f '$(FDINITRD)' ] ; then \
		echo 'initrd: $(FDINITRD)'; \
		cp $(FDINITRD) $(obj)/isoimage/initrd.img ; \
	elif [ -z '$(FDINITRD)' ]; then \
		echo "initrd: None"; \
	else \
		echo >&2 'initrd: $(FDINITRD) not found!'; \
		exit 1; \
	fi
	mkisofs -J -r -o $(obj)/image.iso -b isolinux.bin -c boot.cat \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		$(obj)/isoimage
	rm -rf $(obj)/isoimage
else
	model=`uname -m`; \
	if [ "$$model" = "i486" -o "$$model" = "i586" -o "$$model" = "i686" ]; then \
		model="i386"; \
	fi; \
	if [ -d /usr/lib/grub/$${model}-pc ] ; then \
		mkdir -p $(obj)/isoimage/boot/grub; \
		mkdir -p $(obj)/isoimage/etc; \
		echo '$(gimage_cmdline)'  > $(obj)/isoimage/boot/grub/menu.lst; \
		ln -sf /boot/grub/menu.lst $(obj)/isoimage/etc/grub.conf; \
		cp /usr/lib/grub/$${model}-pc/stage2_eltorito $(obj)/isoimage/boot/grub ; \
	fi ; \
	cp $(BOOTIMAGE) $(obj)/isoimage/boot/lwk
	@if [ -f '$(FDINITRD)' ] ; then \
		echo 'initrd: $(FDINITRD)'; \
		cp $(FDINITRD) $(obj)/isoimage/boot/initrd.img ; \
	elif [ -z '$(FDINITRD)' ]; then \
		echo "initrd: None"; \
	else \
		echo >&2 'initrd: $(FDINITRD) not found!'; \
		exit 1; \
	fi
	mkisofs -J -R -o $(obj)/image.iso -b boot/grub/stage2_eltorito -c boot.cat \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		$(obj)/isoimage
	rm -rf $(obj)/isoimage
endif

