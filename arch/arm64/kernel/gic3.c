/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>
#include <lwk/cpuinfo.h>

#include <arch/irqchip.h>
#include <arch/msr.h>
#include <arch/gicdef.h>
#include <arch/of.h>
#include <arch/io.h>
#include <arch/irq_vectors.h>
#include <arch/topology.h>


#define GIC3_SPURIOUS_IRQ 1023

struct gic3 {
	vaddr_t gicd_virt_start;
	paddr_t gicd_phys_start;
	paddr_t gicd_phys_size;

	vaddr_t gicr_virt_start;
	paddr_t gicr_phys_start;
	paddr_t gicr_phys_size;
};


static struct gic3 gic;




static int
__gic3_parse_irqs(struct device_node *  dt_node, 
	          uint32_t              num_irqs, 
	          struct irq_def     *  irqs);


static inline uint32_t 
__gicd_read32(uintptr_t offset)
{
	return readl(gic.gicd_virt_start + offset);
}

static inline void
__gicd_write32(uintptr_t offset, 
	       uint32_t  value)
{
	writel(value, gic.gicd_virt_start + offset);
}




static inline uint32_t 
__gicr_read32(uint32_t  cpu,
	      uintptr_t offset)
{
	return readl(gic.gicr_virt_start + GICR_CPU_OFFSET(cpu) + offset);
}

static inline void
__gicr_write32(uint32_t  cpu,
	       uintptr_t offset, 
	       uint32_t  value)
{
	writel(value, gic.gicr_virt_start + GICR_CPU_OFFSET(cpu) + offset);
}


static inline uint32_t 
__gicr_read64(uint32_t  cpu,
	      uintptr_t offset)
{
	return readq(gic.gicr_virt_start + GICR_CPU_OFFSET(cpu) + offset);
}

static inline void
__gicr_write64(uint32_t  cpu,
	       uintptr_t offset, 
	       uint64_t  value)
{
	writeq(value, gic.gicr_virt_start + GICR_CPU_OFFSET(cpu) + offset);
}



static void 
__gic3_print_pending_irqs(void)
{
	uint32_t pending = 0;
	uint32_t enabled = 0;
	struct gicr_ipriorityr ipriority = {0};
	uint64_t icc_rpr = mrs(ICC_RPR_EL1);
	uint64_t icc_pmr = mrs(ICC_PMR_EL1);
	uint64_t hppir0   = mrs(ICC_HPPIR0_EL1);
	uint64_t hppir1   = mrs(ICC_HPPIR1_EL1);
	uint32_t gicd_ctlr = 0;
	uint64_t gicr_typer = 0;

	pending = __gicr_read32(this_cpu, GICR_ISPENDR_0_OFFSET);
	enabled = __gicr_read32(this_cpu, GICR_ISENABLER_0_OFFSET);
	printk("GICR_pending_0 = %x, enabled_0=%x\n", pending, enabled);

	ipriority.val = __gicr_read32(this_cpu, GICR_IPRIORITYR_OFFSET(7));
	printk("GICR_IPRIORITY(7) = %x, icc_rpr = %p, icc_pmr = %p\n", ipriority.val, (void *)icc_rpr, (void *)icc_pmr);
	printk("ICC_HPPIR0_EL1 = %ld, ICC_HPPIR1_EL1 = %ld\n", hppir0, hppir1);

	gicd_ctlr = __gicd_read32(GICD_CTLR_OFFSET);
	printk("GICD_CTLR Value = %x\n", gicd_ctlr);

	gicr_typer = __gicr_read64(this_cpu, GICR_TYPER_OFFSET);
	printk("GICR_TYPER = 0x%lx\n", gicr_typer);


}

static void 
__gic3_dump_state(void)
{
	printk("Dumping GIC3 state not implemented\n");
}





