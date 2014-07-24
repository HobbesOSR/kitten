/*
 * XPMEM extensions for multiple domain support
 *
 * Thie file implements core driver support for regular XPMEM commands. XPMEM
 * registrations (invocations of xpmem_make()) must be sent to the name service,
 * as do gets/attachments of remote exported regions.
 *
 * Local makes/gets/attaches proceed as normal via SMARTMAP. However, in order
 * to determine that a region has been exported by a local and not a remote
 * process, the segid uniq field must only be allocated from a range guaranteed
 * to only be allocated from by local processes. This range can be queried from
 * the name service by attempting the registration of a segid with uniq value -1.
 *
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
static struct xpmem_hashtable * segid_map = NULL;
DEFINE_SPINLOCK(map_lock);


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

static int
xpmem_open_op(struct inode * inodep, 
              struct file  * filp)
{
    filp->private_data = inodep->priv;
    return 0;
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
xpmem_make(void          * vaddr, 
           size_t          size, 
	   int             permit_type, 
	   void          * permit_value, 
	   xpmem_segid_t * segid_p)
{
    int ret = 0;

    if ((u64)vaddr & (PAGE_SIZE - 1)) {
	printk(KERN_ERR "XPMEM: Cannot export non page-aligned virtual address %p\n", vaddr);
	return -1;
    }

    if (size & (PAGE_SIZE - 1)) {
	printk(KERN_ERR "XPMEM: Cannot export region with non page-aligned size %llu\n", (unsigned long long)size);
	return -1;
    }

    *segid_p = make_xpmem_addr(current->aspace->id, vaddr);

    {
	xpmem_segid_t ext_segid;
	struct xpmem_id * id = (struct xpmem_id *)&ext_segid;
	id->tgid = current->aspace->id;
	id->uniq = 0;

        ret = xpmem_make_remote(&(xpmem_my_part->part_state), &ext_segid);
	if (ret) {
	    *segid_p = -1;
	} else {
	    htable_insert(segid_map, (uintptr_t)ext_segid, (uintptr_t)*segid_p);
	    *segid_p = ext_segid;
	}
    }

    return ret;
}

static int
xpmem_remove(xpmem_segid_t segid)
{
    htable_remove(segid_map, (uintptr_t)segid, 0);
    return xpmem_remove_remote(&(xpmem_my_part->part_state), segid);
}

static int
xpmem_get(xpmem_segid_t  segid, 
          int            flags, 
	  int            permit_type, 
	  void         * permit_value, 
	  xpmem_apid_t * apid_p)
{
    int ret = 0;

    *apid_p = segid;

    {
	xpmem_segid_t search = (xpmem_segid_t)htable_search(segid_map, segid);
	if (search == 0) {
	    /* Miss means the segid is registered remotely */
	    ret = xpmem_get_remote(&(xpmem_my_part->part_state), segid, flags, permit_type, (u64)permit_value, apid_p);
	}
    }
    
    return ret;
}

static int
xpmem_release(xpmem_apid_t apid)
{
    return xpmem_release_remote(&(xpmem_my_part->part_state), apid);
}

static int
xpmem_attach(xpmem_apid_t apid, 
             off_t        off, 
	     size_t       size, 
	     u64        * vaddr)
{
    int ret = 0;

    if (*vaddr) {
	printk(KERN_ERR "XPMEM: Kitten does not support user-specified target vaddr\n");
	return -1;
    }

    if (off & (PAGE_SIZE - 1)) {
	printk(KERN_ERR "XPMEM: Cannot attach region with non page-aligned offset %llu\n", (unsigned long long)off);
	return -1;
    }

    /* If the size is not page aligned, fix it */
    if (size & (PAGE_SIZE - 1)) {
	size += PAGE_SIZE - (size & (PAGE_SIZE - 1));
    }

    {
	xpmem_segid_t search = (xpmem_segid_t)htable_search(segid_map, apid);
	if (search == 0) {
	    /* Miss means the apid is registered remotely */
	    ret = xpmem_attach_remote(&(xpmem_my_part->part_state), apid, off, size, vaddr);
	} else {
	    /* Hit means the apid is registered locally and is the smartmap'd virtual adddress */
	    *vaddr = (u64)((void *)search + off);
	}
    }
    
    return ret;
}

static int
xpmem_detach(u64 vaddr)
{
    return xpmem_detach_remote(&(xpmem_my_part->part_state), vaddr);
}


static long
xpmem_ioctl_op(struct file * filp, 
               unsigned int  cmd, 
	       unsigned long arg)
{
    long ret = 0;

    switch (cmd) {
	case XPMEM_CMD_VERSION: {
	    return 0x00022000;
	}

	case XPMEM_CMD_MAKE: {
	    struct xpmem_cmd_make make_info;

	    if (copy_from_user(&make_info, (void *)arg, sizeof(struct xpmem_cmd_make)))
		return -EFAULT;

	    ret = xpmem_make((void *)make_info.vaddr, make_info.size, make_info.permit_type,
		(void *)make_info.permit_value, &make_info.segid);

	    if (ret != 0)
		return ret;

            if (copy_to_user((void *)arg, &make_info, sizeof(struct xpmem_cmd_make))) {
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

	    if (copy_from_user(&get_info, (void *)arg, sizeof(struct xpmem_cmd_get)))
		return -EFAULT;

	    ret = xpmem_get(get_info.segid, get_info.flags, get_info.permit_type,
		(void *)get_info.permit_value, &get_info.apid);

	    if (ret != 0)
		return ret;

	    if (copy_to_user((void *)arg, &get_info, sizeof(struct xpmem_cmd_get)))
		return -EFAULT;

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

	    if (copy_from_user(&attach_info, (void *)arg, sizeof(struct xpmem_cmd_attach)))
		return -EFAULT;

	    ret = xpmem_attach(attach_info.apid, attach_info.offset, attach_info.size, &attach_info.vaddr);

	    if (ret != 0)
		return ret;

	    if (copy_to_user((void *)arg, &attach_info, sizeof(struct xpmem_cmd_attach)))
		return -EFAULT;

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

static struct kfs_fops
xpmem_fops = 
{
    .open               = xpmem_open_op,
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
    
    kfs_create("/xpmem",
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

    segid_map = create_htable(0, segid_hash_fn, segid_eq_fn);
    if (!segid_map) {
	printk(KERN_ERR "XPMEM: Could not create segid map\n");
	return -ENOMEM;
    }

    return 0;
}



struct xpmem_partition * 
get_local_partition(void)
{
    return xpmem_my_part;
}


xpmem_apid_t 
xpmem_get_local_apid(xpmem_apid_t remote_apid)
{
    if (!segid_map) {
	return -1;
    }

    return (xpmem_apid_t)htable_search(segid_map, remote_apid);
}


DRIVER_INIT("kfs", xpmem_init);
