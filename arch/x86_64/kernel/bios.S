/*
 *  ROM Bios images for V3Vee emulator
 */

.data
.globl rombios_start
rombios_start:
.incbin "../palacios/build/rombios"
.globl rombios_end
rombios_end:

.globl vgabios_start
vgabios_start:
.incbin "../palacios/build/vgabios"
.globl vgabios_end
vgabios_end:
