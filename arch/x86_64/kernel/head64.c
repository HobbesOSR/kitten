#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/string.h>
#include <lwk/screen_info.h>
#include <arch/bootsetup.h>
#include <arch/sections.h>

// Needed for set_intr_gate()
#include <arch/processor.h>
#include <arch/desc.h>

// Needed for early_idt_handler
#include <arch/proto.h>


/** Data passed to the kernel by the bootloader.
 *
 * NOTE: This is marked as __initdata so it goes away after the
 *       kernel bootstrap process is complete.
 */
char x86_boot_params[BOOT_PARAM_SIZE] __initdata = {0,};


/** Interrupt Descriptor Table (IDT) descriptor.
 *
 * This descriptor contains the length of the IDT table and a
 * pointer to the table.  The lidt instruction (load IDT) requires
 * this format.
 */
struct desc_ptr idt_descr = { 256 * 16 - 1, (unsigned long) idt_table };


/** Determines the address of the kernel boot command line. */
static char *
__init
find_command_line(void)
{
	int new_data;
	char *command_line;

	new_data = *(int *) (x86_boot_params + NEW_CL_POINTER);
	if (!new_data) {
		if (OLD_CL_MAGIC != * (u16 *) OLD_CL_MAGIC_ADDR) {
			return NULL;
		}
		new_data = OLD_CL_BASE_ADDR + * (u16 *) OLD_CL_OFFSET;
	}
	command_line = (char *) ((u64)(new_data));
	return command_line;
}


static void
__init
setup_console(char *cfg)
{
	char buf[256];
	char *space;

	// Create a copy of config string and NULL
	// terminate it at the first space.
	strlcpy(buf, cfg, sizeof(buf));
	space = strchr(buf, ' ');
	if (space)
		*space = 0;

	// VGA console
	if (strstr(buf, "vga"))
		vga_init(SCREEN_INFO.orig_y);

	// Serial console
	if (strstr(buf, "serial"))
		serial_init();
}


/** This is the initial C entry point to the kernel.
 *
 * NOTE: The order of operations is usually important.  Be careful.
 */
void
__init
x86_64_start_kernel(char * real_mode_data)
{
	char *s;
	int i;

	// Setup the initial interrupt descriptor table (IDT).
	// This will be eventually be populated with the real handlers.
	for (i = 0; i < 256; i++)
		set_intr_gate(i, early_idt_handler);
	asm volatile("lidt %0" :: "m" (idt_descr));

	// Zero the "Block Started by Symbol" section...
	// you know, the one that holds uninitialized data.
	memset(__bss_start, 0,
	       (unsigned long) __bss_stop - (unsigned long) __bss_start);

	// Store a copy of the data passed in by the bootloader.
	// real_mode_data will get clobbered eventually when the
	// memory subsystem is initialized.
	memcpy(x86_boot_params, real_mode_data, sizeof(x86_boot_params));

	// Stash away the kernel boot command line.
	memcpy(lwk_command_line, find_command_line(), sizeof(lwk_command_line));

	// Switch to a new copy of the boot page tables.  Not exactly sure
	// why this is done but it is probably because boot_level4_pgt is
	// marked as __initdata, meaning it gets discarded after bootstrap.
	memcpy(init_level4_pgt, boot_level4_pgt, PTRS_PER_PGD*sizeof(pgd_t));
	asm volatile("movq %0,%%cr3" :: "r" (__pa_symbol(&init_level4_pgt)));

	// Initialize the console(s)... printk() will/should work after this.
	if ((s = strstr(lwk_command_line, "console=")) != NULL)
		setup_console(strchr(s, '=') + 1);

	// Call platform-independent init() function in init/main.c
	init();
}

