#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/string.h>
#include <lwk/screen_info.h>
#include <lwk/params.h>
#include <lwk/smp.h>
#include <lwk/cpuinfo.h>
#include <arch/bootsetup.h>
#include <arch/sections.h>
#include <arch/pda.h>
#include <arch/processor.h>
#include <arch/desc.h>
#include <arch/proto.h>


/**
 * Data passed to the kernel by the bootloader.
 *
 * NOTE: This is marked as __initdata so it goes away after the
 *       kernel bootstrap process is complete.
 */
char x86_boot_params[BOOT_PARAM_SIZE] __initdata = {0,};


/**
 * Interrupt Descriptor Table (IDT) descriptor.
 *
 * This descriptor contains the length of the IDT table and a
 * pointer to the table.  The lidt instruction (load IDT) requires
 * this format.
 */
struct desc_ptr idt_descr = { 256 * 16 - 1, (unsigned long) idt_table };


/**
 * Array of exception stacks.
 */
char boot_exception_stacks[(N_EXCEPTION_STACKS - 1) * EXCEPTION_STKSZ + DEBUG_STKSZ]
__attribute__((section(".bss.page_aligned")));


/**
 * Package ID of each logical CPU.
 */
uint16_t phys_proc_id[NR_CPUS] __read_mostly = { [0 ... NR_CPUS-1] = BAD_APICID };


/**
 * Core ID of each logical CPU.
 */
uint16_t cpu_core_id[NR_CPUS] __read_mostly = { [0 ... NR_CPUS-1] = BAD_APICID };


/**
 * Determines the address of the kernel boot command line.
 */
static char * __init
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


/**
 * This is the initial C entry point to the kernel.
 * NOTE: The order of operations is usually important.  Be careful.
 */
void __init
x86_64_start_kernel(char * real_mode_data)
{
	int i;

	/* Sanity check. */
	if (__pa_symbol(&_end) >= KERNEL_TEXT_SIZE)
		while (1) {}

	/*
	 * Setup the initial interrupt descriptor table (IDT).
	 * This will be eventually be populated with the real handlers.
	 */
	for (i = 0; i < 256; i++)
		set_intr_gate(i, early_idt_handler);
	asm volatile("lidt %0" :: "m" (idt_descr));

	/*
 	 * Zero the "Block Started by Symbol" section...
	 * you know, the one that holds uninitialized data.
	 */
	memset(__bss_start, 0,
	       (unsigned long) __bss_stop - (unsigned long) __bss_start);

	/*
 	 * Switch to a new copy of the boot page tables.  Not exactly sure
	 * why this is done but it is probably because boot_level4_pgt is
	 * marked as __initdata, meaning it gets discarded after bootstrap.
	 */
	memcpy(init_level4_pgt, boot_level4_pgt, PTRS_PER_PGD*sizeof(pgd_t));
	asm volatile("movq %0,%%cr3" :: "r" (__pa_symbol(&init_level4_pgt)));

	/*
 	 * Early per-processor data area (PDA) initialization.
 	 */
	for (i = 0; i < NR_CPUS; i++)
		cpu_pda(i) = &boot_cpu_pda[i];
	pda_init(0);

	/*
 	 * Store a copy of the data passed in by the bootloader.
	 * real_mode_data will get clobbered eventually when the
	 * memory subsystem is initialized.
	 */
	memcpy(x86_boot_params, real_mode_data, sizeof(x86_boot_params));

	/*
 	 * Tell the VGA driver the starting line number... this avoids
	 * overwriting BIOS and bootloader messages.  Use of SCREEN_INFO
	 * requires that x86_boot_params has been initialized.
	 */
	param_set_by_name_int("vga.row", SCREEN_INFO.orig_y);

	/*
	 * Mark the boot CPU up... kinda.
	 */
	cpu_set(0, cpu_online_map);

	/* Stash away the kernel boot command line. */
	memcpy(lwk_command_line, find_command_line(), sizeof(lwk_command_line));

	/* 
 	 * Okay... we've done the bare essentials.  Call into the 
 	 * platform-independent bootstrap function.  This will in turn
 	 * call back into architecture dependent code to do things like
 	 * initialize interrupts and boot CPUs. 
 	 */
	start_kernel();
}

