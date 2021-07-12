/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>
#include <lwk/cpuinfo.h>

#include <arch/irqchip.h>
#include <arch/msr.h>
#include <arch/of.h>
#include <arch/io.h>
#include <arch/irq_vectors.h>
#include <arch/topology.h>

#include "bcm2835.h"
#include "bcm2836.h"


/* We'll layout the vector space this way:
 * BCM2836 IRQ vectors       [0  - 31]
 * BCM2835 BASIC IRQ vectors [32 - 51]
 * BCM2835 IRQ vectors       [52 - 115]
 */
 
 
 /* BCM 2836 Vectors 
  * 9 PMU
  * 8 BCM 2835
  * 3 CNTVIRQ interrupt
  * 2 CNTHPIRQ interrupt
  * 1 CNTPNSIRQ interrupt
  * 0 CNTPSIRQ interrupt (Physical Timer -1)
  */

/* BCM 2835 Vectors 
 * ??????
 */

struct bcm2836_state {
	vaddr_t bcm2836_virt_start;
	paddr_t bcm2836_phys_start;
	paddr_t bcm2836_phys_size;


	vaddr_t bcm2835_virt_start;
	paddr_t bcm2835_phys_start;
	paddr_t bcm2835_phys_size;

	
	struct dt_node * bcm2835_dt_node;
};

static struct bcm2836_state bcm;


static inline uint32_t 
__bcm2836_read32(uintptr_t offset)
{
	return readl(bcm.bcm2836_virt_start + offset);
}

static inline void
__bcm2836_write32(uintptr_t offset, 
	     uint32_t  value)
{
	writel(value, bcm.bcm2836_virt_start + offset);
}



static inline uint32_t 
__bcm2835_read32(uintptr_t offset)
{
	return readl(bcm.bcm2835_virt_start + offset);
}

static inline void
__bcm2835_write32(uintptr_t offset, 
	    	 uint32_t  value)
{
	writel(value, bcm.bcm2835_virt_start + offset);
}



static int
__bcm2836_parse_irqs(struct device_node *  dt_node, 
		     uint32_t              num_irqs, 
		     struct irq_def     *  irqs);


static void 
__bcm2836_print_pending_irqs(void)
{
	printk("BCM TODO: Print Pending Interrupts\n");
}

extern void bcm2835_dump_state(void);

static void 
__bcm2836_dump_state(void)
{
	/* 2836 State */
	{
		uint32_t ctrl_reg   = __bcm2836_read32(BCM2836_CTRL_OFFSET);
		uint32_t timer_ctrl = __bcm2836_read32(BCM2836_TIMER_CTRL_OFFSET);
		uint32_t mbox_irqs  = __bcm2836_read32(BCM2836_MAILBOX_IRQS_OFFSET(this_cpu));
		uint32_t irq_src    = __bcm2836_read32(BCM2836_IRQ_SOURCE_OFFSET(this_cpu));


		printk("ctrl_reg     : %x\n", ctrl_reg);
		printk("timer_ctrl   : %x\n", timer_ctrl);
		printk("IRQ Source   : %x\n", irq_src);
		printk("MailBox IRQs : %x\n", mbox_irqs);
	}


	/* 2835 State */
	{
		uint32_t pending_b = __bcm2835_read32(BCM2835_IRQ_BASIC_PENDING_OFFFSET);
		uint32_t pending_1 = __bcm2835_read32(BCM2835_IRQ_PENDING_OFFSET(0));
		uint32_t pending_2 = __bcm2835_read32(BCM2835_IRQ_PENDING_OFFSET(1));

		printk("Basic pending   : %x\n", pending_b);
		printk("Pending IRQs 1  : %x\n", pending_1);
		printk("Pending IRQs 2  : %x\n", pending_2);
	}

}





static void
__bcm2836_enable_irq(uint32_t           irq_num, 
		  irq_trigger_mode_t trigger_mode)
{
	// We actually enable these on the 2835
	uint32_t value = 0x1 << (irq_num % 32);
	uint32_t bank  = irq_num / 32;

	printk("Enabling IRQ %d\n", irq_num);

	if (irq_num < 32) {
		/* BCM 2836 */
		if (irq_num < 4) { 
			/* Timers */
			uint32_t timer_ctrl = __bcm2836_read32(BCM2836_TIMER_IRQS_OFFSET(this_cpu));
			timer_ctrl |= (0x1 << irq_num);
			__bcm2836_write32(BCM2836_TIMER_IRQS_OFFSET(this_cpu), timer_ctrl);
		} else if (irq_num < 8 ) { 
			/* Mailboxes */
			uint32_t mailbox_ctrl = __bcm2836_read32(BCM2836_MAILBOX_IRQS_OFFSET(this_cpu));
			mailbox_ctrl |= (0x1 << (irq_num - 4));
			__bcm2836_write32(BCM2836_MAILBOX_IRQS_OFFSET(this_cpu), mailbox_ctrl);
		} else if (irq_num == 9) {
			/* PMU */
			uint32_t pmu_ctrl = (0x1 << this_cpu);
			__bcm2836_write32(BCM2836_PMU_IRQS_SET_OFFSET, pmu_ctrl);	
		}
	} else if (irq_num < 40) {
		/* BCM 2835 Basic */
	
	} else {
		/* BCM 2835 Peripherals */
		__bcm2835_write32(BCM2835_ENABLE_IRQS_OFFSET(bank), value - 64);
	}


}

