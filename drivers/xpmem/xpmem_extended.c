/*
 * XPMEM extensions for multiple domain support.
 *
 * This file provides support for mapping remote process memory in Kitten
 * address spaces. The current approach is to carve aspace out dynamically with
 * aspace_{add/del}_region in xpmem_map_pfn_range. This approach should allow
 * smartmap to still function as normal, including for xpmem communication
 * between Kitten processes.
 *
 * This file also implements the handling of remote xpmem requests
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 */

#include <lwk/aspace.h>
#include <lwk/list.h>
#include "hashtable.h"

#include <xpmem.h>
#include <xpmem_extended.h>

u32 extend_enabled = 0;
struct xpmem_extended_ops * xpmem_extended_ops = NULL;

struct hashtable * attach_htable = NULL;

struct xpmem_remote_attach_struct {
    vaddr_t vaddr;
    unsigned long size;
    struct list_head node;
};

static u32 xpmem_hash_fn(uintptr_t key) {
    return pisces_hash_long(key, 32);
}

static int xpmem_eq_fn(uintptr_t key1, uintptr_t key2) {
    return (key1 == key2);
}


/* Handle remote requests */

int xpmem_get_remote(struct xpmem_cmd_get_ex * get_ex) {
    get_ex->apid = get_ex->segid;
    return 0;
}

int xpmem_release_remote(struct xpmem_cmd_release_ex * release_ex) {
    return 0;
}

int xpmem_attach_remote(struct xpmem_cmd_attach_ex * attach_ex) {
    vaddr_t seg_vaddr;
    u64 num_pfns, i;
    u64 * pfns;
    xpmem_apid_t local_apid;

    xpmem_apid_t apid = attach_ex->apid;
    off_t offset = attach_ex->off;
    size_t size = attach_ex->size;

    if (apid <= 0) {
	return -EINVAL;
    }

    if (offset & (PAGE_SIZE - 1)) {
	return -EINVAL;
    }

    if (size & (PAGE_SIZE - 1)) { 
	size += (PAGE_SIZE  - (size & (PAGE_SIZE - 1)));
    }

    local_apid = xpmem_get_local_apid(apid);
    if (local_apid <= 0) {
	printk(KERN_ERR "XPMEM: no mapping for remote apid %lli\n", apid);
	return -EINVAL;
    }

    seg_vaddr = (vaddr_t)(local_apid + offset);
    seg_vaddr -= 0x8000000000;

    num_pfns = size / PAGE_SIZE;
    pfns = kmem_alloc(sizeof(u64) * num_pfns);
    if (!pfns) {
	printk(KERN_ERR "XPMEM: out of memory\n");
	return -ENOMEM;
    }

    /* NOTE: This will almost certainly fail without SMARTMAP enabled, as it is
     * likely to only be invoked by the PCT */
    for (i = 0; i < num_pfns; i++) {
	paddr_t paddr;
	if (aspace_virt_to_phys(current->aspace->id, seg_vaddr, &paddr)) {
	    printk(KERN_ERR "XPMEM: Cannot get PFN for vaddr %p\n", (void *)seg_vaddr);
	    kfree(pfns);
	    return -EFAULT;
	}

	pfns[i] = paddr >> PAGE_SHIFT;
    }

    attach_ex->num_pfns = num_pfns;
    attach_ex->pfns = pfns;

    return 0;
}

int xpmem_detach_remote(struct xpmem_cmd_detach_ex * detach_ex) {
    return 0;
}


unsigned long
xpmem_map_pfn_range(u64 * pfns, u64 num_pfns)
{
    u64 i = 0;
    unsigned long size = 0;
    vaddr_t addr = 0;
    vaddr_t attach_addr = 0;
    int status = 0;
    struct xpmem_remote_attach_struct * remote;
    struct list_head * head;

    size = num_pfns * PAGE_SIZE;

    if (aspace_find_hole(current->aspace->id, 0, size, PAGE_SIZE, &attach_addr)) {
	printk(KERN_ERR "XPMEM: aspace_find_hole failed\n");
	return -ENOMEM;
    }

    if (aspace_add_region(current->aspace->id, attach_addr, size, VM_READ | VM_WRITE | VM_USER,
		PAGE_SIZE, "xpmem")) {
	printk(KERN_ERR "XPMEM: aspace_add_region failed\n");
	return -ENOMEM;
    }

    for (i = 0; i < num_pfns; i++) {
	addr = attach_addr + (i * PAGE_SIZE);

	printk("XPMEM: mapping vaddr = %p, pfn = %llu, (paddr = %p)\n",
		(void *)addr,
		pfns[i],
		(void *)(pfns[i] << PAGE_SHIFT)
	);

	status = aspace_map_pmem(current->aspace->id, (pfns[i] << PAGE_SHIFT), addr, PAGE_SIZE); 
	if (status) {
	    printk(KERN_ERR "XPMEM: aspace_map_pmem failed\n");
	    aspace_del_region(current->aspace->id, attach_addr, size);
	    return status;
	}
    }

    if (!attach_htable) {
	attach_htable = pisces_create_htable(0, xpmem_hash_fn, xpmem_eq_fn);
	if (!attach_htable) {
	    printk(KERN_ERR "XPMEM: cannot create attachment hashtable\n");
	    aspace_del_region(current->aspace->id, attach_addr, size);
	    return -ENOMEM;
	}
    }

    remote = kmem_alloc(sizeof(struct xpmem_remote_attach_struct));
    if (!remote) {
	printk(KERN_ERR "XPMEM: out of memory\n");
	aspace_del_region(current->aspace->id, attach_addr, size);
	return -ENOMEM;
    }

    remote->vaddr = attach_addr;
    remote->size = size;

    head = (struct list_head *)pisces_htable_search(attach_htable, current->aspace->id);
    if (!head) {
	head = kmem_alloc(sizeof(struct list_head));
	if (!head) {
	    printk(KERN_ERR "XPMEM: out of memory\n");
	    aspace_del_region(current->aspace->id, attach_addr, size);
	    return -ENOMEM;
	}

	INIT_LIST_HEAD(head);
	if (!pisces_htable_insert(attach_htable, (uintptr_t)current->aspace->id, (uintptr_t)head)) {
	    printk(KERN_ERR "XPMEM: list operation failed\n");
	    aspace_del_region(current->aspace->id, attach_addr, size);
	    return -1;
	}
    }
    list_add_tail(&(remote->node), head);

    return attach_addr;
}

void
xpmem_detach_vaddr(u64 vaddr)
{
    struct list_head * head = NULL;

    if (!attach_htable) {
	return;
    }

    head = (struct list_head *)pisces_htable_search(attach_htable, current->aspace->id);
    if (!head) {
	printk(KERN_ERR "XPMEM: LEAKING VIRTUAL ADDRESS SPACE\n");
    } else {
	struct xpmem_remote_attach_struct * remote = NULL, * next = NULL;
	list_for_each_entry_safe(remote, next, head, node) {
	    if (remote->vaddr == vaddr) {
		aspace_del_region(current->aspace->id, remote->vaddr, remote->size);
		list_del(&(remote->node));
		kmem_free(remote);
		break;
	    }
	}

	if (list_empty(head)) {
	    pisces_htable_remove(attach_htable, (uintptr_t)current->aspace->id, 0);
	    kmem_free(head);
	}
    }
}
