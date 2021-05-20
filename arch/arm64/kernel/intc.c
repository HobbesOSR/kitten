/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>


#include <arch/msr.h>
#include <arch/intc.h>
#include <arch/irq_vectors.h>

#include <arch/of.h>
#include <arch/io.h>



struct irqchip * irq_controller = NULL;

int __init 
intc_local_init(void)
{
	irq_controller->core_init(irq_controller->dt_node);


}


extern int gic3_global_init(struct device_node * dt_node);
extern int gic2_global_init(struct device_node * dt_node);

static const struct of_device_id intr_ctrlr_of_match[]  = {
	{ .compatible = "arm,gic-v3",		.data = gic3_global_init},
	{ .compatible = "arm,cortex-a15-gic",	.data = gic2_global_init},
	{},
};



int 
register_irqchip(struct irqchip * chip)
{
	if (irq_controller) {
		panic("Failed to register irq controller. Already registered.\n");
	}


	printk("Registering IRQ Controller [%s]\n", chip->name);
	irq_controller = chip;	

	return 0;
}


int __init
intc_global_init(void)
{	

	struct device_node  * dt_node    = NULL;
	struct of_device_id * matched_np = NULL;
	irqchip_init_fn init_fn;

	dt_node = of_find_matching_node_and_match(NULL, intr_ctrlr_of_match, &matched_np);

	if (!dt_node || !of_device_is_available(dt_node)) {
		panic("Could not find interrupt controller\n");
	//	return -ENODEV;
	}

	init_fn = (irqchip_init_fn)(matched_np->data);

	return init_fn(dt_node);
}

extern void gic3_probe(void);
extern void gic2_probe(void);

void probe_pending_irqs(void) {
	irq_controller->print_pending_irqs();
}

void
enable_irq(uint32_t           irq_num, 
	   irq_trigger_mode_t trigger_mode)
{
	irq_controller->enable_irq(irq_num, trigger_mode);
}


#include <dt-bindings/interrupt-controller/arm-gic.h>
int 
parse_fdt_irqs(struct device_node *  dt_node, 
	       uint32_t              num_irqs, 
	       struct irq_def     *  irqs)
{
	const __be32 * ip;
	uint32_t       irq_cells = 0;

	int i   = 0;
	int ret = 0;

	ip = of_get_property(irq_controller->dt_node, "#interrupt-cells", NULL);

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