static void
__gic3_enable_irq(uint32_t           irq_num, 
		  irq_trigger_mode_t trigger_mode)
{
	struct gicr_icfgr      icfgr     = {0};
	struct gicd_ipriorityr ipriority = {0};

	uint32_t icfgr_index    = 0; // depends on the irq_num
	uint32_t icfgr_shift    = 0; // depends on the irq_num
	uint32_t icfgr_mode     = 0;
	uint32_t icfgr_mask     = 0;

	uint32_t priority_maj_index = irq_num / 4;
	uint32_t priority_min_index = irq_num % 4;
	uint8_t  priority           = 0;

	uint32_t pending_index      = irq_num / 32;
	uint32_t pending_shift      = irq_num % 32;

	uint32_t enable_index       = irq_num / 32;
	uint32_t enable_shift       = irq_num % 32;

	printk("Enabling IRQ %d\n", irq_num);

	if (trigger_mode == IRQ_LEVEL_TRIGGERED) {
		icfgr_mode = 0x0;
	} else if (trigger_mode == IRQ_EDGE_TRIGGERED) {
		icfgr_mode = 0x1;
	}

	if (irq_num < 32) {
		icfgr_index = (irq_num / 16);
		icfgr_shift = (irq_num % 16) * 2;
		icfgr_mask  = 0x3;
	} else {
		icfgr_index = (irq_num / 32) + 1;
		icfgr_shift = (irq_num % 32);
		icfgr_mask  = 0x1;
	}

	// set trigger mode 
	icfgr.val = __gicr_read32(this_cpu, GICR_ICFGR_OFFSET(icfgr_index));
	icfgr.bits &= ~(icfgr_mask << icfgr_shift);
	icfgr.bits |=  (icfgr_mode << icfgr_shift);
	__gicr_write32(this_cpu, GICR_ICFGR_OFFSET(icfgr_index), icfgr.val);

	// set priority to highest
	ipriority.val = __gicr_read32(this_cpu, GICR_IPRIORITYR_OFFSET(priority_maj_index));
	ipriority.bits[priority_min_index] = priority;
	__gicr_write32(this_cpu, GICR_IPRIORITYR_OFFSET(priority_maj_index), ipriority.val);

	// clear pending
	__gicr_write32(this_cpu, GICR_ICPENDR_OFFSET(pending_index), 0x1U << pending_shift);

	// enable irq
	__gicr_write32(this_cpu, GICR_ISENABLER_OFFSET(enable_index), 0x1U << enable_shift);
}


static void
__gic3_disable_irq(uint32_t vector)
{
	panic("Disabling IRQs not supported\n");
}

static uint32_t 
__gic3_ack_irq(void)
{
	return mrs(ICC_IAR1_EL1);
}

static void
__gic3_do_eoi(int vector)
{
	msr(ICC_EOIR1_EL1, vector);
}



static void
__gic3_send_ipi(int target_cpu, uint32_t vector)
{
	struct arch_cpuinfo * const arch_info = &cpu_info[target_cpu].arch;
	struct icc_sgir   sgir = {0};
	struct icc_ctlr   ctlr = {mrs(ICC_CTLR_EL1)};

	int ret = 0;

	if ((ctlr.rss   == 0) && 
	    (target_cpu >= 16)) {
		panic("GIC Configuration error! GIC does not support this many CPUs\n");
	}

	sgir.rs       =         arch_info->cpu_core_id / 16;
	sgir.tgt_list = 0x1 << (arch_info->cpu_core_id % 16);

	sgir.aff1 = arch_info->cpu_cluster_id;
	sgir.aff2 = arch_info->cpu_affinity_2;
	sgir.aff3 = arch_info->cpu_affinity_3;
	sgir.irm  = 0;

	sgir.intid = vector;

	dsb(ishst);
	msr(ICC_SGI1R_EL1, sgir.val);
	isb();
}



static int
__gicd_setup()
{
	struct gicd_ctlr_ns  ctlr  = {0};
	struct gicd_pidr2    pidr2 = {0};
	struct gicd_typer    typer = {0};

	int i = 0;

	pidr2.val = __gicd_read32(GICD_PIDR2_OFFSET);

	// disable GICD
	__gicd_write32(GICD_CTLR_OFFSET, ctlr.val); 

	// disable all irqs (manual says there are 32 registers...)
	for (i = 0; i < 32; i++) {
		__gicd_write32(GICD_ICENABLER_OFFSET(i), 0xffffffff);
	}

	// clear all pending irqs
	for (i = 1; i < 32; i++) {
		__gicd_write32(GICD_ICPENDR_OFFSET(i), 0xffffffff);
	}

	// set all SPIs to lowest priority
	for (i = 0; i < 255; i++) {
		__gicd_write32(GICD_IPRIORITYR_OFFSET(i), 0xffffffff);
	}


	// send all SPIs to cpu 0
	for (i = 8; i < 255; i++) {
		__gicd_write32(GICD_ITARGETSR_OFFSET(i), 0x00000000);
	}	


	// set all SPIs to level triggered
	for (i = 2; i < 64; i++) {
		__gicd_write32(GICD_ICFGR_OFFSET(i), 0x00000000);
	}

	ctlr.val = __gicd_read32(GICD_CTLR_OFFSET);
	printk("CTLR Value = %x\n", ctlr.val);

	// enable GICD
	__gicd_write32(GICD_CTLR_OFFSET, 0x3); 

	ctlr.val = __gicd_read32(GICD_CTLR_OFFSET);
	printk("CTLR Value = %x\n", ctlr.val);
}




