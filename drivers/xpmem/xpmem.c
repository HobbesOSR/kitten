/*
 * XPMEM extensions for multiple domain support
 *
 * Thie file implements core driver support for regular XPMEM commands. XPMEM
 * registrations (invocations of xpmem_make()) must be sent to the name service,
 * as do gets/attachments of remote exported regions.
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 */
 
#include <lwk/types.h>
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <lwk/aspace.h>
#include <arch/uaccess.h>
#include <arch/mutex.h>
#include <arch/atomic.h>

#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_extended.h>

#include <arch/pisces/pisces_xpmem.h>

#include <xpmem_private.h>
#include <xpmem_hashtable.h>


struct xpmem_partition * xpmem_my_part = NULL;


/* Segids are creates in a fashion that conflicts with Kitten's SMARTMAP-based approach.
 * So, we keep a hashtable where the segid maps to the source process' SMARTMAP rank and
 * address info so that we can convert segids to SMARTMAP-able apids during XPMEM attach
 * operations
 */
static struct xpmem_hashtable * segid_map     = NULL;

/* We also need to maintain an apid map for apids created remotely and assigned to Kitten.
 * The reason for this is that remote releases and attachments require both a segid and an
 * apid, but users are not required to pass the segid for these operations. So, we map
 * apids to remote segids here
 */
static struct xpmem_hashtable * apid_map      = NULL;


struct xpmem_smartmap_info {
    pid_t  pid;
    size_t size;
    u64    vaddr;
};


static u32 
segid_hash_fn(uintptr_t key)
{
    return hash_long(key, 32);
}

static int 
segid_eq_fn(uintptr_t key1, 
	    uintptr_t key2)
{
    return (key1 == key2);
}

static unsigned long
make_xpmem_addr(pid_t  src, 
		void * vaddr) 
{
    unsigned long slot;

    if (src == INIT_ASPACE_ID)
	slot = 0;
    else
	slot = src - 0x1000 + 1;

    return (((slot + 1) << SMARTMAP_SHIFT) | ((unsigned long) vaddr));
}

static int
xpmem_make(void		 * vaddr, 
	   size_t	   size, 
	   int		   permit_type, 
	   void		 * permit_value, 
	   int             flags,
	   xpmem_segid_t   request,
	   xpmem_segid_t * segid_p,
	   int           * fd)
{
    xpmem_segid_t segid   = 0;
    int           ret     = 0;

    if ((u64)vaddr & (PAGE_SIZE - 1)) {
	XPMEM_ERR("Cannot export non page-aligned virtual address %p", vaddr);
	return -EINVAL;
    }

    if (size & (PAGE_SIZE - 1)) {
	XPMEM_ERR("Cannot export region with non page-aligned size %llu", (unsigned long long)size);
	return -EINVAL;
    }

    if (flags & XPMEM_REQUEST_MODE)  {
	if (request <= 0 || request > XPMEM_MAX_WK_SEGID) {
	    return -EINVAL;
	}
    } else {
	request = 0;
    }

    /* Request a segid from the nameserver */
    ret = xpmem_make_remote(xpmem_my_part->domain_link, request, &segid);

    if (ret == 0) {
	/* Store the SMARTMAP info in the hashtable */
	struct xpmem_smartmap_info * info = kmem_alloc(sizeof(struct xpmem_smartmap_info));

	if (info == NULL) {
	    xpmem_remove_remote(xpmem_my_part->domain_link, segid);
	    return -1;
	}

	info->pid   = current->aspace->id;
	info->size  = size;
	info->vaddr = (u64)vaddr;

	ret = htable_insert(segid_map, (uintptr_t)segid, (uintptr_t)info);
	if (ret == 0) {
	    XPMEM_ERR("Cannot insert segid %lli into hashtable", segid);
	    xpmem_remove_remote(xpmem_my_part->domain_link, segid);
	    return -1;
	}

	/* Success */
	*segid_p = segid;

	return 0;
    }

    return ret;
}

