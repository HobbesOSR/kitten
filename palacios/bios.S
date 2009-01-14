/** \file
 *  ROM BIOS images for use by guest operating systems.
 *
 * These are only used if CONFIG_PALACIOS is defined and are used
 * to jump start the guest operating systems virtual machine.
 */

.data
.globl rombios_start
rombios_start:
.incbin ROMBIOS_PATH
.globl rombios_end
rombios_end:

.globl vgabios_start
vgabios_start:
.incbin VGABIOS_PATH
.globl vgabios_end
vgabios_end:


/** Hacks to get libxed to be quiet */
.globl fprintf
.globl stderr
.globl abort
	jmp panic