static int
__gicr_setup()
{
	struct gicr_waker waker    = {0};
	struct gicr_pidr2 pidr2 = {0};
	int i = 0;

	pidr2.val = __gicr_read32(this_cpu, GICR_PIDR2_OFFSET);

	waker.val = __gicr_read32(this_cpu, GICR_WAKER_OFFSET);

	printk("Waker = %x\n", waker.val);

	waker.processor_sleep = 0;
	__gicr_write32(this_cpu, GICR_WAKER_OFFSET, waker.val);


	while (1) {
		printk("Polling children Asleep\n");
		waker.val = __gicr_read32(this_cpu, GICR_WAKER_OFFSET);
		if (waker.children_asleep == 0) break;
	}

	printk("Waker = %x\n", waker.val);

	// Zero out CTLR reg
	__gicr_write32(this_cpu, GICR_CTLR_OFFSET, 0x0);

	// disable all SGIs and PPIs
	for (i = 0; i < 3; i++) {
		__gicr_write32(this_cpu, GICR_ICENABLER_OFFSET(i), 0xffffffff);
	}

	// clear all pending irqs
	for (i = 1; i < 3; i++) {
		__gicr_write32(this_cpu, GICR_ICPENDR_OFFSET(i),   0xffffffff);
		__gicr_write32(this_cpu, GICR_ICACTIVER_OFFSET(i), 0xffffffff);
	}

	// set all SGIs and PPIs to lowest priority
	for (i = 0; i < 24; i++) {
		__gicr_write32(this_cpu, GICR_IPRIORITYR_OFFSET(i), 0xffffffff);
	}

	// set all SGIs and PPIs to edge triggered
	for (i = 0; i < 2; i++) {
		__gicr_write32(this_cpu, GICR_ICFGR_OFFSET(i), 0x55555555);
	}

	for (i = 2; i < 6; i++) {
		__gicr_write32(this_cpu, GICR_ICFGR_OFFSET(i), 0xffffffff);
	}


	// Set all IRQs to Non-secure Group 1
	for (i = 0; i < 3; i++) {
		__gicr_write32(this_cpu, GICR_IGRPMODR_OFFSET(i), 0x00000000);
		__gicr_write32(this_cpu, GICR_IGROUPR_OFFSET(i),  0xffffffff);
	}

}


static int 
__icc_enable()
{
	struct icc_sre icc_sre  = {mrs(ICC_SRE_EL1)};
	icc_sre.sre = 1;
	msr(ICC_SRE_EL1, icc_sre.val);
}

static int 
__icc_setup()
{

	struct icc_bpr     bpr0     = {mrs(ICC_BPR0_EL1)};
	struct icc_bpr     bpr1     = {mrs(ICC_BPR1_EL1)};
	struct icc_ctlr    icc_ctlr = {mrs(ICC_CTLR_EL1)};
	struct icc_igrpen  igrpen0  = {mrs(ICC_IGRPEN0_EL1)};
	struct icc_igrpen  igrpen1  = {mrs(ICC_IGRPEN1_EL1)};



	/* JRL: Is this necessary ?? */
	if (icc_ctlr.rss == 1) {
		struct gicd_typer typer = {__gicd_read32(GICD_TYPER_OFFSET)};
		typer.rss = 1;
		__gicd_write32(GICD_TYPER_OFFSET, typer.val);
	}

	// clear ICC_CTLR
	msr(ICC_CTLR_EL1, 0x00000000);

	// default to lowest priority
	msr(ICC_PMR_EL1, 0xff);

	// Flatten IRQ grouping
	bpr0.binary_point = 0;
	bpr1.binary_point = 0;
	msr(ICC_BPR0_EL1, bpr0.val);
	msr(ICC_BPR1_EL1, bpr1.val);


	// clear group 0 irqs (I don't think we need to do this...)
	while (1) {
		struct icc_hppir hppir = {mrs(ICC_HPPIR0_EL1)};

		if (hppir.intid == GIC3_SPURIOUS_IRQ) {
			break;
		}
		
		printk("Clearing IRQ %d\n", hppir.intid);

		mrs(ICC_IAR0_EL1);
		msr(ICC_EOIR0_EL1, hppir.val);
	}

	// clear group 1 irqs
	while (1) {
		struct icc_hppir hppir = {mrs(ICC_HPPIR1_EL1)};

		if (hppir.intid == GIC3_SPURIOUS_IRQ) {
			break;
		}
		
		printk("Clearing IRQ %d\n", hppir.intid);

		mrs(ICC_IAR1_EL1);
		msr(ICC_EOIR1_EL1, hppir.val);
	}



	igrpen0.enable = 1;
	igrpen1.enable = 1;
	msr(ICC_IGRPEN1_EL1, igrpen1.val);
	msr(ICC_IGRPEN0_EL1, igrpen0.val);

}