static int
xpmem_remove(xpmem_segid_t segid)
{
    struct xpmem_smartmap_info * info = NULL;

    /* Remove from hashtable */
    info = (struct xpmem_smartmap_info *)htable_remove(segid_map, (uintptr_t)segid, 0);

    if (info == NULL) {
	XPMEM_ERR("Cannot find segid %lli in hashtable", segid);
	return -1;
    }

    kmem_free(info);

    /* Tell nameserver */
    xpmem_remove_remote(xpmem_my_part->domain_link, segid);

    return 0;
}

static int
xpmem_get(xpmem_segid_t  segid, 
	  int		 flags, 
	  int		 permit_type, 
	  void	       * permit_value, 
	  xpmem_apid_t * apid_p)
{
    struct xpmem_smartmap_info * info = NULL;
    xpmem_apid_t		 apid = 0;
    int				 ret  = 0;
    u64				 size = 0;

    /* Lookup segid in hashtable */
    info = (struct xpmem_smartmap_info *)htable_search(segid_map, (uintptr_t)segid);

    if (info == NULL) {

	/* Can't find the segid locally - need to check remotely */
	ret = xpmem_get_remote(xpmem_my_part->domain_link, segid, flags, permit_type, (u64)permit_value, &apid, &size);

	if (ret != 0) {
	    return -1;
	}

	if (apid <= 0) {
	    return -1;
	}

	/* We need a way to associate this apid as a remote one - map it to the remote
	 * segid */
	ret = htable_insert(apid_map, (uintptr_t)apid, (uintptr_t)segid);
	if (ret == 0) {
	    XPMEM_ERR("Cannot insert apid %lli into hashtable", apid); 
	    xpmem_release_remote(xpmem_my_part->domain_link, segid, apid);
	    return -1;
	}

	/* Success */
	*apid_p = apid;

	return 0;
    }

    /* Otherwise, the segid is a local segid - set to the segid and return */
    *apid_p = segid;

    return 0;
}

static int
xpmem_release(xpmem_apid_t apid)
{
    xpmem_segid_t segid = 0;
    int		  ret	= 0;

    /* Lookup remote segid in hashtable */
    segid = (xpmem_segid_t)htable_search(apid_map, (uintptr_t)apid);

    if (segid > 0) {
	/* This is a remotely created apid - release it */
	ret = xpmem_release_remote(xpmem_my_part->domain_link, segid, apid);

	if (ret != 0) {
	    return -1;
	}

	/* Remove from apid hashtable */
	(void)htable_remove(apid_map, (uintptr_t)apid, 0);

	/* Success */
	return 0;
    }
    
    /* Else, this is a locally created apid - nothing to do */
    return 0;
}


static int 
xpmem_try_attach_remote(xpmem_segid_t	segid,
			xpmem_apid_t	apid,
			off_t		off,
			size_t		size,
			u64	      * vaddr)
{
    int      ret     = 0;
    vaddr_t at_vaddr = 0;

    /* Find free address space */
    if (aspace_find_hole(current->aspace->id, 0, size, PAGE_SIZE, &at_vaddr)) {
	XPMEM_ERR("aspace_find_hole() failed");
	return -1;
    }

    /* Add region to aspace */
    if (aspace_add_region(current->aspace->id, at_vaddr, size, VM_READ | VM_WRITE | VM_USER,
		PAGE_SIZE, "xpmem")) {
	XPMEM_ERR("aspace_add_region() failed");
	return -1;
    }

    /* Attach to remote memory */
    ret = xpmem_attach_remote(xpmem_my_part->domain_link, segid, apid, off, size, (u64)at_vaddr);

    if (ret != 0) {
	aspace_del_region(current->aspace->id, at_vaddr, size);
	return -1;
    }

    /* Success */
    *vaddr = (u64)at_vaddr;
    return 0;
}

