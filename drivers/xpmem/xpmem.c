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

#include <xpmem_partition.h>
#include <xpmem_hashtable.h>


static struct xpmem_partition * xpmem_my_part = NULL;


/* Segids are creates in a fashion that conflicts with Kitten's SMARTMAP-based approach.
 * So, we keep a hashtable where the segid maps to the source process' SMARTMAP rank and
 * address info so that we can convert segids to SMARTMAP-able apids during XPMEM attach
 * operations
 */
static struct xpmem_hashtable * segid_map     = NULL;
DEFINE_SPINLOCK(segid_map_lock);


/* We also need to maintain an apid map for apids created remotely and assigned to Kitten.
 * The reason for this is that remote releases and attachments require both a segid and an
 * apid, but users are not required to pass the segid for these operations. So, we map
 * apids to remote segids here
 */
static struct xpmem_hashtable * apid_map      = NULL;
DEFINE_SPINLOCK(apid_map_lock);


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


/* Hashtable helpers */
static int
xpmem_ht_add(struct xpmem_hashtable * ht,
	     spinlock_t		    * lock,
	     uintptr_t		      key,
	     uintptr_t		      val)
{
    int ret = 0;

    spin_lock(lock);
    {
	ret = htable_insert(ht, key, val);
    }
    spin_unlock(lock);

    return ret;
}

static uintptr_t
xpmem_ht_search_or_remove(struct xpmem_hashtable * ht,
			  spinlock_t		 * lock,
			  uintptr_t		   key,
			  int			   remove)
{
    uintptr_t ret = 0;

    spin_lock(lock);
    {
	if (remove) {
	    ret = htable_remove(ht, key, 0);
	} else {
	    ret = htable_search(ht, key);
	}
    }
    spin_unlock(lock);

    return ret;
}

static uintptr_t
xpmem_ht_search(struct xpmem_hashtable * ht,
		spinlock_t	       * lock,
		uintptr_t		 key)
{
    return xpmem_ht_search_or_remove(ht, lock, key, 0);
}

static uintptr_t
xpmem_ht_remove(struct xpmem_hashtable * ht,
		spinlock_t	       * lock,
		uintptr_t		 key)
{
    return xpmem_ht_search_or_remove(ht, lock, key, 1);
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
	   char		 * name,
	   xpmem_segid_t * segid_p)
{
    xpmem_segid_t segid = 0;
    int		  ret	= 0;

    if ((u64)vaddr & (PAGE_SIZE - 1)) {
	XPMEM_ERR("Cannot export non page-aligned virtual address %p", vaddr);
	return -1;
    }

    if (size & (PAGE_SIZE - 1)) {
	XPMEM_ERR("Cannot export region with non page-aligned size %llu", (unsigned long long)size);
	return -1;
    }

    /* Request a segid from the nameserver */
    ret = xpmem_make_remote(&(xpmem_my_part->part_state), name, &segid);

    if (ret == 0) {
	/* Store the SMARTMAP info in the hashtable */
	struct xpmem_smartmap_info * info = kmem_alloc(sizeof(struct xpmem_smartmap_info));

	if (info == NULL) {
	    xpmem_remove_remote(&(xpmem_my_part->part_state), segid);
	    return -1;
	}

	info->pid   = current->aspace->id;
	info->size  = size;
	info->vaddr = (u64)vaddr;

	ret = xpmem_ht_add(segid_map, &segid_map_lock, (uintptr_t)segid, (uintptr_t)info);
	if (ret == 0) {
	    XPMEM_ERR("Cannot insert segid %lli into hashtable", segid);
	    xpmem_remove_remote(&(xpmem_my_part->part_state), segid);
	    return -1;
	}

	/* Success */
	*segid_p = segid;

	return 0;
    }

    return ret;
}

static int
xpmem_search(char	   * name,
	     xpmem_segid_t * segid)
{
    return xpmem_search_remote(&(xpmem_my_part->part_state), name, segid);
}

