#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>
#include <lwk/smp.h>
#include <arch/page.h>
#include <arch/pgtable.h>
#include <arch/fixmap.h>
#include <arch/apicdef.h>
#include <arch/apic.h>
#include <arch/idt_vectors.h>

/**
 * Physical address of the local APIC memory mapping.
 * This is set in arch/x86_64/kernel/mpparse.c
 */
unsigned long lapic_phys_addr;

/**
 * Resource entry for the local APIC memory mapping.
 */
static struct resource lapic_resource = {
	.name  = "Local APIC",
	.flags = IORESOURCE_MEM | IORESOURCE_BUSY,
	/* .start and .end filled in later based on detected address */
};

/**
 * Creates a kernel mapping for the local APIC.
 *
 * The hardware/platform/BIOS maps each CPU's local APIC at the same location
 * in physical memory. This function uses the 'fixmap' to map the local APIC
 * into the kernel's virtual memory space at a fixed virtual address that is
 * known at compile time. Since the local APIC's virtual address is known
 * at compile time, local APIC registers can be accessed directly, without
 * any pointer dereferencing.
 */
void __init
lapic_map(void)
{
	/* Reserve physical memory used by the local APIC */
	lapic_resource.start = lapic_phys_addr;
	lapic_resource.end   = lapic_phys_addr + PAGE_SIZE - 1;
	request_resource(&iomem_resource, &lapic_resource);

	/* Map local APIC into the kernel */ 
	set_fixmap_nocache(FIX_APIC_BASE, lapic_phys_addr);
	printk(KERN_DEBUG "Local APIC mapped to virtual address 0x%016lx\n",
	                  fix_to_virt(FIX_APIC_BASE));
}

/**
 * Initializes the calling CPU's local APIC.
 */
void __init
lapic_init(void)
{
	uint32_t val;

	/*
	 * Initialize Destination Format Register.
	 * When using logical destination mode, we want to use the flat model.
	 */
	apic_write(APIC_DFR, APIC_DFR_FLAT);

	/*
 	 * Initialize the Logical Destination Register.
 	 * The LWK never uses logical destination mode, so just set it to the
 	 * APIC's physical ID to avoid possible confusion.
 	 */
	val = apic_read(APIC_LDR) & ~APIC_LDR_MASK;
	val |= SET_APIC_LOGICAL_ID( GET_APIC_ID(apic_read(APIC_ID)) );
	apic_write(APIC_LDR, val);

	/*
	 * Initialize the Task Priority Register.
	 * We set this to accept all (0) and never touch it again.
	 */
	val = apic_read(APIC_TASKPRI) & ~APIC_TPRI_MASK;
	apic_write(APIC_TASKPRI, val);

	/*
	 * Intialize the Spurious-Interrupt Vector Register.
	 * This also enables the local APIC.
 	 */
	val = apic_read(APIC_SPIV) & ~APIC_VECTOR_MASK;
	val |= (APIC_SPIV_APIC_ENABLED | SPURIOUS_APIC_VECTOR);
	apic_write(APIC_SPIV, val);

	/* Setup LVT[0] = APIC Timer Interrupt */
	apic_write(APIC_LVTT, 0
	             | APIC_DM_FIXED       /* route to fixed IDT vector */
	             | LOCAL_TIMER_VECTOR  /* IDT vector to route to */
	             | APIC_LVT_MASKED     /* initially disable */
	);

	/* Setup LVT[1] = Thermal Sensor Interrupt */
	apic_write(APIC_LVTTHMR, 0
	             | APIC_DM_FIXED       /* route to fixed IDT vector */     
	             | THERMAL_APIC_VECTOR /* IDT vector to route to */
	);

	/* Setup LVT[2] = Performance Counter Interrupt */
	apic_write(APIC_LVTPC, 0
	             | APIC_DM_NMI         /* treat as non-maskable interrupt */
	             | APIC_LVT_MASKED     /* initially disable */
	);

	/* Setup LVT[3] = Local Interrupt Pin 0 */
	apic_write(APIC_LVT0, 0
	             | APIC_DM_EXTINT      /* hooked up to old 8259A PIC */
	             | APIC_LVT_MASKED     /* disable */
	);

	/* Setup LVT[4] = Local Interrupt Pin 1 */
	apic_write(APIC_LVT1, 0
	             | APIC_DM_NMI         /* treat as non-maskable interrupt */
	             | ((cpu_id() != 0)
	                 ? APIC_LVT_MASKED /* mask on all but bootstrap CPU */
	                 : 0)              /* bootstrap CPU (0) receives NMIs */
	);

	/* Setup LVT[5] = Internal APIC Error Detector Interrupt */
	apic_write(APIC_LVTERR, 0
	             | APIC_DM_FIXED       /* route to fixed IDT vector */
	             | ERROR_APIC_VECTOR   /* IDT vector to route to */
	);
	apic_write(APIC_ESR, 0); /* spec says to clear after enabling LVTERR */
}

/**
 * Converts an entry in a local APIC's Local Vector Table to a
 * human-readable string.
 */