static int
xpmem_attach(xpmem_apid_t apid, 
	     off_t	  off, 
	     size_t	  size, 
	     u64	* vaddr)
{
    struct xpmem_smartmap_info * info  = NULL;
    xpmem_segid_t		 segid = 0;
    int				 ret   = 0;

    if (*vaddr) {
	XPMEM_ERR("Kitten does not support user-specified target vaddr");
	return -1;
    }

    if (off & (PAGE_SIZE - 1)) {
	XPMEM_ERR("Cannot attach region with non page-aligned offset %llu", (unsigned long long)off);
	return -1;
    }

    /* If the size is not page aligned, fix it */
    if (size & (PAGE_SIZE - 1)) {
	size += PAGE_SIZE - (size & (PAGE_SIZE - 1));
    }

    /* Lookup remote segid in hashtable */
    segid = (xpmem_segid_t)htable_search(apid_map, (uintptr_t)apid);

    if (segid > 0) {
	/* This is a remotely created apid - attach it */
	ret = xpmem_try_attach_remote(segid, apid, off, size, vaddr);

	if (ret != 0) {
	    return -1;
	}

	/* Success */
	return 0;
    }

    /* The apid maps to a locally created segment. Search the segid map for the source
     * smartmap parameters
     */
    info = (struct xpmem_smartmap_info *)htable_search(segid_map, (uintptr_t)apid);

    if (info == NULL) {
	/* Invalid apid: no local or remote creation */
	return -1;
    }

    /* Success */
    *vaddr = make_xpmem_addr(info->pid, (void *)info->vaddr) + off;
    return 0;
}

static int
xpmem_detach(u64 vaddr)
{
    /* No way to do these right now. NBD */
    return 0;
}


static long
xpmem_ioctl_op(struct file * filp, 
	       unsigned int  cmd, 
	       unsigned long arg)
{
    long ret = 0;

    switch (cmd) {
	case XPMEM_CMD_VERSION: {
	    return 0x00030000;
	}

	case XPMEM_CMD_MAKE: {
	    struct xpmem_cmd_make make_info;
	    xpmem_segid_t	  segid = -1;
	    int fd;

	    if (copy_from_user(&make_info, (void *)arg, 
			sizeof(struct xpmem_cmd_make)))
		return -EFAULT;

	    ret = xpmem_make((void *)make_info.vaddr, 
		    make_info.size, 
		    make_info.permit_type,
		    (void *)make_info.permit_value, 
		    make_info.flags,
		    make_info.segid, &segid, &fd);

	    if (ret != 0)
		return ret;

	    if (put_user(segid,
		    &((struct xpmem_cmd_make __user *)arg)->segid)) {
		xpmem_remove(segid);
		return -EFAULT;
	    }

	    return 0;
	}

	case XPMEM_CMD_REMOVE: {
	    struct xpmem_cmd_remove remove_info;

	    if (copy_from_user(&remove_info, (void *)arg, sizeof(struct xpmem_cmd_remove)))
		return -EFAULT;

	    return xpmem_remove(remove_info.segid);
	}

	case XPMEM_CMD_GET: {
	    struct xpmem_cmd_get get_info;
	    xpmem_apid_t apid = 0;

	    if (copy_from_user(&get_info, (void *)arg, sizeof(struct xpmem_cmd_get)))
		return -EFAULT;

	    ret = xpmem_get(get_info.segid, get_info.flags, get_info.permit_type,
		(void *)get_info.permit_value, &apid);

	    if (ret != 0)
		return ret;

	    if (put_user(apid,
		    &((struct xpmem_cmd_get __user *)arg)->apid)) {
		(void)xpmem_release(apid);
		return -EFAULT;
	    }

	    return 0;
	}

	case XPMEM_CMD_RELEASE: {
	    struct xpmem_cmd_release release_info;

	    if (copy_from_user(&release_info, (void *)arg, sizeof(struct xpmem_cmd_release)))
		return -EFAULT;

	    return xpmem_release(release_info.apid);
	}

	case XPMEM_CMD_ATTACH: {
	    struct xpmem_cmd_attach attach_info;
	    u64 at_vaddr = 0;

	    if (copy_from_user(&attach_info, (void *)arg, sizeof(struct xpmem_cmd_attach)))
		return -EFAULT;

	    ret = xpmem_attach(attach_info.apid, attach_info.offset, attach_info.size, &at_vaddr);

	    if (ret != 0)
		return ret;

	    if (put_user(at_vaddr,
		    &((struct xpmem_cmd_attach __user *)arg)->vaddr)) {
		(void)xpmem_detach(at_vaddr);
		return -EFAULT;
	    }

	    return 0;
	}

	case XPMEM_CMD_DETACH: {
	    struct xpmem_cmd_detach detach_info;

	    if (copy_from_user(&detach_info, (void *)arg, sizeof(struct xpmem_cmd_detach)))
		return -EFAULT;

	    return xpmem_detach(detach_info.vaddr);
	}

	case XPMEM_CMD_GET_DOMID: {
	    xpmem_domid_t domid = xpmem_get_domid();

	    if (put_user(domid,
		&((struct xpmem_cmd_domid __user *)arg)->domid))
		return -EFAULT;

	    return 0;
	}

	default: {
	    ret = -1;
	}
    }

    return ret;
}

