#include <arch/pisces/pisces_pagemap.h>
#include <arch/pisces/pisces_portals.h>
#include <lwk/aspace.h>
#include <lwk/pfn.h>

static int 
map_pfn_range(unsigned long addr, unsigned long pfn, unsigned long size)
{
    paddr_t phys_addr = PFN_PHYS(pfn);
    vaddr_t virt_addr = addr;
    int status;
    
    status = __aspace_map_pmem(current->aspace, phys_addr, virt_addr, size);
    if (status)
        printk(KERN_ERR "__aspace_map_pmem() failed (status=%d)\n", status);

    return status;
}


unsigned long
pisces_map_xpmem_pfn_range(
    struct xpmem_pfn * pfns,
    u64 num_pfns
)
{
    struct aspace * aspace = current->aspace;
    unsigned long addr = 0;
    unsigned long attach_addr = 0;
    unsigned long size = 0;
    u64 i = 0;
    int status = 0;

    size = num_pfns * PAGE_SIZE;

    spin_lock(&(aspace->lock));
    status = __aspace_find_hole(aspace, 0, size, PAGE_SIZE, &attach_addr);
    if (status) {
        spin_unlock(&(aspace->lock));
        printk(KERN_ERR "Cannot map xpmem pfn range - out of memory\n");
        return -ENOMEM;
    }

    status = __aspace_add_region(aspace, attach_addr, size, VM_READ | VM_WRITE | VM_USER,
            PAGE_SIZE, "pisces_ppe");
    if (status) {
        spin_unlock(&(aspace->lock));
        printk(KERN_ERR "Cannot map xpmem pfn range - cannot add memory to aspace\n");
        return -ENOMEM;
    }
    spin_unlock(&(aspace->lock));

    for (i = 0; i < num_pfns; i++) {
        addr = attach_addr + (i * PAGE_SIZE);

        printk("Mapping vaddr = %p, pfn = %llu, (paddr = %p)\n", 
            (void *)addr,
            pfns[i].pfn,
            (void *)(pfns[i].pfn << PAGE_SHIFT)
        );

        status = map_pfn_range(addr, pfns[i].pfn, PAGE_SIZE);
        if (status) {
            return -ENOMEM;
        }
    }   

    return attach_addr;
}
