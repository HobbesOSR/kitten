#include <arch/pisces/pisces_boot_params.h>


/* Externed globals from setup.c 
 * We can remove these when/if we move this code to setup.c proper
 */
extern struct pisces_boot_params * pisces_boot_params;

// Global pointer to the MPTable
// We override the scan and set it according to boot paramters
extern struct intel_mp_floating  * mpf_found;

/* Temporary access, until mp table parsing is done */
extern unsigned int ioapic_num;

/** End externs */


static void __init
pisces_setup_memory_region(void)
{

	e820.nr_map      = 1;
	e820.map[0].addr = pisces_boot_params->base_mem_paddr;
	e820.map[0].size = pisces_boot_params->base_mem_size;
	e820.map[0].type = E820_RAM;

	//printk(KERN_INFO "e820[0] base 0x%llx size 0x%llx\n", e820.map[0].addr, e820.map[0].size);
	printk(KERN_DEBUG "Pisces provided physical RAM map:\n");

	e820_print_map("Pisces-map");

	/* This also sets end_pfn_map */
	end_pfn = e820_end_of_ram();
}



/**
 * Mark in-use memory regions as reserved.
 * This prevents the bootmem allocator from allocating them.
 */
static void __init
pisces_reserve_memory(void)
{
    
	uintptr_t end_of_loader_mem = pisces_boot_params->initrd_addr +	\
		pisces_boot_params->initrd_size;

        /* Reserve the kernel page table memory */
        reserve_bootmem(table_start << PAGE_SHIFT,
                        (table_end - table_start) << PAGE_SHIFT);


        reserve_bootmem(__pa(pisces_boot_params),
                        end_of_loader_mem - __pa(pisces_boot_params));




	// TODO: Only mark the used memory as reserved, there is a fair amount of padding included in the boot params

	initrd_start = pisces_boot_params->initrd_addr;
	initrd_end   = initrd_start + pisces_boot_params->initrd_size;

	printk("Initrd_start=%p, initrd_end=%p\n", 
	       (void *)initrd_start, 
	       (void *)initrd_end);

	printk("table_start=%p, table_end=%p\n", 
	       (void *)(table_start << PAGE_SHIFT), 
	       (void *)(table_end   << PAGE_SHIFT));


        return;
}


static void __init
pisces_acpi_init(void)
{
	printk(KERN_WARNING "Disabling ACPI on Pisces.\n");
        disable_acpi();
        return;
}

static void __init
pisces_init_resources(void) 
{
	e820_reserve_resources();
	return;
}


/*
 * Setup pisces enclave trampoline code
 * a reset cpu => Linux trampoline (trampoline_code_pa)
 * => launch code head
 * => secondary_startup_64
 * => initial_code (start_secondary)
 */
static void
pisces_reset_trampoline( void )
{

	/* Report Kitten's secondary startup target to the Linux CPU driver */
	{
		extern void (*secondary_startup_64)(void);
		pisces_boot_params->launch_code_target_addr = (u64) &secondary_startup_64;
	}

	/* Replase Kitten's trampoline with the one from Linux */
	{
		extern uintptr_t trampoline_base_paddr;
		trampoline_base_paddr = pisces_boot_params->trampoline_code_pa;
	}
}