static int
xpmem_open_op(struct inode * inodep, 
	      struct file  * filp)
{
    filp->private_data = inodep->priv;
    return 0;
}

static struct kfs_fops
xpmem_fops = 
{
    .open		= xpmem_open_op,
    .unlocked_ioctl	= xpmem_ioctl_op,
    .compat_ioctl	= xpmem_ioctl_op,
};


static int
xpmem_init(void)
{
    int ret, is_ns;

    xpmem_my_part = kmem_alloc(sizeof(struct xpmem_partition));
    if (xpmem_my_part == NULL)
	return -ENOMEM;

    segid_map = create_htable(0, segid_hash_fn, segid_eq_fn);
    if (segid_map == NULL) {
	ret = -ENOMEM;
	goto out_1;
    }

    apid_map = create_htable(0, segid_hash_fn, segid_eq_fn);
    if (apid_map == NULL) {
	ret = -ENOMEM;
	goto out_2;
    }
    
#ifdef CONFIG_XPMEM_NS
    is_ns = 1;
#else
    is_ns = 0;
#endif

    ret = xpmem_partition_init(is_ns);
    if (ret != 0)
	goto out_3;

    xpmem_my_part->domain_link = xpmem_domain_init();
    if (xpmem_my_part->domain_link < 0) {
	ret = xpmem_my_part->domain_link;
	goto out_4;
    }

#ifdef CONFIG_PISCES
    ret = pisces_xpmem_init();
    if (ret != 0)
	goto out_5;
#endif

    kfs_create(XPMEM_DEV_PATH,
	NULL,
	&xpmem_fops,
	0777,
	NULL,
	0);

    return 0;

#ifdef CONFIG_PISCES
out_5:
    xpmem_domain_deinit(xpmem_my_part->domain_link);
#endif
out_4:
    xpmem_partition_deinit();
out_3:
    free_htable(segid_map, 1, 0);
out_2:
    free_htable(segid_map, 1, 0);
out_1:
    kmem_free(xpmem_my_part);
    return ret;
}



struct xpmem_partition * 
get_local_partition(void)
{
    return xpmem_my_part;
}


int
do_xpmem_attach_domain(xpmem_apid_t    apid,
		       off_t	       offset,
		       size_t	       size,
		       u64             num_pfns,
		       u64             pfn_pa)
{
    struct xpmem_smartmap_info * info = NULL;

    u64   i    = 0;
    u32 * pfns = NULL;

    vaddr_t seg_vaddr = 0;
    pid_t   seg_pid   = 0;

    /* Search the segid map for SMARTMAP info */
    info = (struct xpmem_smartmap_info *)htable_search(segid_map, (uintptr_t)apid);
    if (info == NULL) {
	return -1;
    }

    /* Grab the SMARTMAP'd virtual address */
    seg_vaddr = info->vaddr;
    seg_pid   = info->pid;
    
    /* pfn list is preallocated */
    pfns = __va(pfn_pa);

    for (i = 0; i < num_pfns; i++) {
	paddr_t vaddr = 0;
	paddr_t paddr = 0;

	vaddr = seg_vaddr + (i * PAGE_SIZE);

	if (aspace_virt_to_phys(seg_pid, vaddr, &paddr)) {
	    XPMEM_ERR("Cannot get PFN for process %d vaddr %p", seg_pid, (void *)vaddr);
	    kmem_free(pfns);
	    return -1;
	}

	/* Save pfn in the remote domain list */
	pfns[i] = (paddr >> PAGE_SHIFT);
    }

    /* Success */
    return 0;
}

DRIVER_INIT("kfs", xpmem_init);
