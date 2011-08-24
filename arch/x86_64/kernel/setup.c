#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/cpuinfo.h>
#include <lwk/bootmem.h>
#include <lwk/smp.h>
#include <lwk/sched.h>
#include <lwk/acpi.h>
#include <arch/bootsetup.h>
#include <arch/e820.h>
#include <arch/page.h>
#include <arch/sections.h>
#include <arch/proto.h>
#include <arch/mpspec.h>
#include <arch/pda.h>
#include <arch/io_apic.h>
#include <arch/pgtable.h>

/**
 * Bitmap of of PTE/PMD entry flags that are supported.
 * This is AND'ed with a PTE/PMD entry before it is installed.
 */
unsigned long __supported_pte_mask __read_mostly = ~0UL;

/**
 * Bitmap of features enabled in the CR4 register.
 */
unsigned long mmu_cr4_features;

/**
 * Start and end addresses of the initrd image.
 */
paddr_t __initdata initrd_start;
paddr_t __initdata initrd_end;

/**
 * Base address and size of the Extended BIOS Data Area.
 */
paddr_t __initdata ebda_addr;
size_t  __initdata ebda_size;
#define EBDA_ADDR_POINTER 0x40E

/**
 * Finds the address and length of the Extended BIOS Data Area.
 */
static void __init
discover_ebda(void)
{
	/*
	 * There is a real-mode segmented pointer pointing to the 
	 * 4K EBDA area at 0x40E
	 */
	ebda_addr = *(unsigned short *)__va(EBDA_ADDR_POINTER);
	ebda_addr <<= 4;

	ebda_size = *(unsigned short *)__va(ebda_addr);

	/* Round EBDA up to pages */
	if (ebda_size == 0)
		ebda_size = 1;
	ebda_size <<= 10;
	ebda_size = round_up(ebda_size + (ebda_addr & ~PAGE_MASK), PAGE_SIZE);
	if (ebda_size > 64*1024)
		ebda_size = 64*1024;
}

/**
 * Checks to see if the "No Execute" memory protection bit is supported.
 * The NX bit is not available on some very early Intel x86_64 processors
 * and can be disabled on newer systems via a BIOS setting.
 */
static void __init
check_for_nx_bit(void)
{
	unsigned long efer;

	rdmsrl(MSR_EFER, efer);
	if (!(efer & EFER_NX)) {
		printk(KERN_WARNING "_PAGE_NX disabled.\n");
		__supported_pte_mask &= ~_PAGE_NX;
	}
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
setup_bootmem_allocator(
	unsigned long	start_pfn,
	unsigned long	end_pfn
)
{
	unsigned long bootmap_size, bootmap;

	bootmap_size = bootmem_bootmap_pages(end_pfn)<<PAGE_SHIFT;
	bootmap = find_e820_area(0, end_pfn<<PAGE_SHIFT, bootmap_size, PAGE_SIZE);
	if (bootmap == -1L)
		panic("Cannot find bootmem map of size %ld\n",bootmap_size);
	bootmap_size = init_bootmem(bootmap >> PAGE_SHIFT, end_pfn);
	e820_bootmem_free(0, end_pfn << PAGE_SHIFT);
	reserve_bootmem(bootmap, bootmap_size);
}

/**
 * Mark in-use memory regions as reserved.
 * This prevents the bootmem allocator from allocating them.
 */
static void __init
reserve_memory(void)
{
	/* Reserve the kernel page table memory */
	reserve_bootmem(table_start << PAGE_SHIFT,
	                (table_end - table_start) << PAGE_SHIFT);

	/* Reserve kernel memory */
	reserve_bootmem(__pa_symbol(&_text),
	                __pa_symbol(&_end) - __pa_symbol(&_text));

	/* Reserve physical page 0... it's a often a special BIOS page */
	reserve_bootmem(0, PAGE_SIZE);

	/* Reserve the Extended BIOS Data Area memory */
	if (ebda_addr)
		reserve_bootmem(ebda_addr, ebda_size);

	/* Reserve SMP trampoline */
	reserve_bootmem(SMP_TRAMPOLINE_BASE, 2*PAGE_SIZE);

	/* Find and reserve boot-time SMP configuration */
	find_mp_config();

	/* Reserve memory used by the initrd image */
	if (LOADER_TYPE && INITRD_START) {
		if (INITRD_START + INITRD_SIZE <= (end_pfn << PAGE_SHIFT)) {
			printk(KERN_DEBUG
			       "reserving memory used by initrd image\n");
			printk(KERN_DEBUG
			       "  INITRD_START=0x%lx, INITRD_SIZE=%ld bytes\n",
			       (unsigned long) INITRD_START,
			       (unsigned long) INITRD_SIZE);
			reserve_bootmem(INITRD_START, INITRD_SIZE);
			initrd_start = INITRD_START;
			initrd_end = initrd_start+INITRD_SIZE;
		} else {
			printk(KERN_ERR
			       "initrd extends beyond end of memory "
			       "(0x%08lx > 0x%08lx)\ndisabling initrd\n",
			       (unsigned long)(INITRD_START + INITRD_SIZE),
			       (unsigned long)(end_pfn << PAGE_SHIFT));
			initrd_start = 0;
		}
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

static inline int get_family(int cpuid)
{       
        int base = (cpuid>>8) & 0xf;
        int extended = (cpuid>>20) &0xff;
                        
        return (0xf == base) ? base + extended : base;
}

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
	/*
	 * Figure out which memory regions are usable and which are reserved.
	 * This builds the "e820" map of memory from info provided by the
	 * BIOS.
	 */
	setup_memory_region();

	/*
 	 * Get the bare minimum info about the bootstrap CPU... the
 	 * one we're executing on right now.  Latter on, the full
 	 * boot_cpu_data and cpu_info[boot_cpu_id] structures will be
 	 * filled in completely.
 	 */
	boot_cpu_data.logical_id = 0;
	early_identify_cpu(&boot_cpu_data);

	/*
	 * Find the Extended BIOS Data Area.
	 * (Not sure why exactly we need this, probably don't.)
	 */ 
	discover_ebda();

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
	setup_bootmem_allocator(0, end_pfn);
	reserve_memory();

	/*
	 * Get the multiprocessor configuration...
	 * number of CPUs, PCI bus info, APIC info, etc.
	 */
	get_mp_config();

	/*
	 * Initialize the ACPI subsystem.
	 */
	acpi_init();

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
	lapic_map();
	ioapic_map();

	/*
	 * Initialize the virtual system call code/data page.
	 * The vsyscall page is mapped into every task's address space at a
	 * well-known address.  User code can call functions in this page
	 * directly, providing a light-weight mechanism for read-only system
	 * calls such as gettimeofday().
	 */
	vsyscall_map();

	cpu_init();

	ioapic_init();

	lapic_set_timer_freq(sched_hz);
}

void disable_APIC_timer(void)
{
      printk( KERN_NOTICE
              "CPU %d: disabling APIC timer.\n", this_cpu);
      lapic_stop_timer();
}


void enable_APIC_timer(void)
{
      lapic_set_timer_freq(sched_hz);
      printk( KERN_NOTICE
              "CPU %d: enabled APIC timer.\n", this_cpu);
}
