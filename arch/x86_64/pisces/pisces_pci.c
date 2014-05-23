/*
 * Pisces native MSI support
 * (c) Jiannan Ouyang, 2013
 */
#include <lwk/kernel.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/pci/pci.h>
#include <lwk/pci/msidef.h>
#include <lwk/resource.h>

//#include <arch/pisces/pisces_pci.h>
#include <arch/io.h>
#include <arch/pgtable.h>

#define DEBUG

#ifdef DEBUG
#define debug(fmt) printk(fmt)
#else
#define debug(fmt)
#endif

extern struct cpuinfo cpu_info[NR_CPUS];

/* for now, route MSI to the core calls msi_enable,
 * do not support MSI per vector MASK
 */
int pisces_pci_msi_enable(pci_dev_t *dev, u8 vector) 
{
    debug("Pisces: Enabling MSI\n");

    pci_msi_setup(dev, vector);

    debug("MSI enabled\n");

    return 0;
}

int pisces_pci_msi_disable(pci_dev_t *dev) 
{
    debug("Disabling MSI\n");

    pci_msi_disable(dev);
    pci_intx_enable(dev);

    return 0;
}

int pisces_pci_msix_enable(pci_dev_t *dev, struct msix_entry * entries, u32 num_entries)
{
    int ret = 0;

    ret = pci_msix_setup(
            dev,
            entries,
            num_entries);
    if (ret) {
        printk("MSI-X enable error\n");
        return ret;
    }

    return ret;
}


int pisces_pci_msix_disable(pci_dev_t *dev)
{
    pci_msix_disable(dev);
    pci_intx_enable(dev);
    return 0;
}

/*
vaddr_t
_pisces_map_region(paddr_t pmem, size_t size, vmflags_t flags) 
{
    vaddr_t vaddr;
    paddr_t paddr;
    int ret;

    // setup page tables for ahci mmio
    for (paddr = pmem; paddr < pmem + size; paddr += VM_PAGE_4KB) {
        if (arch_aspace_virt_to_phys(&bootstrap_aspace, (vaddr_t)__va(paddr), NULL) == -ENOENT) {
            ret = arch_aspace_map_page(
                    &bootstrap_aspace,
                    (vaddr_t)__va(paddr),
                    paddr,
                    flags,
                    VM_PAGE_4KB
                    );

            if (ret) {
                printk(KERN_ERR "Error: Could not map kernel memory for MMIO paddr=0x%016lx.\n", paddr);
                break;
            }
        }
    }

    if (ret) {
        printk(KERN_ERR "Error: _pisces_map_region failed for %s at %lx, size %lu, flags %lx\n", pmem, size, flags);
        return (vaddr_t) NULL;
    }

    return vaddr;
}


vaddr_t pisces_map_region(paddr_t pmem, size_t size) {
    return _pisces_map_region(pmem, size, _KERNPG_TABLE);
}

vaddr_t pisces_map_region_nocache(paddr_t pmem, size_t size)
{
    return _pisces_map_region(pmem, size, _KERNPG_TABLE | _PAGE_PCD);
}
*/
