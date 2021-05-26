#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/cpuinfo.h>
#include <lwk/bootmem.h>
#include <lwk/smp.h>
#include <lwk/sched.h>
#include <lwk/acpi.h>
#include <arch/bootsetup.h>
#include <arch/page.h>
#include <arch/sections.h>
#include <arch/proto.h>
#include <arch/mpspec.h>
#include <arch/pda.h>
//#include <arch/io_apic.h>
#include <arch/pgtable.h>
#include <arch/mce.h>
#include <arch/of_fdt.h>
#include <arch/memory.h>
#include <arch/memblock.h>
#include <arch/cputype.h>


/**
 * Bitmap of of PTE/PMD entry flags that are supported.
 * This is AND'ed with a PTE/PMD entry before it is installed.
 */
unsigned long __supported_pte_mask __read_mostly = ~0UL;

extern int early_printk(const char * fmt, ...);



phys_addr_t memstart_addr __read_mostly = 0;



paddr_t __initdata fdt_start;
paddr_t __initdata fdt_end;


/**
 * Start and end addresses of the initrd image.
 */
paddr_t __initdata initrd_start;
paddr_t __initdata initrd_end;


uint64_t __initdata init_task_start;
uint64_t __initdata init_task_size;
#include <lwk/params.h>
param(init_task_start, ulong);
param(init_task_size, ulong);







/**
 * Checks to see if the "No Execute" memory protection bit is supported.
 * The NX bit is not available on some very early Intel x86_64 processors
 * and can be disabled on newer systems via a BIOS setting.
 */
static void __init
check_for_nx_bit(void)
{
#if 0
	unsigned long efer;

	rdmsrl(MSR_EFER, efer);
	if (!(efer & EFER_NX)) {
		printk(KERN_WARNING "_PAGE_NX disabled.\n");
		__supported_pte_mask &= ~_PAGE_NX;
	}
#endif
}

/**
 * This sets up the bootstrap memory allocator.  It is a simple
 * bitmap based allocator that tracks memory at a page granularity.
 * Once the bootstrap process is complete, each unallocated page
 * is added to the real memory allocator's free pool.  Memory allocated
 * during bootstrap remains allocated forever, unless explicitly
 * freed before turning things over to the real memory allocator.
 */
static void __init
setup_bootmem_allocator(unsigned long	start_pfn,
			unsigned long	end_pfn)
{
	struct memblock_region * reg          = NULL;
	unsigned long            bootmap_size = 0;
	phys_addr_t              phys         = 0;

	printk("Setting up bootmem allocator\n");

	bootmap_size = bootmem_bootmap_pages((end_pfn - start_pfn));
	phys         = memblock_alloc(bootmap_size * PAGE_SIZE, 16);
	bootmap_size = init_bootmem(phys >> PAGE_SHIFT, start_pfn, end_pfn);


	for_each_memblock(memory, reg) {
		if (reg->size == 0) {
			continue;
		}

		printk("Freeing Bootmem at %p [%d bytes]\n", reg->base, reg->size);

		free_bootmem(reg->base, reg->size);
	}


	reserve_bootmem(phys, bootmap_size);

	for_each_memblock(reserved, reg) {
		printk("Reserving Bootmem %p [%d bytes]\n", reg->base, reg->size);

		reserve_bootmem(reg->base, reg->size);
	}


}

/**
 * This initializes a per-CPU area for each CPU.
 *
 * TODO: The PDA and per-CPU areas are pretty tightly wound.  It should be
 *       possible to make the per-CPU area *be* the PDA, or put another way,
 *       point %GS at the per-CPU area rather than the PDA.  All of the PDA's
 *       current contents would become normal per-CPU variables.
 */
static void __init
setup_per_cpu_areas(void)
{ 

	int i;
	size_t size;

	/*
 	 * There is an ELF section containing all per-CPU variables
 	 * surrounded by __per_cpu_start and __per_cpu_end symbols.
 	 * We create a copy of this ELF section for each CPU.
 	 */
	size = ALIGN(__per_cpu_end - __per_cpu_start, SMP_CACHE_BYTES);

	printk("Setting up per-cpu areas (size=%d)\n", size);
	printk("\t__per_cpu_start = %p, __per_cpu_end = %p\n", __per_cpu_start, __per_cpu_end);


	for_each_cpu_mask (i, cpu_present_map) {
		char *ptr;

		ptr = alloc_bootmem_aligned(size, PAGE_SIZE);
		if (!ptr)
			panic("Cannot allocate cpu data for CPU %d\n", i);

		/*
 		 * Pre-bias data_offset by subtracting its offset from
 		 * __per_cpu_start.  Later, per_cpu() will calculate a
 		 * per_cpu variable's address with:
 		 * 
 		 * addr = offset_in_percpu_ELF_section + data_offset
 		 *      = (__per_cpu_start + offset)   + (ptr - __per_cpu_start)
 		 *      =                    offset    +  ptr
 		 */
		cpu_pda(i)->data_offset = ptr - __per_cpu_start;

		memcpy(ptr, __per_cpu_start, __per_cpu_end - __per_cpu_start);
	}
} 