static void
__gic3_core_init( void )
{
	printk("GIC3 Core init\n");

	__gicr_setup();
	__icc_enable();
	__icc_setup();
}

static struct irqchip gic3_chip = {
	.name               = "GIC3",
	.dt_node            = NULL,
	.core_init          = __gic3_core_init,
	.enable_irq         = __gic3_enable_irq,
	.disable_irq        = __gic3_disable_irq,
	.do_eoi             = __gic3_do_eoi,
	.ack_irq            = __gic3_ack_irq,
	.send_ipi	    = __gic3_send_ipi,
	.parse_devtree_irqs = __gic3_parse_irqs,
	.dump_state         = __gic3_dump_state, 
	.print_pending_irqs = __gic3_print_pending_irqs
};



void 
gic3_global_init(struct device_node * dt_node)
{
	u32    regs[8] = {[0 ... 7] = 0};
	size_t reg_cnt = 0; 
	int    ret     = 0;
	
	if ( (of_n_addr_cells(dt_node) != 2) ||
	     (of_n_size_cells(dt_node) != 2) ) {
		panic("Only 64 bit reg values are supported\n");
	}

	ret = of_property_read_u32_array(dt_node, "reg", (u32 *)regs, 8);

	if (ret != 0) {
		panic("Could not read GIC registers from device tree (ret=%d)\n", ret);
	}


	gic.gicd_phys_start = (regs[0] << 32) | regs[1]; 
	gic.gicd_phys_size  = (regs[2] << 32) | regs[3]; 
	gic.gicr_phys_start = (regs[4] << 32) | regs[5]; 
	gic.gicr_phys_size  = (regs[6] << 32) | regs[7]; 

	printk("\tGICD at: %p [%d bytes]\n", gic.gicd_phys_start, gic.gicd_phys_size);
	printk("\tGICR at: %p [%d bytes]\n", gic.gicr_phys_start, gic.gicr_phys_size);

	gic.gicd_virt_start = ioremap(gic.gicd_phys_start, gic.gicd_phys_size);
	gic.gicr_virt_start = ioremap(gic.gicr_phys_start, gic.gicr_phys_size);


	printk("\tGICD Virt Addr: %p\n", gic.gicd_virt_start);
	printk("\tGICR Virt Addr: %p\n", gic.gicr_virt_start);

	__gicd_setup();

	gic3_chip.dt_node = dt_node;
	register_irqchip(&gic3_chip);
}

#include <dt-bindings/interrupt-controller/arm-gic.h>
static int
__gic3_parse_irqs(struct device_node *  dt_node, 
	          uint32_t              num_irqs, 
	          struct irq_def     *  irqs)
{
	const __be32 * ip;
	uint32_t       irq_cells = 0;

	int i   = 0;
	int ret = 0;

	ip = of_get_property(gic3_chip.dt_node, "#interrupt-cells", NULL);

	if (!ip) {
		printk("Could not find #interrupt_cells property\n");
		goto err;
	}

	irq_cells = be32_to_cpup(ip);

	if (irq_cells != 3) {
		printk("Interrupt Cell size of (%d) is not supported\n", irq_cells);
		goto err;
	}
	

	for (i = 0; i < num_irqs; i++) {
		uint32_t type   = 0;
		uint32_t vector = 0;
		uint32_t mode   = 0;
		
		ret |= of_property_read_u32_index(dt_node, "interrupts", &type,   (i * 3));
		ret |= of_property_read_u32_index(dt_node, "interrupts", &vector, (i * 3) + 1);
		ret |= of_property_read_u32_index(dt_node, "interrupts", &mode,   (i * 3) + 2);

		if (ret != 0) {
			printk("Failed to fetch interrupt cell\n");
			goto err;
		}

		if (mode & IRQ_TYPE_EDGE_BOTH) {
			irqs[i].mode = IRQ_EDGE_TRIGGERED;
		} else {
			irqs[i].mode = IRQ_LEVEL_TRIGGERED; 
		}

		if (type == GIC_PPI) {
			irqs[i].vector = vector + PPI_VECTOR_START;
		} else if (type == GIC_SPI) {
			irqs[i].vector = vector + SPI_VECTOR_START;
		} else {
			panic("Invalid IRQ Type\n");
		}
	}

	return 0;

err:
	return -1;	
}