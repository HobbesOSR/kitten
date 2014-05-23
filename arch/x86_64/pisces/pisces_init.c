#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/string.h>
#include <lwk/bootmem.h>
#include <lwk/cpuinfo.h>
#include <arch/page.h>
#include <arch/bootsetup.h>
#include <arch/e820.h>
#include <arch/mpspec.h>
#include <arch/proto.h>
#include <arch/sections.h>
#include <arch/acpi.h>
#include <arch/cpuinfo.h>

#include <arch/pisces/pisces.h>
#include <arch/pisces/pisces_boot_params.h>


struct pisces_boot_params * pisces_boot_params = NULL;


void __init
pisces_init(char *real_mode_data) 
{
      
	pisces_boot_params = (struct pisces_boot_params *)__va((u64)real_mode_data << PAGE_SHIFT);


	if (pisces_boot_params->magic != PISCES_MAGIC) {
		return;
	}


	// Emulate the necessary x86_boot_params provided by bootloader
	// See: include/arch-x86_64/bootsetup.h
	// This is ugly....
	INITRD_START = pisces_boot_params->initrd_addr;
	INITRD_SIZE  = pisces_boot_params->initrd_size;
	KERNEL_START = pisces_boot_params->kernel_addr;
	LOADER_TYPE  = 'P'; // For now it just needs to be set to something

	memcpy(lwk_command_line, pisces_boot_params->cmd_line, sizeof(lwk_command_line));


	start_kernel();
}

