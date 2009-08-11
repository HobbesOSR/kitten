#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/delay.h>
#include <arch/page.h>
#include <arch/pgtable.h>
#include <arch/fixmap.h>
#include <arch/apicdef.h>
#include <arch/apic.h>
#include <arch/idt_vectors.h>
#include <arch/tsc.h>

/**
 * Physical address of the local APIC memory mapping.
 * If the system BIOS provided an MP configuration table, this is set in
 * arch/x86_64/kernel/mpparse.c to the value parsed from the table.
 * Otherwise, the default address is used.
 */
unsigned long lapic_phys_addr = APIC_DEFAULT_PHYS_BASE;

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
	if (!cpu_has_apic)
		panic("No local APIC.");

	/* Reserve physical memory used by the local APIC */
	lapic_resource.start = lapic_phys_addr;
	lapic_resource.end   = lapic_phys_addr + 4096 - 1;
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
	val |= (APIC_SPIV_APIC_ENABLED | APIC_SPURIOUS_VECTOR);
	apic_write(APIC_SPIV, val);

	/* Setup LVT[0] = APIC Timer Interrupt */
	apic_write(APIC_LVTT, 0
	             | APIC_DM_FIXED       /* route to fixed IDT vector */
	             | APIC_TIMER_VECTOR   /* IDT vector to route to */
	             | APIC_LVT_MASKED     /* initially disable */
	);

	/* Setup LVT[1] = Thermal Sensor Interrupt */
	apic_write(APIC_LVTTHMR, 0
	             | APIC_DM_FIXED       /* route to fixed IDT vector */     
	             | APIC_THERMAL_VECTOR /* IDT vector to route to */
	);

	/* Setup LVT[2] = Performance Counter Interrupt */
	apic_write(APIC_LVTPC, 0
	             | APIC_DM_NMI         /* treat as non-maskable interrupt */
	                                   /* NMIs are routed to IDT vector 2 */
	             | APIC_LVT_MASKED     /* initially disable */
	);

	/* Setup LVT[3] = Local Interrupt Pin 0 */
	apic_write(APIC_LVT0, 0
	             | APIC_DM_EXTINT      /* hooked up to old 8259A PIC   */
	                                   /* IDT vector provided by 8259A */
	             | APIC_LVT_MASKED     /* disable */
	);

	/* Setup LVT[4] = Local Interrupt Pin 1 */
	apic_write(APIC_LVT1, 0
	             | APIC_DM_NMI         /* treat as non-maskable interrupt */
	                                   /* NMIs are routed to IDT vector 2 */
	             | ((this_cpu != 0)
	                 ? APIC_LVT_MASKED /* mask on all but bootstrap CPU */
	                 : 0)              /* bootstrap CPU (0) receives NMIs */
	);

	/* Setup LVT[5] = Internal APIC Error Detector Interrupt */
	apic_write(APIC_LVTERR, 0
	             | APIC_DM_FIXED       /* route to fixed IDT vector */
	             | APIC_ERROR_VECTOR   /* IDT vector to route to */
	);
	apic_write(APIC_ESR, 0); /* spec says to clear after enabling LVTERR */
}

static void 
lapic_set_timer_count(uint32_t count)
{
	uint32_t lvt;

	/* Setup Divide Count Register to use the bus frequency directly. */
	apic_write(APIC_TDCR, APIC_TDR_DIV_1);

	/* Program the initial count register */
	apic_write(APIC_TMICT, count);

	/* Enable the local APIC timer */
	lvt = apic_read(APIC_LVTT);
	lvt &= ~APIC_LVT_MASKED;
	lvt |= APIC_LVT_TIMER_PERIODIC;
	apic_write(APIC_LVTT, lvt);
}

/**
 * Configures the APIC timer to fire at 'hz' times per second.
 */
void
lapic_set_timer_freq(unsigned int hz)
{
	uint64_t count = cpu_info[this_cpu].arch.lapic_khz * 1000ul / hz;
	lapic_set_timer_count((uint32_t)count);

	printk(KERN_DEBUG
	       "CPU %u: LAPIC timer set to %u Hz (count=%llu).\n",
               this_cpu, hz, count);
}

void
lapic_stop_timer(void)
{
	uint32_t lvt;

	/* Set the initial count to 0 */
	apic_write(APIC_TMICT, 0);

	/* Enable the local APIC timer */
	lvt = apic_read(APIC_LVTT);
	lvt |= APIC_LVT_MASKED;
	apic_write(APIC_LVTT, lvt);
}

/**
 * Detects the local APIC reference bus clock. The only sure-fire way to do
 * this is to depend on some other absolute timing source. This function uses
 * the CPU's cycle counter and the previously detected CPU clock frequency.
 *
 * NOTE: This assumes that the CPU's clock frequency has already been detected.
 *       (i.e., cpu_info[cpu_id()].arch.tsc_khz has been initialized.
 */
