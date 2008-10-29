/*
 *  ROM Bios images for V3Vee emulator
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
