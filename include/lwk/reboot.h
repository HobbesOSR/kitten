/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LWK_REBOOT_H
#define _LWK_REBOOT_H


#define SYS_DOWN	0x0001	/* Notify of system down */
#define SYS_RESTART	SYS_DOWN
#define SYS_HALT	0x0002	/* Notify of system halt */
#define SYS_POWER_OFF	0x0003	/* Notify of system power off */

enum reboot_mode {
	REBOOT_COLD = 0,
	REBOOT_WARM,
	REBOOT_HARD,
	REBOOT_SOFT,
	REBOOT_GPIO,
};
extern enum reboot_mode reboot_mode;

enum reboot_type {
	BOOT_TRIPLE	= 't',
	BOOT_KBD	= 'k',
	BOOT_BIOS	= 'b',
	BOOT_ACPI	= 'a',
	BOOT_EFI	= 'e',
	BOOT_CF9_FORCE	= 'p',
	BOOT_CF9_SAFE	= 'q',
};
extern enum reboot_type reboot_type;

extern int reboot_default;
extern int reboot_cpu;
extern int reboot_force;


#endif