static void
__bcm2836_disable_irq(uint32_t vector)
{
	panic("Disabling IRQs not supported\n");
}

static struct arch_irq 
__bcm2836_ack_irq(void)
{
	struct arch_irq irq = {0, 0};


	/* Check 2835 State */
	if (0) {
		/* 2835 State */
		{
			uint32_t pending_b = __bcm2835_read32(BCM2835_IRQ_BASIC_PENDING_OFFFSET);
			uint32_t pending_1 = __bcm2835_read32(BCM2835_IRQ_PENDING_OFFSET(0));
			uint32_t pending_2 = __bcm2835_read32(BCM2835_IRQ_PENDING_OFFSET(1));

			printk("Basic pending   : %x\n", pending_b);
			printk("Pending IRQs 1  : %x\n", pending_1);
			printk("Pending IRQs 2  : %x\n", pending_2);
		}


	}

	/* Check 2836 State */
	{
		uint32_t irq_src = __bcm2836_read32(BCM2836_IRQ_SOURCE_OFFSET(this_cpu));
		uint32_t vector  = fls(irq_src);

		if (!irq_src) {
			// Nothing pending
			goto out; 
		}

		if ((vector >= 4) && (vector < 8)) {
			// IPI
			uint32_t ipi_vector = __bcm2836_read32(BCM2836_MAILBOX_CLR_OFFSET(this_cpu, vector - 4));
			irq.type   = ARCH_IRQ_IPI;
			irq.vector = ipi_vector;
			__bcm2836_write32(BCM2836_MAILBOX_CLR_OFFSET(this_cpu, vector - 4), 0xffffffff);

		} else {
			irq.type   = ARCH_IRQ_EXT;
			irq.vector = vector;
		}

	}

out:
	return irq;
}

static void
__bcm2836_do_eoi(struct arch_irq irq)
{
	// Every IRQ is level triggered, so no EOI needed
	// printk("BCM TODO: Do EOI\n");
}



static void
__bcm2836_send_ipi(int target_cpu, uint32_t vector)
{
	__bcm2836_write32(BCM2836_MAILBOX_SET_OFFSET(target_cpu, this_cpu), vector);
}

static void
__bcm2836_core_init( void )
{
	/* Enable Mailboxes */
	printk("BCM2836 Core init\n");
	__bcm2836_write32(BCM2836_MAILBOX_IRQS_OFFSET(this_cpu), 0x0000000f);

}

static struct irqchip bcm2836_chip = {
	.name               = "BCM-2836",
	.dt_node            = NULL,
	.core_init          = __bcm2836_core_init,
	.enable_irq         = __bcm2836_enable_irq,
	.disable_irq        = __bcm2836_disable_irq,
	.do_eoi             = __bcm2836_do_eoi,
	.ack_irq            = __bcm2836_ack_irq,
	.send_ipi           = __bcm2836_send_ipi,
	.parse_devtree_irqs = __bcm2836_parse_irqs,
	.dump_state         = __bcm2836_dump_state,
	.print_pending_irqs = __bcm2836_print_pending_irqs
};


extern void bcm2835_global_init(struct device_node * dt_node);

