/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2004-2007 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * Cross Partition Memory (XPMEM) support.
 *
 * This module (along with a corresponding library) provides support for
 * cross-partition shared memory between threads.
 *
 * UPDATE:
 * 
 *  Port to Kitten including updates for cross-enclave XPMEM added by 
 *  Brian Kocoloski <briankoco@cs.pitt.edu>, 2015
 *
 */



#include <xpmem.h>
#include <xpmem_extended.h>
#include <xpmem_private.h>

#include <arch/uaccess.h>
#include <arch/pisces/pisces_xpmem.h>
#include <arch-generic/fcntl.h>

#include <lwk/kfs.h>
#include <lwk/driver.h>


struct xpmem_partition * xpmem_my_part  = NULL;  /* pointer to this partition */


#ifdef CONFIG_XPMEM_NS
static int is_ns = 1;
#else
static int is_ns = 0;
#endif

static int
xpmem_init_aspace(struct aspace * aspace)
{
    struct xpmem_thread_group *tg;
    int index;
    unsigned long flags;

    /* if this has already been done, just return silently */
    tg = xpmem_tg_ref_by_tgid(aspace->id);
    if (!IS_ERR(tg)) {
        xpmem_tg_deref(tg);
        return 0;
    }

    /* create tg */
    tg = kmem_alloc(sizeof(struct xpmem_thread_group));
    if (tg == NULL)
        return -ENOMEM;

    spin_lock_init(&(tg->lock));
    tg->tgid = aspace->id;
    atomic_set(&tg->uniq_apid, 0);
    rwlock_init(&(tg->seg_list_lock));
    INIT_LIST_HEAD(&tg->seg_list);
    INIT_LIST_HEAD(&tg->tg_hashnode);
    tg->aspace = aspace;

    /* create and initialize struct xpmem_access_permit hashtable */
    tg->ap_hashtable = kmem_alloc(sizeof(struct xpmem_hashlist) * XPMEM_AP_HASHTABLE_SIZE);
    if (tg->ap_hashtable == NULL) {
        kmem_free(tg);
        return -ENOMEM;
    }

    for (index = 0; index < XPMEM_AP_HASHTABLE_SIZE; index++) {
        rwlock_init(&(tg->ap_hashtable[index].lock));
        INIT_LIST_HEAD(&tg->ap_hashtable[index].list);
    }

    /* create and initialize struct xpmem_attachment hashtable */
    tg->att_hashtable = kmem_alloc(sizeof(struct xpmem_hashlist) * XPMEM_ATT_HASHTABLE_SIZE);
    if (tg->att_hashtable == NULL) {
	kmem_free(tg->ap_hashtable);
        kmem_free(tg);
        return -ENOMEM;
    }

    for (index = 0; index < XPMEM_ATT_HASHTABLE_SIZE; index++) {
        rwlock_init(&(tg->att_hashtable[index].lock));
        INIT_LIST_HEAD(&tg->att_hashtable[index].list);
    }

    xpmem_tg_not_destroyable(tg);

    /* add tg to its hash list */
    index = xpmem_tg_hashtable_index(tg->tgid);
    write_lock_irqsave(&xpmem_my_part->tg_hashtable[index].lock, flags);
    list_add_tail(&tg->tg_hashnode,
              &xpmem_my_part->tg_hashtable[index].list);
    write_unlock_irqrestore(&xpmem_my_part->tg_hashtable[index].lock, flags);

    return 0;
}



/*
 * User open of the XPMEM driver. Called whenever /dev/xpmem is opened.
 * Create a struct xpmem_thread_group structure for the specified thread group.
 * And add the structure to the tg hash table.
 */
static int
xpmem_open(struct inode *inode, struct file *file)
{
    return xpmem_init_aspace(current->aspace);
}

static int
xpmem_flush_tg(struct xpmem_thread_group * tg)
{
    int index;
    unsigned long flags;

    spin_lock_irqsave(&tg->lock, flags);
    if (tg->flags & XPMEM_FLAG_DESTROYING) {
	spin_unlock_irqrestore(&tg->lock, flags);
	xpmem_tg_deref(tg);
	return -EALREADY;
    }
    tg->flags |= XPMEM_FLAG_DESTROYING;
    spin_unlock_irqrestore(&tg->lock, flags);

    xpmem_release_aps_of_tg(tg);
    xpmem_remove_segs_of_tg(tg);

    /* Remove tg structure from its hash list */
    index = xpmem_tg_hashtable_index(tg->tgid);
    write_lock_irqsave(&xpmem_my_part->tg_hashtable[index].lock, flags);
    list_del_init(&tg->tg_hashnode);
    write_unlock_irqrestore(&xpmem_my_part->tg_hashtable[index].lock, flags);

    xpmem_tg_destroyable(tg);
    xpmem_tg_deref(tg);

    return 0;
}