static char *
lvt_stringify(uint32_t entry, char *buf)
{
	uint32_t delivery_mode = GET_APIC_DELIVERY_MODE(entry);

	if (delivery_mode == APIC_MODE_FIXED) {
		sprintf(buf, "FIXED -> IDT_VECTOR %d",
			entry & APIC_VECTOR_MASK
		);
	} else if (delivery_mode == APIC_MODE_NMI) {
		sprintf(buf, "NMI"); 
	} else if (delivery_mode == APIC_MODE_EXTINT) {
		sprintf(buf, "ExtINT, hooked to old 8259A PIC");
	} else {
		sprintf(buf, "UNKNOWN");
	}

	if (entry & APIC_LVT_MASKED)
		strcat(buf, ", MASKED");

	return buf;
}

/**
 * Prints various local APIC registers of interest to the console.
 */
void
lapic_dump(void)
{
	char buf[128];

	printk(KERN_DEBUG "LOCAL APIC DUMP (LOGICAL CPU #%d):\n", cpu_id());

	/*
 	 * Lead off with the important stuff...
 	 */
	printk(KERN_DEBUG
		"  ID:  0x%08x (id=%d)\n",
		apic_read(APIC_ID),
		GET_APIC_ID(apic_read(APIC_ID))
	);
	printk(KERN_DEBUG
		"  VER: 0x%08x (version=0x%x, max_lvt=%d)\n",
		apic_read(APIC_LVR),
		GET_APIC_VERSION(apic_read(APIC_LVR)),
		GET_APIC_MAXLVT(apic_read(APIC_LVR))
	);
	printk(KERN_DEBUG
		"  ESR: 0x%08x (Error Status Reg, non-zero is bad)\n",
		apic_read(APIC_ESR)
	);
	printk(KERN_DEBUG
		"  SVR: 0x%08x (Spurious vector=%d, %s)\n",
		apic_read(APIC_SPIV),
		apic_read(APIC_SPIV) & APIC_VECTOR_MASK,
		(apic_read(APIC_SPIV) & APIC_SPIV_APIC_ENABLED)
			? "APIC IS ENABLED"
			: "APIC IS DISABLED"
	);

	/*
 	 * Local Vector Table
 	 */
	printk(KERN_DEBUG "  Local Vector Table Entries:\n");
	printk(KERN_DEBUG "      LVT[0] Timer:     0x%08x (%s)\n",
		apic_read(APIC_LVTT),
		lvt_stringify(apic_read(APIC_LVTT), buf)
	);
	printk(KERN_DEBUG "      LVT[1] Thermal:   0x%08x (%s)\n",
		apic_read(APIC_LVTTHMR),
		lvt_stringify(apic_read(APIC_LVTTHMR), buf)
	);
	printk(KERN_DEBUG "      LVT[2] Perf Cnt:  0x%08x (%s)\n",
		apic_read(APIC_LVTPC),
		lvt_stringify(apic_read(APIC_LVTPC), buf)
	);
	printk(KERN_DEBUG "      LVT[3] LINT0 Pin: 0x%08x (%s)\n",
		apic_read(APIC_LVT0),
		lvt_stringify(apic_read(APIC_LVT0), buf)
	);
	printk(KERN_DEBUG "      LVT[4] LINT1 Pin: 0x%08x (%s)\n",
		apic_read(APIC_LVT1),
		lvt_stringify(apic_read(APIC_LVT1), buf)
	);
	printk(KERN_DEBUG "      LVT[5] Error:     0x%08x (%s)\n",
		apic_read(APIC_LVTERR),
		lvt_stringify(apic_read(APIC_LVTERR), buf)
	);

	/*
 	 * APIC timer configuration registers
 	 */
	printk(KERN_DEBUG "  Local APIC Timer:\n");
	printk(KERN_DEBUG "      DCR (Divide Config Reg): 0x%08x\n",
		apic_read(APIC_TDCR)
	);
	printk(KERN_DEBUG "      ICT (Initial Count Reg): 0x%08x\n",
		apic_read(APIC_TMICT)
	);
	printk(KERN_DEBUG "      CCT (Current Count Reg): 0x%08x\n",
		apic_read(APIC_TMCCT)
	);

	/*
 	 * Logical APIC addressing mode registers
 	 */
	printk(KERN_DEBUG "  Logical Addressing Mode Information:\n");
	printk(KERN_DEBUG "      LDR (Logical Dest Reg):  0x%08x (id=%d)\n",
		apic_read(APIC_LDR),
		GET_APIC_LOGICAL_ID(apic_read(APIC_LDR))
	);
	printk(KERN_DEBUG "      DFR (Dest Format Reg):   0x%08x (%s)\n",
		apic_read(APIC_DFR),
		(apic_read(APIC_DFR) == APIC_DFR_FLAT) ? "FLAT" : "CLUSTER"
	);

	/*
 	 * Task/processor/arbitration priority registers
 	 */
	printk(KERN_DEBUG "  Task/Processor/Arbitration Priorities:\n");
	printk(KERN_DEBUG "      TPR (Task Priority Reg):        0x%08x\n",
		apic_read(APIC_TASKPRI)
	);
	printk(KERN_DEBUG "      PPR (Processor Priority Reg):   0x%08x\n",
		apic_read(APIC_PROCPRI)
	);
	printk(KERN_DEBUG "      APR (Arbitration Priority Reg): 0x%08x\n",
		apic_read(APIC_ARBPRI)
	);
}

