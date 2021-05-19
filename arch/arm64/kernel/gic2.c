/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>


#include <arch/intc.h>
#include <arch/msr.h>
#include <arch/gicdef.h>
#include <arch/of.h>
#include <arch/io.h>

#define GIC2_SPURIOUS_IRQ 1023

struct gic2 {
	vaddr_t gicd_virt_start;
	paddr_t gicd_phys_start;
	paddr_t gicd_phys_size;

	vaddr_t gicc_virt_start;
	paddr_t gicc_phys_start;
	paddr_t gicc_phys_size;
};


static struct gic2 gic;




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
__gicc_read32(uintptr_t offset)
{
	return readl(gic.gicc_virt_start + offset);
}

static inline void
__gicc_write32(uintptr_t offset, 
	       uint32_t  value)
{
	writel(value, gic.gicc_virt_start + offset);
}


static int
__gicd_setup()
{
	struct gicd_ctlr_ns ctlr = {0};

	// disable GICD
	__gicd_write32(GICD_CTLR_OFFSET, ctlr.val); 

	// disable all irqs (only 64 of them apparently)
	__gicd_write32(GICD_ICENABLER_OFFSET(0), 0xffffffff);
	__gicd_write32(GICD_ICENABLER_OFFSET(1), 0xffffffff);

	// clear all pending irqs
	__gicd_write32(GICD_ICPENDR_OFFSET(0), 0xffffffff);
	__gicd_write32(GICD_ICPENDR_OFFSET(1), 0xffffffff);

	// set all irqs to lowest priority
	__gicd_write32(GICD_IPRIORITYR_OFFSET(0), 0xffffffff);
	__gicd_write32(GICD_IPRIORITYR_OFFSET(1), 0xffffffff);

	// send all SPIs to cpu 0
	__gicd_write32(GICD_ITARGETSR_OFFSET(8),  0x01010101);
	__gicd_write32(GICD_ITARGETSR_OFFSET(9),  0x01010101);
	__gicd_write32(GICD_ITARGETSR_OFFSET(10), 0x01010101);
	__gicd_write32(GICD_ITARGETSR_OFFSET(11), 0x01010101);
	__gicd_write32(GICD_ITARGETSR_OFFSET(12), 0x01010101);
	__gicd_write32(GICD_ITARGETSR_OFFSET(13), 0x01010101);
	__gicd_write32(GICD_ITARGETSR_OFFSET(14), 0x01010101);
	__gicd_write32(GICD_ITARGETSR_OFFSET(15), 0x01010101);

	// set all irqs to level triggered
	__gicd_write32(GICD_ICFGR_OFFSET(1), 0x00000000);
	__gicd_write32(GICD_ICFGR_OFFSET(2), 0x00000000);
	__gicd_write32(GICD_ICFGR_OFFSET(3), 0x00000000);

	ctlr.enable_grp_1 = 1;
	// enable GICD
	__gicd_write32(GICD_CTLR_OFFSET, ctlr.val);

}


static int
__gicc_setup()
{
	struct gicc_ctlr ctlr = {0};

	// disable GICC
	__gicc_write32(GICC_CTLR_OFFSET, ctlr.val);

	// default to lowest priority
	__gicc_write32(GICC_PMR_OFFSET, 0xff);

	// collect all irqs into single group
	__gicc_write32(GICC_BPR_OFFSET, 0);

	// clear outstanding irqs
	while (1) {
		struct gicc_iar irq = {0};

		irq.val = __gicc_read32(GICC_IAR_OFFSET);

		if (irq.intid == GIC2_SPURIOUS_IRQ) {
			break;
		}

		printk("Clearing IRQ %d\n", irq.intid);

		__gicc_write32(GICC_EOIR_OFFSET, irq.val);
	}

	ctlr.enable_grp1 = 1;
	__gicc_write32(GICC_CTLR_OFFSET, ctlr.val);
}



void 
gic2_global_init(struct device_node * dt_node)
{
	u32    regs[8] = {0};
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
	gic.gicc_phys_start = (regs[4] << 32) | regs[5]; 
	gic.gicc_phys_size  = (regs[6] << 32) | regs[7]; 

	printk("\tGICD at: %p [%d bytes]\n", gic.gicd_phys_start, gic.gicd_phys_size);
	printk("\tGICC at: %p [%d bytes]\n", gic.gicc_phys_start, gic.gicc_phys_size);

	gic.gicd_virt_start = ioremap(gic.gicd_phys_start, gic.gicd_phys_size);
	gic.gicc_virt_start = ioremap(gic.gicc_phys_start, gic.gicc_phys_size);


	printk("\tGICD Virt Addr: %p\n", gic.gicd_virt_start);
	printk("\tGICR Virt Addr: %p\n", gic.gicc_virt_start);


	__gicd_setup();
	__gicc_setup();

}

void
gic2_enable_irq(uint32_t           irq_num,
		irq_trigger_mode_t trigger_mode)
{
	struct gicd_icfgr      icfgr     = {0};
	struct gicd_ipriorityr ipriority = {0};
	struct gicd_itargetsr  itargets  = {0};

	uint32_t icfgr_index        = (irq_num / 16);
	uint32_t icfgr_shift        = (irq_num % 16) * 2;
	uint32_t icfgr_mode        = 0;

	uint32_t priority_maj_index = irq_num / 4;
	uint32_t priority_min_index = irq_num % 4;
	uint8_t  priority           = 0;

	uint32_t target_maj_index   = irq_num / 4;
	uint32_t target_min_index   = irq_num % 4;
	uint8_t  target             = 0;

	uint32_t pending_index      = irq_num / 32;
	uint32_t pending_shift      = irq_num % 32;

	uint32_t enable_index       = irq_num / 32;
	uint32_t enable_shift       = irq_num % 32;


	if (trigger_mode == IRQ_LEVEL_TRIGGERED) {
		icfgr_mode = 0x0;
	} else if (trigger_mode == IRQ_EDGE_TRIGGERED) {
		icfgr_mode = 0x2;
	}

	// set trigger mode 
	icfgr.val = __gicd_read32(GICD_ICFGR_OFFSET(icfgr_index));
	icfgr.bits &= ~(0x3        << icfgr_shift);
	icfgr.bits |=  (icfgr_mode << icfgr_shift);
	__gicd_write32(GICD_ICFGR_OFFSET(icfgr_index), icfgr.val);

	// set priority to highest
	ipriority.val = __gicd_read32(GICD_IPRIORITYR_OFFSET(priority_maj_index));
	ipriority.bits[priority_min_index] = priority;
	__gicd_write32(GICD_IPRIORITYR_OFFSET(priority_maj_index), ipriority.val);

	// set target CPU to CPU 0
	itargets.val = __gicd_read32(GICD_ITARGETSR_OFFSET(target_maj_index));
	itargets.targets_offset[target_min_index] = 1;
	__gicd_write32(GICD_ITARGETSR_OFFSET(target_maj_index), itargets.val);

	// clear pending
	__gicd_write32(GICD_ICPENDR_OFFSET(pending_index), 0x1U << pending_shift);

	// enable irq
	__gicd_write32(GICD_ISENABLER_OFFSET(enable_index), 0x1U << enable_shift);

}

void 
gic2_probe()
{

	printk("GIC2 Probe...\n");
}