static int
xpmem_release_aspace(struct aspace * aspace)
{
    struct xpmem_thread_group *tg;

    tg = xpmem_tg_ref_by_tgid(aspace->id);
    if (tg == NULL) {
        /*
         * xpmem_flush() can get called twice for thread groups
         * which inherited /dev/xpmem: once for the inherited fd,
         * once for the first explicit use of /dev/xpmem. If we
         * don't find the tg via xpmem_tg_ref_by_tgid() we assume we
         * are in this type of scenario and return silently.
         */
        return 0;
    }

    return xpmem_flush_tg(tg);
}


static int
xpmem_close(struct file * filp)
{
    return xpmem_release_aspace(current->aspace);
}


/*
 * The following function gets called whenever a thread group that has opened
 * /dev/xpmem closes it.
 */
static int
xpmem_release_op(struct inode * inodep,
                 struct file  * filp)
{
    return xpmem_release_aspace(current->aspace);
}

/*
 * User ioctl to the XPMEM driver. Only 64-bit user applications are
 * supported.
 */
static long
xpmem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret;

    if (cmd == XPMEM_CMD_GET_DOMID) {
        xpmem_domid_t domid = xpmem_get_domid();

        if (put_user(domid,
                &((struct xpmem_cmd_domid __user *)arg)->domid)) 
            return -EFAULT;

        return 0;
    }

    /* Make sure we have a domid */
    if (!xpmem_ensure_valid_domid())
        return -EBUSY;

    switch (cmd) {
        case XPMEM_CMD_VERSION: {
            return XPMEM_CURRENT_VERSION;
        }
        case XPMEM_CMD_MAKE: {
            struct xpmem_cmd_make make_info;
            xpmem_segid_t segid;
            int fd;

            if (copy_from_user(&make_info, (void __user *)arg,
                       sizeof(struct xpmem_cmd_make)))
                return -EFAULT;

            ret = xpmem_make(make_info.vaddr, make_info.size,
                     make_info.permit_type,
                     (void *)make_info.permit_value,
                     make_info.flags,
                     make_info.segid, &segid, &fd);
            if (ret != 0)
                return ret;

            if (put_user(segid,
                     &((struct xpmem_cmd_make __user *)arg)->segid)) {
                (void)xpmem_remove(segid);
                return -EFAULT;
            }

            if (put_user(fd,
                    &((struct xpmem_cmd_make __user *)arg)->fd)) {
                (void)xpmem_remove(segid);
                return -EFAULT;
            }

            return 0;
        }
        case XPMEM_CMD_REMOVE: {
            struct xpmem_cmd_remove remove_info;

            if (copy_from_user(&remove_info, (void __user *)arg,
                       sizeof(struct xpmem_cmd_remove)))
                return -EFAULT;

            return xpmem_remove(remove_info.segid);
        }
        case XPMEM_CMD_GET: {
            struct xpmem_cmd_get get_info;
            xpmem_apid_t apid;

            if (copy_from_user(&get_info, (void __user *)arg,
                       sizeof(struct xpmem_cmd_get)))
                return -EFAULT;

            ret = xpmem_get(get_info.segid, get_info.flags,
                    get_info.permit_type,
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

            if (copy_from_user(&release_info, (void __user *)arg,
                       sizeof(struct xpmem_cmd_release)))
                return -EFAULT;

            return xpmem_release(release_info.apid);
        }
        case XPMEM_CMD_SIGNAL: {
            struct xpmem_cmd_signal signal_info;

            if (copy_from_user(&signal_info, (void __user *)arg,
                      sizeof(struct xpmem_cmd_signal)))
                return -EFAULT;

            return xpmem_signal(signal_info.apid);
        }
        case XPMEM_CMD_ATTACH: {
            struct xpmem_cmd_attach attach_info;
            vaddr_t at_vaddr;

            if (copy_from_user(&attach_info, (void __user *)arg,
                       sizeof(struct xpmem_cmd_attach)))
                return -EFAULT;

            ret = xpmem_attach(attach_info.apid, attach_info.offset,
                       attach_info.size, 
		       attach_info.vaddr,
		       attach_info.flags,
                       &at_vaddr);
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

            if (copy_from_user(&detach_info, (void __user *)arg,
                       sizeof(struct xpmem_cmd_detach)))
                return -EFAULT;

            return xpmem_detach(detach_info.vaddr);
        }
        default:
            break;
    }

    return -ENOIOCTLCMD;
}