unsigned int __init
lapic_calibrate_timer(void)
{
	const unsigned int tick_count = 100000000;
	cycles_t tsc_start, tsc_now;
	uint32_t apic_start, apic_now;
	unsigned int apic_Hz;

	/* Start the APIC counter running for calibration */
	lapic_set_timer_count(4000000000);

	apic_start = apic_read(APIC_TMCCT);
	tsc_start  = get_cycles_sync();

	/* Spin until enough ticks for a meaningful result have elapsed */
	do {
		apic_now = apic_read(APIC_TMCCT);
		tsc_now  = get_cycles_sync();
	} while ( ((tsc_now    - tsc_start) < tick_count) &&
	          ((apic_start - apic_now)  < tick_count) );

	apic_Hz = (apic_start - apic_now) * 1000L *
	          cpu_info[this_cpu].arch.tsc_khz / (tsc_now - tsc_start);

	lapic_stop_timer();

	return (apic_Hz / 1000);
}

static uint32_t
lapic_wait4_icr_idle(void)
{
	uint32_t send_status;
	int timeout;

	/* Wait up to 100 milliseconds */
	timeout = 0;
	do {
		send_status = apic_read(APIC_ICR) & APIC_ICR_BUSY;
		if (!send_status)
			break;
		udelay(100);
	} while (timeout++ < 1000);

	return send_status;
}

/**
 * Returns the number of entries in the Local Vector Table minus one.
 * 
 * This should return 5 or higher on all x86_64 CPUs.
 * 6 is returned if the APIC Thermal Interrupt is supported, 5 otherwise.
 */
static uint32_t
lapic_get_maxlvt(void)
{
	return GET_APIC_MAXLVT(apic_read(APIC_LVR));
}

/**
 * Sends an INIT inter-processor interrupt.
 * This is used during bootstrap to wakeup the AP CPUs.
 */
void __init
lapic_send_init_ipi(unsigned int cpu)
{
	uint32_t status;
	unsigned int apic_id = cpu_info[cpu].arch.apic_id;

	/* Turn on INIT at target CPU */
	apic_write(APIC_ICR2, SET_APIC_DEST_FIELD(apic_id));
	apic_write(APIC_ICR, APIC_INT_LEVELTRIG | APIC_INT_ASSERT
				| APIC_DM_INIT);
	status = lapic_wait4_icr_idle();
	if (status)
		panic("INIT IPI ERROR: failed to assert INIT. (%x)", status);
	mdelay(10);

	/* Turn off INIT at target CPU */
	apic_write(APIC_ICR2, SET_APIC_DEST_FIELD(apic_id));
	apic_write(APIC_ICR, APIC_INT_LEVELTRIG | APIC_DM_INIT);
	status = lapic_wait4_icr_idle();
	if (status)
		panic("INIT IPI ERROR: failed to deassert INIT. (%x)", status);
}

/**
 * Send a STARTUP inter-processor interrupt.
 * This is used during bootstrap to wakeup the AP CPUs.
 */
void __init
lapic_send_startup_ipi(
	unsigned int	cpu,		/* Logical CPU ID */
	unsigned long	start_rip	/* Physical addr  */
)
{
	uint32_t status;
	unsigned int maxlvt  = lapic_get_maxlvt();
	unsigned int apic_id = cpu_info[cpu].arch.apic_id;

	/* Clear errors */
	apic_write(APIC_ESR, 0);
	apic_read(APIC_ESR);

	/* Set target CPU */
	apic_write(APIC_ICR2, SET_APIC_DEST_FIELD(apic_id));

	/* Send Startup IPI to target CPU */
	apic_write(APIC_ICR2, SET_APIC_DEST_FIELD(apic_id));
	apic_write(APIC_ICR, APIC_DM_STARTUP | (start_rip >> 12));
	udelay(300);  /* Give AP CPU some time to accept the IPI */
	status = lapic_wait4_icr_idle();
	if (status)
		panic("STARTUP IPI ERROR: failed to send. (%x)", status);
	udelay(300);  /* Give AP CPU some time to accept the IPI */

	/* Fixup for Pentium erratum 3AP, clear errors */
	if (maxlvt > 3)
		apic_write(APIC_ESR, 0);

	/* Verify that IPI was accepted */
	status = (apic_read(APIC_ESR) & 0xEF);
	if (status)
		panic("STARTUP IPI ERROR: failed to accept. (%x)", status);
}

/**
 * Sends an inter-processor interrupt (IPI) to the specified CPU.
 * Note that the IPI has not necessarily been delivered when this function
 * returns.
 */
void
lapic_send_ipi(
	unsigned int	cpu,		/* Logical CPU ID */
	unsigned int	vector		/* Interrupt vector to send */
)
{
	uint32_t status;
	unsigned int apic_id;

	/* Wait for idle */
	status = lapic_wait4_icr_idle();
	if (status)
		panic("lapic_wait4_icr_idle() timed out. (%x)", status);

	/* Set target CPU */
	apic_id = cpu_info[cpu].arch.apic_id;
	apic_write(APIC_ICR2, SET_APIC_DEST_FIELD(apic_id));

	/* Send the IPI */
	if (unlikely(vector == NMI_VECTOR))
		apic_write(APIC_ICR, APIC_DEST_PHYSICAL|APIC_DM_NMI);
	else
		apic_write(APIC_ICR, APIC_DEST_PHYSICAL|APIC_DM_FIXED|vector);
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
		sprintf(buf, "NMI   -> IDT VECTOR 2"); 
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

	printk(KERN_DEBUG "LOCAL APIC DUMP (LOGICAL CPU #%d):\n", this_cpu);

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