static int
xpmem_remove(xpmem_segid_t segid)
{
    struct xpmem_smartmap_info * info = NULL;

    /* Remove from hashtable */
    info = (struct xpmem_smartmap_info *)xpmem_ht_remove(segid_map, &segid_map_lock, (uintptr_t)segid);

    if (info == NULL) {
	XPMEM_ERR("Cannot find segid %lli in hashtable", segid);
	return -1;
    }

    kmem_free(info);

    /* Tell nameserver */
    xpmem_remove_remote(&(xpmem_my_part->part_state), segid);

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
    info = (struct xpmem_smartmap_info *)xpmem_ht_search(segid_map, &segid_map_lock, (uintptr_t)segid);

    if (info == NULL) {

	/* Can't find the segid locally - need to check remotely */
	ret = xpmem_get_remote(&(xpmem_my_part->part_state), segid, flags, permit_type, (u64)permit_value, &apid, &size);

	if (ret != 0) {
	    return -1;
	}

	if (apid <= 0) {
	    return -1;
	}

	/* We need a way to associate this apid as a remote one - map it to the remote
	 * segid */
	ret = xpmem_ht_add(apid_map, &apid_map_lock, (uintptr_t)apid, (uintptr_t)segid);
	if (ret == 0) {
	    XPMEM_ERR("Cannot insert apid %lli into hashtable", apid); 
	    xpmem_release_remote(&(xpmem_my_part->part_state), segid, apid);
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
    segid = (xpmem_segid_t)xpmem_ht_search(apid_map, &apid_map_lock, (uintptr_t)apid);

    if (segid > 0) {
	/* This is a remotely created apid - release it */
	ret = xpmem_release_remote(&(xpmem_my_part->part_state), segid, apid);

	if (ret != 0) {
	    return -1;
	}

	/* Remove from apid hashtable */
	xpmem_ht_remove(apid_map, &apid_map_lock, (uintptr_t)apid);

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
    ret = xpmem_attach_remote(&(xpmem_my_part->part_state), segid, apid, off, size, (u64)at_vaddr);

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
    segid = (xpmem_segid_t)xpmem_ht_search(apid_map, &apid_map_lock, (uintptr_t)apid);

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
    info = (struct xpmem_smartmap_info *)xpmem_ht_search(segid_map, &segid_map_lock, (uintptr_t)apid);

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

	    if (copy_from_user(&make_info, (void *)arg, 
			sizeof(struct xpmem_cmd_make)))
		return -EFAULT;

	    if (make_info.name_size > XPMEM_MAXNAME_SIZE)
		return -EINVAL;


	    if (make_info.name_size > 0) {
		if (strncpy_from_user(make_info.name,
			((struct xpmem_cmd_make *)arg)->name,
			make_info.name_size) < 0)
		    return -EFAULT;

		/* Ensure name is truncated */
		make_info.name[XPMEM_MAXNAME_SIZE - 1] = 0;
	    } else {
		make_info.name = NULL;
	    }

	    ret = xpmem_make((void *)make_info.vaddr, 
		    make_info.size, 
		    make_info.permit_type,
		    (void *)make_info.permit_value, 
		    make_info.name,
		    &segid);

	    if (ret != 0)
		return ret;

	    if (put_user(segid,
		    &((struct xpmem_cmd_make __user *)arg)->segid)) {
		xpmem_remove(segid);
		return -EFAULT;
	    }

	    return 0;
	}

	case XPMEM_CMD_SEARCH: {
	    struct xpmem_cmd_search search_info;
	    xpmem_segid_t segid = 0;

	    if (copy_from_user(&search_info, (void __user *)arg,
		       sizeof(struct xpmem_cmd_search)))
		return -EFAULT;

	    if (search_info.name_size > XPMEM_MAXNAME_SIZE)
		return -EINVAL;

	    if (search_info.name_size <= 0) 
		return -EINVAL;

	    if (strncpy_from_user(search_info.name,
		    ((struct xpmem_cmd_search *)arg)->name,
		    search_info.name_size) < 0)
		return -EFAULT;

	    /* Ensure name is truncated */
	    search_info.name[XPMEM_MAXNAME_SIZE - 1] = 0;

	    ret = xpmem_search(search_info.name, &segid);
	    if (ret != 0)
		return ret;

	    if (put_user(segid,
		    &((struct xpmem_cmd_search __user *)arg)->segid))
		return -EFAULT;

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
    xpmem_my_part = kmem_alloc(sizeof(struct xpmem_partition));
    if (!xpmem_my_part) {
	return -ENOMEM;
    }

    segid_map = create_htable(0, segid_hash_fn, segid_eq_fn);
    if (!segid_map) {
	kmem_free(xpmem_my_part);
	return -ENOMEM;
    }

    apid_map  = create_htable(0, segid_hash_fn, segid_eq_fn);
    if (!apid_map) {
	free_htable(segid_map, 0, 0);
	kmem_free(xpmem_my_part);
	return -ENOMEM;
    }
    
    kfs_create(XPMEM_DEV_PATH,
	NULL,
	&xpmem_fops,
	0777,
	NULL,
	0);

#ifdef CONFIG_XPMEM_NS
    xpmem_partition_init(&(xpmem_my_part->part_state), 1);
#else
    xpmem_partition_init(&(xpmem_my_part->part_state), 0);
#endif

#ifdef CONFIG_PISCES
    pisces_xpmem_init();
#endif

    return 0;
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
		       u64	    ** p_pfns,
		       u64	     * p_num_pfns)
{
    struct xpmem_smartmap_info * info	    = NULL;
    u64			       * pfns	    = NULL;
    u64				 num_pfns   = 0;
    u64				 i	    = 0;

    vaddr_t			 seg_vaddr  = 0;
    pid_t			 seg_pid    = 0;


    *p_pfns	= NULL;
    *p_num_pfns = 0;

    /* Search the segid map for SMARTMAP info */
    info = (struct xpmem_smartmap_info *)xpmem_ht_search(segid_map, &segid_map_lock, (uintptr_t)apid);
    if (info == NULL) {
	return -1;
    }

    /* Grab the SMARTMAP'd virtual address */
    //seg_vaddr = make_xpmem_addr(info->pid, (void *)info->vaddr + offset);
    //seg_vaddr -= 0x8000000000;
    seg_vaddr = info->vaddr;
    seg_pid   = info->pid;

    num_pfns  = size / PAGE_SIZE;
    pfns      = kmem_alloc(sizeof(u64) * num_pfns);
    if (!pfns) {
	return -1;
    }

    for (i = 0; i < num_pfns; i++) {
	paddr_t vaddr = 0;
	paddr_t paddr = 0;

	vaddr = seg_vaddr + (i * PAGE_SIZE);

	if (aspace_virt_to_phys(seg_pid, vaddr, &paddr)) {
	    XPMEM_ERR("Cannot get PFN for process %d vaddr %p", seg_pid, (void *)vaddr);
	    kmem_free(pfns);
	    return -1;
	}

	pfns[i] = (paddr >> PAGE_SHIFT);
    }

    /* Success */
    *p_pfns	= pfns;
    *p_num_pfns = num_pfns;

    return 0;
}

DRIVER_INIT("kfs", xpmem_init);