static struct kfs_fops 
xpmem_fops = 
{
    .open           = xpmem_open,
    .close          = xpmem_close,
    .release        = xpmem_release_op,
    .unlocked_ioctl = xpmem_ioctl,
    .compat_ioctl   = xpmem_ioctl
};



/* This allows kitten kernel threads (or anything running with
 * KERNEL_ASPACE_ID) use xpmem 
 */
static int
kernel_xpmem_init(void)
{
    struct aspace * aspace;
    int status;

    if (current->aspace->id != KERNEL_ASPACE_ID)
	return -EFAULT;

    aspace = aspace_acquire(KERNEL_ASPACE_ID);
    BUG_ON(aspace == NULL);

    status = xpmem_init_aspace(aspace);
    if (status != 0)
	aspace_release(aspace);

    return status;
}

static int
kernel_xpmem_finish(void)
{
    struct aspace * aspace;
    int status;

    if (current->aspace->id != KERNEL_ASPACE_ID)
	return -EFAULT;

    aspace = aspace_acquire(KERNEL_ASPACE_ID);
    BUG_ON(aspace == NULL);

    status = xpmem_release_aspace(aspace);
    aspace_release(aspace);

    if (status == 0)
	aspace_release(aspace);

    return status;
}

/*
 * Initialize the XPMEM driver.
 */
static int
xpmem_init(void)
{
    int i, ret;

    /* create and initialize struct xpmem_partition array */
    xpmem_my_part = kmem_alloc(sizeof(struct xpmem_partition));
    if (xpmem_my_part == NULL)
        return -ENOMEM;

    memset(xpmem_my_part, 0, sizeof(struct xpmem_partition));

    xpmem_my_part->tg_hashtable = kmem_alloc(sizeof(struct xpmem_hashlist) * XPMEM_TG_HASHTABLE_SIZE);
    if (xpmem_my_part->tg_hashtable == NULL) {
        kmem_free(xpmem_my_part);
        return -ENOMEM;
    }

    for (i = 0; i < XPMEM_TG_HASHTABLE_SIZE; i++) {
        rwlock_init(&(xpmem_my_part->tg_hashtable[i].lock));
        INIT_LIST_HEAD(&xpmem_my_part->tg_hashtable[i].list);
    }

    for (i = 0; i <= XPMEM_MAX_WK_SEGID; i++) {
	xpmem_my_part->wk_segid_to_tgid[i] = -1;
    }

    rwlock_init(&(xpmem_my_part->wk_segid_to_tgid_lock));

    ret = xpmem_partition_init(is_ns);
    if (ret != 0) 
        goto out_1;

    /* Register a local domain */
    xpmem_my_part->domain_link = xpmem_domain_init();
    if (xpmem_my_part->domain_link < 0) {
        ret = xpmem_my_part->domain_link;
        goto out_2;
    }

#ifdef CONFIG_PISCES
    xpmem_my_part->pisces_link = pisces_xpmem_init();
    if (xpmem_my_part->pisces_link < 0) {
	ret = xpmem_my_part->pisces_link;
	goto out_3;
    }
#endif

    ret = xpmem_palacios_init();
    if (ret < 0)
	goto out_4;

    kfs_create(XPMEM_DEV_PATH,
        NULL,
        &xpmem_fops,
        0777,
        NULL,
        0);

    kernel_xpmem_init();

    printk("XPMEM v%s loaded\n",
           XPMEM_CURRENT_VERSION_STRING);
    return 0;

out_4:
#ifdef CONFIG_PISCES
    pisces_xpmem_deinit(xpmem_my_part->pisces_link);
out_3:
    xpmem_domain_deinit(xpmem_my_part->domain_link);
#endif
out_2:
    xpmem_partition_deinit();
out_1:
    kmem_free(xpmem_my_part->tg_hashtable);
    kmem_free(xpmem_my_part);
    return ret;
}

/*
 * Remove the XPMEM driver from the system.
 */
static void
xpmem_exit(void)
{
    kernel_xpmem_finish();

    /* Free partition resources */
    xpmem_domain_deinit(xpmem_my_part->domain_link);
    xpmem_partition_deinit();
    xpmem_palacios_deinit();

#ifdef CONFIG_PISCES
    pisces_xpmem_deinit(xpmem_my_part->pisces_link);
#endif

    kmem_free(xpmem_my_part->tg_hashtable);
    kmem_free(xpmem_my_part);

    printk("XPMEM v%s unloaded\n",
           XPMEM_CURRENT_VERSION_STRING);
}

DRIVER_INIT("late", xpmem_init);
DRIVER_EXIT(xpmem_exit);