static void __init
memblock_init(void)
{
	u64 * reserve_map;
	u64   base;
	u64   size;

	/* Register the kernel text, kernel data and initrd with memblock */
	memblock_reserve(__pa(_text), _end - _text);
	

	if (initrd_start) {
		paddr_t initrd_size = initrd_end - initrd_start;

		memblock_reserve(initrd_start, initrd_size);

	} else {
		panic("No initrd (init_task) provided\n");
	}


	/*
	 * Reserve the page tables.  These are already in use,
	 * and can only be in node 0.
	 */
	memblock_reserve(__pa(swapper_pg_dir), SWAPPER_DIR_SIZE);
	memblock_reserve(__pa(idmap_pg_dir), IDMAP_DIR_SIZE);

	/* Reserve the dtb region */
	memblock_reserve(virt_to_phys(initial_boot_params),
			 be32_to_cpu(initial_boot_params->totalsize));

	/*
	 * Process the reserve map.  This will probably overlap the initrd
	 * and dtb locations which are already reserved, but overlapping
	 * doesn't hurt anything
	 */
	reserve_map = ((void*)initial_boot_params) +
			be32_to_cpu(initial_boot_params->off_mem_rsvmap);
	while (1) {
		base = be64_to_cpup(reserve_map++);
		size = be64_to_cpup(reserve_map++);
		
		if (!size) {
			break;
		}

		memblock_reserve(base, size);
	}

	memblock_dump_all();
}


static inline int get_family(int cpuid)
{       
        int base     = (cpuid >> 8)  & 0x0f;
        int extended = (cpuid >> 20) & 0xff;
                        
        return (0xf == base) ? base + extended : base;
}


u64 __cpu_logical_map[NR_CPUS] = { [0 ... NR_CPUS-1] = INVALID_HWID };

/**
 * Architecture specific initialization.
 * This is called from start_kernel() in init/main.c.
 *
 * NOTE: Ordering is usually important.  Do not move things
 *       around unless you know what you are doing.
 */
void __init
setup_arch(void)
{
	phys_addr_t start, end;
	u64 i;



	/* This is a bit convoluted...
	 * Basically, the init_task can either be specified explicity
	Command line init task location takes priority over FDT */
	if ((init_task_start != 0) && 
	    (init_task_size  != 0) ) {
		initrd_start = init_task_start;
		initrd_end   = init_task_start + init_task_size;
	}


	memblock_init();

	paging_init();





	/*
	 * Figure out which memory regions are usable and which are reserved.
	 * This builds the "e820" map of memory from info provided by the
	 * BIOS.
	 */
	//setup_memory_region();

	/*
 	 * Get the bare minimum info about the bootstrap CPU... the
 	 * one we're executing on right now.  Latter on, the full
 	 * boot_cpu_data and cpu_info[boot_cpu_id] structures will be
 	 * filled in completely.
 	 */
	// TODO: Brian: This does nothing... but not positive
	boot_cpu_data.logical_id = 0;
	early_identify_cpu(&boot_cpu_data);

	/*
	 * Initialize the kernel page tables.
	 * The kernel page tables map an "identity" map of all physical memory
	 * starting at virtual address PAGE_OFFSET.  When the kernel executes,
	 * it runs inside of the identity map... memory below PAGE_OFFSET is
	 * from whatever task was running when the kernel got invoked.
	 */
	//check_for_nx_bit();
	//init_kernel_pgtables(0, (end_pfn_map << PAGE_SHIFT));

	/*
 	 * Initialize the bootstrap dynamic memory allocator.
 	 * alloc_bootmem() will work after this.
 	 */
	//setup_bootmem_allocator(0, end_pfn);
	// memblock to bootmem
	start = memblock_start_of_DRAM();
	end   = memblock_end_of_DRAM();

	setup_bootmem_allocator(start >> PAGE_SHIFT, end >> PAGE_SHIFT);
	printk("BOOT_MEM start: %p\n",  start);
	printk("BOOT_MEM   end: %p\n",  end);
 
	// Must free available memory

	/*
	 * Initialize the ACPI subsystem.
	 */
	//acpi_init();
	disable_acpi();



	/*
	 * Initialize resources.  Resources reserve sections of normal memory
	 * (iomem) and I/O ports (ioport) for devices and other system
	 * resources.  For each resource type, there is a tree which tracks
	 * which regions are in use.  This eliminates the possibility of
	 * conflicts... e.g., two devices trying to use the same iomem region.
	 */
	init_resources();

	/*
 	 * Initialize per-CPU areas, one per CPU.
 	 * Variables defined with DEFINE_PER_CPU() end up in the per-CPU area.
 	 * This provides a mechanism for different CPUs to refer to their
 	 * private copy of the variable using the same name
 	 * (e.g., get_cpu_var(foo)).
 	 */



	unflatten_device_tree();

	/* Initialize PSCI interface for SMP operations */
	if (acpi_disabled)
		psci_dt_init();
	else
		psci_acpi_init();

	/*
	 * Get the multiprocessor configuration...
	 * number of CPUs, PCI bus info, APIC info, etc.
	 */
	smp_init_boot_cpu();	
	smp_init_cpus();



	smp_prepare_boot_cpu();
	smp_prepare_cpus(num_possible_cpus());

	printk("Per cpu areas\n");
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
	//lapic_map();
	//ioapic_map();
	irqchip_global_init();

	/*
	 * Initialize the virtual system call code/data page.
	 * The vsyscall page is mapped into every task's address space at a
	 * well-known address.  User code can call functions in this page
	 * directly, providing a light-weight mechanism for read-only system
	 * calls such as gettimeofday().
	 */
	printk("Syscall map NOT initialized\n");
	//vsyscall_map();

	cpu_init();

	//ioapic_init();


    //mcheck_init();
}

void disable_APIC_timer(void)
{
#if 0
      printk( KERN_NOTICE
              "CPU %d: disabling APIC timer.\n", this_cpu);
      lapic_stop_timer();
#endif
}


void enable_APIC_timer(void)
{
#if 0
      lapic_set_timer_freq(sched_hz);
      printk( KERN_NOTICE
              "CPU %d: enabled APIC timer.\n", this_cpu);
#endif
}



