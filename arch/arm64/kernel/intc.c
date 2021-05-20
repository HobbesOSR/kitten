/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>


#include <arch/msr.h>
#include <arch/intc.h>

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