/* 
 * 2021, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>


#include <arch/msr.h>
#include <arch/gicdef.h>
#include <arch/fixmap.h>
#include <arch/of.h>

struct gic {
	vaddr_t gicd_virt_start;
	paddr_t gicd_phys_start;
	paddr_t gicd_phys_size;



	vaddr_t gicr_virt_start;
	paddr_t gicr_phys_start;
	paddr_t gicr_phys_size;
};


struct gic gic;

/**
 * Resource entry for the local APIC memory mapping.
 */
static struct resource gic_resource = {
	.name  = "GIC",
	.flags = IORESOURCE_MEM | IORESOURCE_BUSY,
	/* .start and .end filled in later based on detected address */
};




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




void __init
gic_global_init(void)
{	

	struct icc_sre icc_sre = {mrs(ICC_SRE_EL1)};

	struct device_node * dt_node = NULL;

	u32    regs[8] = {0};
	size_t reg_cnt = 0; 
	int    ret     = 0;


	printk("GIC init:\n");

	dt_node = of_find_node_with_property(NULL, "interrupt-controller");

	if (!of_device_is_compatible(dt_node, "arm,gic-v3")) {
		panic("Only GICv3 is currently supported\n");
	}


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


	printk("\tICC_SRE [sre=%d, dfb=%d, dib=%d]\n", icc_sre.sre, icc_sre.dfb, icc_sre.dib);
	printk("\tUsing system register interface\n");

	return;

}