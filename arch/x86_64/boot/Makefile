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
FDARGS_GRUB = $(KERNEL_ARGS) init_argv=\"$(CONFIG_INIT_TASK_ARGS)\" init_envp=\"$(CONFIG_INIT_TASK_ENVS)\"
FDARGS_GRUB2 = $(KERNEL_ARGS) init_argv=$(CONFIG_INIT_TASK_ARGS) init_envp=$(CONFIG_INIT_TASK_ENVS)

# Initial grub2 modules to be loaded.
GRUB2_INIT_MODS = $(shell echo $(CONFIG_GRUB2_MODS))

image_cmdline = default lwk $(FDARGS) $(if $(FDINITRD),initrd=initrd.img,)
gimage_cmdline = default 0\ntimeout 5\n\ntitle Kitten\nroot (cd)\nkernel /boot/lwk ro $(FDARGS_GRUB)\n$(if $(FDINITRD),initrd /boot/initrd.img,)
gimage2_cmdline = set default=0\nset timeout=5\n\nmenuentry Kitten {\n\tlinux /boot/lwk ro $(FDARGS_GRUB2)\n $(if $(FDINITRD),\tinitrd /boot/initrd.img,)\n}

# This requires being root or having syslinux 2.02 or higher installed
isoimage: $(BOOTIMAGE)
	-rm -rf $(obj)/isoimage
	mkdir $(obj)/isoimage

ifeq ($(CONFIG_SYSLINUX_BOOTLOADER),y)
	for i in lib lib64 share end ; do \
		if [ -f /usr/$$i/syslinux/isolinux.bin ] ; then \
			cp /usr/$$i/syslinux/isolinux.bin $(obj)/isoimage ; \
			if [ -f /usr/$$i/syslinux/ldlinux.c32 ] ; then \
				cp /usr/$$i/syslinux/ldlinux.c32 $(obj)/isoimage ; \
			fi ; \
			break ; \
		elif [ -f /usr/$$i/syslinux/bios/isolinux.bin ] ; then \
			cp /usr/$$i/syslinux/bios/* $(obj)/isoimage ; \
			break ; \
		elif [ -f /usr/$$i/ISOLINUX/isolinux.bin ] ; then \
			cp /usr/$$i/ISOLINUX/* $(obj)/isoimage ; \
			break ; \
		fi ; \
		if [ $$i = end ] ; then exit 1 ; fi ; \
	done
	cp $(BOOTIMAGE) $(obj)/isoimage/lwk
	echo 'TIMEOUT 1' > $(obj)/isoimage/isolinux.cfg
	echo '$(image_cmdline)' >> $(obj)/isoimage/isolinux.cfg
	@if [ -f '$(FDINITRD)' ] ; then \
		echo 'initrd: $(FDINITRD)'; \
		cp $(FDINITRD) $(obj)/isoimage/initrd.img ; \
	elif [ -z '$(FDINITRD)' ]; then \
		echo "initrd: None"; \
	else \
		echo >&2 'initrd: $(FDINITRD) not found!'; \
		exit 1; \
	fi
	if which mkisofs 2>&1 1>/dev/null; then \
		mkisofs -J -r -o $(obj)/image.iso -b isolinux.bin -c boot.cat \
			-no-emul-boot -boot-load-size 4 -boot-info-table \
			$(obj)/isoimage; \
	elif which genisoimage 2>&1 1>/dev/null; then \
		genisoimage -J -r -o $(obj)/image.iso -b isolinux.bin -c boot.cat \
			-no-emul-boot -boot-load-size 4 -boot-info-table \
			$(obj)/isoimage; \
	else \
		echo >&2 'mkisofs or genisoimage not found!'; \
		exit 1; \
	fi
	rm -rf $(obj)/isoimage
else
	@ver=`grub-install --version | sed "s,.* \([0-9]\).[0-9].*,\1,"`; \
	platform=pc; \
	model=`uname -m`; \
	if [ "$$model" = "i486" -o "$$model" = "i586" -o "$$model" = "i686" -o $$ver -ne 0 ]; then \
		model="i386"; \
	fi; \
	mkdir -p $(obj)/isoimage/boot/grub; \
	cp $(BOOTIMAGE) $(obj)/isoimage/boot/lwk; \
	if [ -f '$(FDINITRD)' ] ; then \
		echo 'initrd: $(FDINITRD)'; \
		cp $(FDINITRD) $(obj)/isoimage/boot/initrd.img ; \
	elif [ -z '$(FDINITRD)' ]; then \
		echo "initrd: None"; \
	else \
		echo >&2 'initrd: $(FDINITRD) not found!'; \
		exit 1; \
	fi; \
	if [ -d /usr/lib/grub/$${model}-$$platform ] ; then \
		mkdir -p $(obj)/isoimage/etc; \
		if [ $$ver -eq 0 ]; then \
			echo '$(gimage_cmdline)'  > $(obj)/isoimage/boot/grub/menu.lst; \
			ln -sf /boot/grub/menu.lst $(obj)/isoimage/etc/grub.conf; \
			cp /usr/lib/grub/$${model}-$$platform/stage2_eltorito $(obj)/isoimage/boot/grub; \
			mkisofs -J -R -o $(obj)/image.iso -b boot/grub/stage2_eltorito \
				-c boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table $(obj)/isoimage; \
		elif [ $$ver -eq 2 ]; then \
			modules="$(cat $(bootprep)/partmap.lst) $(GRUB2_INIT_MODS)"; \
			(for i in $$modules; do \
				echo "insmod $$i";   \
			done; \
			echo '$(gimage2_cmdline)') > $(obj)/isoimage/boot/grub/grub.cfg; \
		  grub-mkrescue -o $(obj)/image.iso $(obj)/isoimage; \
		else \
			bootprep=$(obj)/bootprep/boot/grub/$$model-$$platform; \
			p_dir=/usr/lib/grub/$$model-$$platform; \
			mkdir -p $$bootprep; \
			cp $$p_dir/*.mod $$bootprep ; \
			cp $$p_dir/*.lst $$bootprep ; \
			cp $$p_dir/efiemu* $$bootprep ; \
			modules="$(cat $(bootprep)/partmap.lst) $(GRUB2_INIT_MODS)"; \
			(for i in $$modules; do \
				echo "insmod $$i";   \
			done; \
			echo "source /boot/grub/grub.cfg") > $$bootprep/grub.cfg; \
			echo  '$(gimage2_cmdline)' > $(obj)/isoimage/boot/grub/grub.cfg; \
			grub-mkimage -d $$p_dir -o $(obj)/core_img --prefix=/boot/grub/$$model-$$platform \
				iso9660 biosdisk; \
			cat $$p_dir/cdboot.img $(obj)/core_img > $$bootprep/stage2_eltorito; \
			grub-mkisofs -b boot/grub/$$model-$$platform/stage2_eltorito -boot-info-table \
				--protective-msdos-label -o $(obj)/image.iso -r $$obj/bootprep $(obj)/isoimage; \
			rm -rf $(obj)/core_img; \
		fi; \
	fi; \
	rm -rf $(obj)/isoimage
endif