void 
bcm2836_global_init(struct device_node * dt_node)
{
	
	/* Initialize BCM2836 */
	{
		u32    regs[2] = {0, 0};
		size_t reg_cnt = 0; 
		int    ret     = 0;
		
		if ( (of_n_addr_cells(dt_node) != 1) ||
		     (of_n_size_cells(dt_node) != 1) ) {
			panic("Only 32 bit reg values are supported\n");
		}

		ret = of_property_read_u32_array(dt_node, "reg", (u32 *)regs, 2);

		if (ret != 0) {
			panic("Could not read BCM2836 registers from device tree (ret=%d)\n", ret);
		}


		bcm.bcm2836_phys_start = regs[0]; 
		bcm.bcm2836_phys_size  = regs[1]; 


		

		printk("\tBCM2836 at: %p [%d bytes]\n", bcm.bcm2836_phys_start, bcm.bcm2836_phys_size);

		bcm.bcm2836_virt_start = ioremap_nocache(bcm.bcm2836_phys_start, bcm.bcm2836_phys_size);
		printk("\tBCM2836 Virt Addr: %p\n", bcm.bcm2836_virt_start);
	}

	/* Initialize BCM2835 */
	{
		struct dt_node * bcm2835_node = NULL;
		u32    regs[2] = {0, 0};
		size_t reg_cnt = 0; 
		int    ret     = 0;
	
		bcm2835_node = of_find_compatible_node(NULL, NULL, "brcm,bcm2836-armctrl-ic");

		if (bcm2835_node == NULL) {
			panic("BCM2836 requires a complementary 2835 to be present\n");
		}


		
		if ( (of_n_addr_cells(bcm2835_node) != 1) ||
		     (of_n_size_cells(bcm2835_node) != 1) ) {
			panic("Only 32 bit reg values are supported\n");
		}

		ret = of_property_read_u32_array(bcm2835_node, "reg", (u32 *)regs, 2);

		if (ret != 0) {
			panic("Could not read BCM2835 registers from device tree (ret=%d)\n", ret);
		}


		bcm.bcm2835_phys_start = regs[0]; 
		bcm.bcm2835_phys_size  = regs[1]; 

		/* From the bcm2837 docs:
		 * (No, they don't officially exist, but if you look hard enough you might be able to find them)
		 * 
		 * Physical addresses range from 0x3F000000 to 0x3FFFFFFF for peripherals. The
		 * bus addresses for peripherals are set up to map onto the peripheral bus address range
		 * starting at 0x7E000000. Thus a peripheral advertised here at bus address 0x7Ennnnnn is
		 * available at physical address 0x3Fnnnnnn.
		 */

		// So...
		bcm.bcm2835_phys_start -= 0x3F000000;

		printk("\tBCM2835 at: %p [%d bytes]\n", bcm.bcm2835_phys_start, bcm.bcm2835_phys_size);
		bcm.bcm2835_virt_start = ioremap_nocache(bcm.bcm2835_phys_start, bcm.bcm2835_phys_size);

		printk("\tBCM2835 Virt Addr: %p\n", bcm.bcm2835_virt_start);

		bcm.bcm2835_dt_node = dt_node;

	}


	bcm2836_chip.dt_node = dt_node;
	irqchip_register(&bcm2836_chip);
}

/******
 ***  BCM 2836
 *    - #interrupt-cells : Should be 2.
 *     The first cell is the GPIO number.
 *     The second cell is used to specify flags:
 *     bits[3:0] trigger type and level flags:
 *     1 = low-to-high edge triggered.
 *     2 = high-to-low edge triggered.
 *     4 = active high level-sensitive.
 *     8 = active low level-sensitive.
 *     Valid combinations are 1, 2, 3, 4, 8.
 ***
 * 
 ***  BCM 2835
 * probably different....
 ***
 *****/

#include <dt-bindings/interrupt-controller/arm-gic.h>
static int
__bcm2836_parse_irqs(struct device_node *  dt_node, 
	             uint32_t              num_irqs, 
	             struct irq_def     *  irqs)
{
	const __be32 * ip;
	uint32_t       irq_cells = 0;

	int i   = 0;
	int ret = 0;

	/* TODO: Figure out the interrupt parent, and offset BCM2835 vectors by 32 */

	ip = of_get_property(bcm2836_chip.dt_node, "#interrupt-cells", NULL);

	if (!ip) {
		printk("Could not find #interrupt_cells property\n");
		goto err;
	}

	irq_cells = be32_to_cpup(ip);

	if (irq_cells != 2) {
		printk("Interrupt Cell size of (%d) is not supported\n", irq_cells);
		goto err;
	}
	
	for (i = 0; i < num_irqs; i++) {
		uint32_t vector = 0;
		uint32_t mode   = 0;
		
		ret |= of_property_read_u32_index(dt_node, "interrupts", &vector, (i * 2) + 0);
		ret |= of_property_read_u32_index(dt_node, "interrupts", &mode,   (i * 2) + 1);

		if (ret != 0) {
			printk("Failed to fetch interrupt cell %d\n", i);
			goto err;
		}

		if (mode & IRQ_TYPE_EDGE_BOTH) {
			irqs[i].mode = IRQ_EDGE_TRIGGERED;
		} else {
			irqs[i].mode = IRQ_LEVEL_TRIGGERED; 
		}

		irqs[i].vector = vector;
	}


	return 0;

err:
	return -1;
}