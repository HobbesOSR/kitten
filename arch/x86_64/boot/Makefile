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

# Set this if you want to pass append arguments to the isoimage kernel
FDARGS = console=vga,serial debug_mptable=1
# Set this if you want an initrd included with the isoimage kernel
FDINITRD = app.bin

image_cmdline = default lwk $(FDARGS) $(if $(FDINITRD),initrd=initrd.img,)

# This require being root or having syslinux 2.02 or higher installed
isoimage: $(BOOTIMAGE)
	-rm -rf $(obj)/isoimage
	mkdir $(obj)/isoimage
	cp `echo /usr/lib*/syslinux/isolinux.bin | awk '{ print $1; }'` \
		$(obj)/isoimage
	cp $(BOOTIMAGE) $(obj)/isoimage/lwk
	echo '$(image_cmdline)' > $(obj)/isoimage/isolinux.cfg
	if [ -f '$(FDINITRD)' ] ; then \
		cp '$(FDINITRD)' $(obj)/isoimage/initrd.img ; \
	fi
	mkisofs -J -r -o $(obj)/image.iso -b isolinux.bin -c boot.cat \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		$(obj)/isoimage
	rm -rf $(obj)/isoimage

