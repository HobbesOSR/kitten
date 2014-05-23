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
#include "hashtable.h"

#include <xpmem.h>
#include <xpmem_extended.h>


/* TODO: Set this based on a config option */
#define EXT_PISCES


static struct xpmem_partition * my_part = NULL;
static struct hashtable * segid_map = NULL;
DEFINE_SPINLOCK(map_lock);


struct xpmem_id {
    pid_t tgid;
    unsigned short uniq;
};

static u32 segid_hash_fn(uintptr_t key) {
    return pisces_hash_long(key, 32);
}

static int segid_eq_fn(uintptr_t key1, uintptr_t key2) {
    return (key1 == key2);
}

static int
xpmem_open_op(struct inode * inodep, struct file * filp)
{
    filp->private_data = inodep->priv;
    return 0;
}

static int
xpmem_release_op(struct inode * inodep, struct file * filp)
{
    return 0;
}



static ssize_t
xpmem_read_op(struct file * filp, char __user * ubuf, size_t size, loff_t * off)
{
    return 0;
}

static ssize_t
xpmem_write_op(struct file * filp, const char __user * ubuf, size_t size, loff_t * off)
{
    return 0;
}

static unsigned long
make_xpmem_addr(pid_t src, void * vaddr) 
{
    unsigned long slot;

    if (src == INIT_ASPACE_ID)
	slot = 0;
    else
	slot = src - 0x1000 + 1;

    return (((slot + 1) << SMARTMAP_SHIFT) | ((unsigned long) vaddr));
}

static int
xpmem_make(void *vaddr, size_t size, int permit_type, void * permit_value, xpmem_segid_t * segid_p)
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

    if (extend_enabled) {
	xpmem_segid_t ext_segid;
	struct xpmem_id * id = (struct xpmem_id *)&ext_segid;
	id->tgid = current->aspace->id;
	id->uniq = 0;

	ret = xpmem_extended_ops->make(my_part, &ext_segid);
	if (ret) {
	    *segid_p = -1;
	} else {
	    printk("Stored segid/apid %lli in map\n", ext_segid);
	    pisces_htable_insert(segid_map, (uintptr_t)ext_segid, (uintptr_t)*segid_p);
	    *segid_p = ext_segid;
	}
    }

    return ret;
}

static int
xpmem_remove(xpmem_segid_t segid)
{
    if (extend_enabled) {
	pisces_htable_remove(segid_map, (uintptr_t)segid, 0);
	return xpmem_extended_ops->remove(my_part, segid);
    }

    return 0;
}

static int
xpmem_get(xpmem_segid_t segid, int flags, int permit_type, void * permit_value, xpmem_apid_t * apid_p)
{
    int ret = 0;

    *apid_p = segid;

    if (extend_enabled) {
	xpmem_segid_t search = (xpmem_segid_t)pisces_htable_search(segid_map, segid);
	if (search == 0) {
	    /* Miss means the segid is registered remotely */
	    ret = xpmem_extended_ops->get(my_part, segid, flags, permit_type, (u64)permit_value, apid_p);
	}
    }
    
    return ret;
}

static int
xpmem_release(xpmem_apid_t apid)
{
    if (extend_enabled) {
	return xpmem_extended_ops->release(my_part, apid);
    }

    return 0;
}

static int
xpmem_attach(xpmem_apid_t apid, off_t off, size_t size, u64 * vaddr)
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

    if (extend_enabled) {
	xpmem_segid_t search = (xpmem_segid_t)pisces_htable_search(segid_map, apid);
	if (search == 0) {
	    /* Miss means the apid is registered remotely */
	    ret = xpmem_extended_ops->attach(my_part, apid, off, size, vaddr);
	} else {
	    /* Hit means the apid is registered locally and is the smartmap'd virtual adddress */
	    *vaddr = (u64)((void *)search + off);
	}
    } else {
	*vaddr = (u64)((void *)apid + off);
    }
    
    return ret;
}

static int
xpmem_detach(u64 vaddr)
{
    if (extend_enabled) {
	return xpmem_extended_ops->detach(my_part, vaddr);
    }

    return 0;
}


static long
xpmem_ioctl_op(struct file * filp, unsigned int cmd, unsigned long arg)
{
    long ret;

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

	case XPMEM_CMD_EXT_LOCAL_CONNECT: {
	    return xpmem_local_connect(my_part->pisces_state);
	}

	case XPMEM_CMD_EXT_LOCAL_DISCONNECT: {
	    return xpmem_local_disconnect(my_part->pisces_state);
	}

	case XPMEM_CMD_EXT_REMOTE_CONNECT: {
	    return xpmem_remote_connect(my_part->pisces_state);
	}

	case XPMEM_CMD_EXT_REMOTE_DISCONNECT: {
	    return xpmem_remote_disconnect(my_part->pisces_state);
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
    .release            = xpmem_release_op,
    .read               = xpmem_read_op,
    .write              = xpmem_write_op,
    .unlocked_ioctl	= xpmem_ioctl_op,
    .compat_ioctl	= xpmem_ioctl_op,
};


static int
xpmem_init(void)
{
    my_part = kmem_alloc(sizeof(struct xpmem_partition));
    if (!my_part) {
	return -ENOMEM;
    }
    
    kfs_create("/xpmem",
	NULL,
	&xpmem_fops,
	0777,
	NULL,
	0);

#ifdef EXT_PISCES
    xpmem_pisces_init(my_part);
    xpmem_extended_ops = &pisces_ops;
    extend_enabled = 1;
#else
    xpmem_extended_ops = NULL;
    extend_enabled = 0;
#endif

    segid_map = pisces_create_htable(0, segid_hash_fn, segid_eq_fn);
    if (!segid_map) {
	printk(KERN_ERR "XPMEM: Could not create segid map\n");
	return -ENOMEM;
    }

    return 0;
}


xpmem_apid_t xpmem_get_local_apid(xpmem_apid_t remote_apid) {
    if (!segid_map) {
	return -1;
    }

    return pisces_htable_search(segid_map, remote_apid);
}


DRIVER_INIT("kfs", xpmem_init);
