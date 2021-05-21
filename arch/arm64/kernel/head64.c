/** \file
 * Initial C entry point for lwk.
 */
#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/string.h>
#include <lwk/screen_info.h>
#include <lwk/params.h>
#include <lwk/smp.h>
#include <lwk/cpuinfo.h>
#include <lwk/tlbflush.h>
#include <arch/bootsetup.h>
#include <arch/sections.h>
#include <arch/pda.h>
#include <arch/processor.h>
//#include <arch/desc.h>
#include <arch/proto.h>
#include <arch/page.h>
#include <arch/pgtable.h>
#include <arch/of_fdt.h>
#include <arch/io.h>

static const char * cpu_name;
static const char * machine_name;


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
//struct desc_ptr idt_descr = { 256 * 16 - 1, (unsigned long) idt_table };

/**
 * Array of pointers to each CPU's per-processor data area.
 * The array is indexed by CPU ID.
 */
struct ARM64_pda *_cpu_pda[NR_CPUS] __read_mostly;

/**
 * Array of per-processor data area structures, one per CPU.
 * The array is indexed by CPU ID.
 */
struct ARM64_pda boot_cpu_pda[NR_CPUS] __cacheline_aligned;

/**
 * This unmaps virtual addresses [0,512GB) by clearing the first entry in the
 * PGD/PML4T. After this executes, accesses to virtual addresses [0,512GB) will
 * cause a page fault.
 */
static void __init
zap_identity_mappings(void)
{
	//pgd_t *pgd = pgd_offset_k(0UL);
	//pgd_clear(pgd);
	//__flush_tlb();
}

/**
 * Determines the address of the kernel boot command line.
 */
static char * __init
find_command_line(void)
{
	unsigned long new_data;

	new_data = *(u32 *) (x86_boot_params + NEW_CL_POINTER);
	if (!new_data) {
		if (OLD_CL_MAGIC != * (u16 *) OLD_CL_MAGIC_ADDR) {
			return NULL;
		}
		new_data = OLD_CL_BASE_ADDR + * (u16 *) OLD_CL_OFFSET;
	}
	return __va(new_data);
}





void __init early_init_dt_setup_initrd_arch(unsigned long start,
					    unsigned long end)
{
	initrd_start = start;
	initrd_end   = end;
}



void __init early_init_dt_add_memory_arch(u64 base, u64 size)
{
	base &= PAGE_MASK;
	size &= PAGE_MASK;
	if (base + size < PHYS_OFFSET) {
		//pr_warning("Ignoring memory block 0x%llx - 0x%llx\n",
			//   base, base + size);
		return;
	}
	if (base < PHYS_OFFSET) {
		//pr_warning("Ignoring memory range 0x%llx - 0x%llx\n",
			//   base, PHYS_OFFSET);
		size -= PHYS_OFFSET - base;
		base = PHYS_OFFSET;
	}
	early_printk("Memblock add %p [size=%d MB]\n", base, size >> 20);
	memblock_add(base, size);
}


static void __init 
early_fdt_setup(phys_addr_t dt_phys)
{
	struct boot_param_header *devtree;
	unsigned long dt_root;

	early_printk("setup_machine_fdt()\n");

	/* Check we have a non-NULL DT pointer */
	if (!dt_phys) {
		early_printk("\n"
			"Error: NULL or invalid device tree blob\n"
			"The dtb must be 8-byte aligned and passed in the first 512MB of memory\n"
			"\nPlease check your bootloader.\n");

		while (true)
			cpu_relax();

	}



	devtree = phys_to_virt(dt_phys);

	/* Check device tree validity */
	if (be32_to_cpu(devtree->magic) != OF_DT_HEADER) {
		early_printk("\n"
			"Error: invalid device tree blob at physical address 0x%p (virtual address 0x%p)\n"
			"Expected 0x%x, found 0x%x\n"
			"\nPlease check your bootloader.\n",
			dt_phys, devtree, OF_DT_HEADER,
			be32_to_cpu(devtree->magic));

		while (true)
			cpu_relax();
	}

	initial_boot_params = devtree;

	/* Update fdt_end based on size of devtree */
	fdt_end = fdt_start + be32_to_cpu(initial_boot_params->totalsize);


	dt_root = of_get_flat_dt_root();

	machine_name = of_get_flat_dt_prop(dt_root, "model", NULL);
	if (!machine_name)
		machine_name = of_get_flat_dt_prop(dt_root, "compatible", NULL);
	if (!machine_name)
		machine_name = "<unknown>";
	early_printk("Machine: %s\n", machine_name);

	/* Retrieve various information from the /chosen node */
	of_scan_flat_dt(early_init_dt_scan_chosen, lwk_command_line);
	/* Initialize {size,address}-cells info */
	of_scan_flat_dt(early_init_dt_scan_root, NULL);
	/* Setup memory, calling early_init_dt_add_memory_arch */
	of_scan_flat_dt(early_init_dt_scan_memory, NULL);
}



void __init
arm64_start_kernel( void ) {
	phys_addr_t ttbr1;
	struct tcr_el1 tcr;
	int32_t i;


	memset(__bss_start, 0,
	      (unsigned long) __bss_stop - (unsigned long) __bss_start);


	early_printk("FDT Located At %p\n", fdt_start);
	early_printk("memstart_addr: %p\n", memstart_addr);

	early_fdt_setup(fdt_start);


	for (i = 0; i < NR_CPUS; i++)
		cpu_pda(i) = &boot_cpu_pda[i];
	pda_init(0, &bootstrap_task_union.task_info);

	start_kernel();
}