void __init
setup_pisces_arch(void)
{

	/* First thing lets check if the initrd got clobbered by the bss */
	if ((unsigned long)__pa(__bss_stop) > pisces_boot_params->initrd_addr) {
		panic("Oh Shit. The BSS just nuked the init_task. (bss_stop = %p, initrd=%p)\n",
		      __pa(__bss_stop), (void *)pisces_boot_params->initrd_addr);
	}


	/*
	 * Figure out which memory regions are usable and which are reserved.
	 * This builds the "e820" map of memory from info provided by the
	 * memory map.
	 */
	pisces_setup_memory_region();



	/*
	 * Get the bare minimum info about the bootstrap CPU... the
	 * one we're executing on right now.  Latter on, the full
	 * boot_cpu_data and cpu_info[boot_cpu_id] structures will be
	 * filled in completely.
	 */
	boot_cpu_data.logical_id = 0;
	early_identify_cpu(&boot_cpu_data);

	/*
	 * Initialize the kernel page tables.
	 * The kernel page tables map an "identity" map of all physical memory
	 * starting at virtual address PAGE_OFFSET.  When the kernel executes,
	 * it runs inside of the identity map... memory below PAGE_OFFSET is
	 * from whatever task was running when the kernel got invoked.
	 */
	check_for_nx_bit();
	init_kernel_pgtables(0, (end_pfn_map << PAGE_SHIFT));


	/*
	 * Initialize the bootstrap dynamic memory allocator.
	 * alloc_bootmem() will work after this.
	 */
    
	/* For now we'll just use the first block... 
	 * This might have to change if we start using up space with the init_task... 
	 */
	{
		uintptr_t block_start_pfn = pisces_boot_params->base_mem_paddr >> PAGE_SHIFT;
		uintptr_t block_end_pfn   = block_start_pfn + (pisces_boot_params->base_mem_size >> PAGE_SHIFT);

		printk("setting up pisces bootmem: (%p)-(%p)\n", 
		       (void *)(block_start_pfn << PAGE_SHIFT), 
		       (void *)(block_end_pfn   << PAGE_SHIFT));

		setup_bootmem_allocator(block_start_pfn, block_end_pfn);
	}


	/*
	 * reserve memory used in bootstrap process
	 * find and reserve MP config
	 */
	pisces_reserve_memory();

	/*
	 * Setup the CPU mapping
	 */
	{
		u64 cpu_id = pisces_boot_params->cpu_id;

		printk("Setting up pisces instance on CPU %llu (APIC ID %llu)\n", 
		       cpu_id, pisces_boot_params->apic_id);

		cpu_set(0, cpu_present_map);
		physid_set(0, phys_cpu_present_map);

		cpu_info[0].logical_id   = 0;
		cpu_info[0].physical_id  = cpu_id;
		cpu_info[0].arch.apic_id = pisces_boot_params->apic_id;
	}

	/* 
	 * Reset the trampoline to allow bringing up secondary cores
	 */
	pisces_reset_trampoline();


	/*
	 * Initialize the ACPI subsystem.
	 */
	pisces_acpi_init();

	/*
	 * Initialize resources.  Resources reserve sections of normal memory
	 * (iomem) and I/O ports (ioport) for devices and other system
	 * resources.  For each resource type, there is a tree which tracks
	 * which regions are in use.  This eliminates the possibility of
	 * conflicts... e.g., two devices trying to use the same iomem region.
	 */
	pisces_init_resources();

	/*
	 * Initialize per-CPU areas, one per CPU.
	 * Variables defined with DEFINE_PER_CPU() end up in the per-CPU area.
	 * This provides a mechanism for different CPUs to refer to their
	 * private copy of the variable using the same name
	 * (e.g., get_cpu_var(foo)).
	 */
	setup_per_cpu_areas();

	/*
	 * Initialize the IDT table and interrupt handlers.
	 */
	interrupts_init();


	/*
	 * Map the APICs into the kernel page tables.
	 * 
	 * Each CPU has its own Local APIC. All Local APICs are memory mapped
	 * to the same virtual address region. A CPU accesses its Local APIC by
	 * accessing the region. A CPU cannot access another CPU's Local APIC.
	 *
	 * Each Local APIC is connected to all IO APICs in the system. Each IO
	 * APIC is mapped to a different virtual address region. A CPU accesses
	 * a given IO APIC by accessing the appropriate region. All CPUs can
	 * access all IO APICs.
	 */

	/*
	 * Get the address of the local APIC from the local API address MSR.
	 * This is needed because Cray sets the local APIC to a non-standard
	 * address, 0xfdfff000.
	 */
	if (!cpu_has_x2apic) {
		unsigned int low, high;
		rdmsr(MSR_IA32_APICBASE, low, high);
		lapic_phys_addr = (((unsigned long)high << 32) | low) & ~0xFFF;
		printk(KERN_INFO "PISCES: Setting lapic_phys_addr to 0x%08lx\n", lapic_phys_addr);
	}

	lapic_map();
	//ioapic_map();

	/*
	 * Initialize the virtual system call code/data page.
	 * The vsyscall page is mapped into every task's address space at a
	 * well-known address.  User code can call functions in this page
	 * directly, providing a light-weight mechanism for read-only system
	 * calls such as gettimeofday().
	 */
	vsyscall_map();

	cpu_init();


	//	ioapic_init();
	printk(KERN_INFO "No I/O APIC in Pisces guest\n");

	lapic_set_timer_freq(sched_hz);



	/* Signal Linux boot loader that we are up */
	pisces_boot_params->initialized = 1;

}

