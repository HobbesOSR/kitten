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


typedef enum { GIC3,
	       GIC2,
	       INVALID_INTC } intc_type_t;


intc_type_t intc_type = INVALID_INTC;

void __init
gic_map(void)
{

	return;
#if 0
	gic_resource.start = 0;
	gic_resource.end   = 0;
	request_resource(&iomem_resource, &gic_resource);

	set_fixmap(FIX_GICD_BASE, gic_phys_addr);
#endif
}


void __init 
gic_local_init(void)
{



}


extern void gic3_global_init(struct device_node * dt_node);
extern void gic2_global_init(struct device_node * dt_node);



void __init
intc_global_init(void)
{	

	struct device_node * dt_node = NULL;


	printk("GIC init:\n");

	dt_node = of_find_node_with_property(NULL, "interrupt-controller");

	if (of_device_is_compatible(dt_node, "arm,gic-v3")) {
		intc_type = GIC3;
		gic3_global_init(dt_node);
	} else if (of_device_is_compatible(dt_node, "arm,cortex-a15-gic")) {
		intc_type = GIC2;
		gic2_global_init(dt_node);
	} else {
		panic("Unsupported Interrupt controller\n");
	}

	return;

}

extern void gic3_probe(void);
extern void gic2_probe(void);

void probe_pending_irqs(void) {
	if (intc_type == GIC3) {
		gic3_probe();
	} else if (intc_type == GIC2) {
		gic2_probe();
	} else {
		panic("Invalid INTC type\n");
	}
}

void
enable_irq(uint32_t           irq_num, 
	   irq_trigger_mode_t trigger_mode)
{
	if (intc_type == GIC3) {
		gic3_enable_irq(irq_num, trigger_mode);
	} else if (intc_type == GIC2) {
		gic2_enable_irq(irq_num, trigger_mode);
	} else {
		panic("Invalid INTC type\n");
	}
}