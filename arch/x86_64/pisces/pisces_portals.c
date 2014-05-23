#include <lwk/types.h>
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <lwk/aspace.h>
#include <arch/uaccess.h>

#include <arch/pisces/pisces_boot_params.h>
#include <arch/pisces/pisces_lcall.h>
#include <arch/pisces/pisces_portals.h>
#include <arch/pisces/pisces_pagemap.h>


extern struct pisces_boot_params * pisces_boot_params;

struct pisces_portals {
    struct pisces_lcall_resp * ppe_resp;
} portals;



static void
clear_ppe_resp(void)
{
    if (portals.ppe_resp) {
        kmem_free(portals.ppe_resp);
        portals.ppe_resp = NULL;
    }
}

static int
portals_open(struct inode * inodep, struct file * filp)
{
    return 0;
}

static int
portals_close(struct file * filp)
{
    return 0;
}

/* Read a response to a generic user-level (non-xpmem) command */
static ssize_t
portals_read(struct file * filp, char __user * ubuf, size_t size, loff_t * off)
{
    u32 copy_len = 0;
    struct pisces_ppe_lcall * ppe_resp = (struct pisces_ppe_lcall *)portals.ppe_resp;

    if (!ppe_resp) {
        return 0;
    }

    copy_len = ppe_resp->lcall.data_len;
    if (copy_len > size) {
        copy_len = size;
    }

    if (copy_to_user(ubuf, ppe_resp->data, copy_len)) {
        printk(KERN_ERR "Error copying buffer to userspace\n");
        return -EFAULT;
    }

    clear_ppe_resp();

    return copy_len;
}

/* Write a generic (non-xpmem) command */
static ssize_t
portals_write(struct file * filp, const char __user * ubuf, size_t size, loff_t * off)
{
    struct pisces_ppe_lcall * ppe_lcall = NULL;
    ssize_t ret = 0;
    u32 cmd_len = 0;
    int status = 0;

    /* Clear any lingering buffers that we didn't read */
    clear_ppe_resp();

    cmd_len = sizeof(struct pisces_ppe_lcall) + size;

    ppe_lcall = kmem_alloc(cmd_len);
    if (!ppe_lcall) {
        printk(KERN_ERR "Out of memory!\n");
        return -ENOMEM;
    }

    if (copy_from_user(ppe_lcall->data, ubuf, size)) {
        printk(KERN_ERR "Error copying buffer from userspace\n");
        return -EFAULT;
    }

    ppe_lcall->lcall.lcall = PISCES_LCALL_PPE_MSG;
    ppe_lcall->lcall.data_len = cmd_len - sizeof(struct pisces_lcall);

    status = pisces_lcall_exec((struct pisces_lcall *)ppe_lcall,
            (struct pisces_lcall_resp **)&portals.ppe_resp);

    kmem_free(ppe_lcall);

    if (status != 0 || portals.ppe_resp->status < 0) {
        ret = -1;
    } else {
        ret = size;
    }

    return ret;
}


/* Ioctls are used to send xpmem commands over the longcall channel */
static long
portals_ioctl(struct file * filp, unsigned int ioctl, unsigned long arg)
{
    long ret = 0;
    int status = 0;

    switch (ioctl) {
        case ENCLAVE_IOCTL_XPMEM_VERSION: {
            struct pisces_xpmem_version_lcall version_lcall;
            struct pisces_xpmem_version_lcall * version_lcall_resp = NULL;

            if (copy_from_user(&(version_lcall.pisces_version), (void *)arg,
                        sizeof(struct pisces_xpmem_version))) {
                printk(KERN_ERR "Error copying buffer from userspace\n");
                ret = -EFAULT;
                break;
            }

            version_lcall.lcall.lcall = PISCES_LCALL_XPMEM_VERSION;
            version_lcall.lcall.data_len = sizeof(struct pisces_xpmem_version_lcall) -
                        sizeof(struct pisces_lcall);

            status = pisces_lcall_exec((struct pisces_lcall *)&version_lcall,
                        (struct pisces_lcall_resp **)&version_lcall_resp);

            if (status != 0 || version_lcall_resp->lcall_resp.status != 0) {
                ret = -EFAULT;
                break;
            }

            if (copy_to_user((void *)arg, &(version_lcall_resp->pisces_version),
                        sizeof(struct pisces_xpmem_version))) {
                printk(KERN_ERR "Error copying buffer to userspace\n");
                ret = -EFAULT;
            }

            kmem_free(version_lcall_resp);
            break;
        }
        case ENCLAVE_IOCTL_XPMEM_MAKE: {
            struct pisces_xpmem_make_lcall make_lcall;
            struct pisces_xpmem_make_lcall * make_lcall_resp = NULL;

            if (copy_from_user(&(make_lcall.pisces_make), (void *)arg,
                        sizeof(struct pisces_xpmem_make))) {
                printk(KERN_ERR "Error copying buffer from userspace\n");
                ret = -EFAULT;
                break;
            }

            make_lcall.lcall.lcall = PISCES_LCALL_XPMEM_MAKE;
            make_lcall.lcall.data_len = sizeof(struct pisces_xpmem_make_lcall) -
                        sizeof(struct pisces_lcall);

            /* Store our CR3 PA in the struct */
            make_lcall.aspace.cr3 = __pa(current->aspace->arch.pgd);

            status = pisces_lcall_exec((struct pisces_lcall *)&make_lcall,
                        (struct pisces_lcall_resp **)&make_lcall_resp);

            if (status != 0 || make_lcall_resp->lcall_resp.status != 0) {
                ret = -EFAULT;
                break;
            }

            if (copy_to_user((void *)arg, &(make_lcall_resp->pisces_make),
                        sizeof(struct pisces_xpmem_make))) {
                printk(KERN_ERR "Error copying buffer to userspace\n");
                ret = -EFAULT;
            }

            kmem_free(make_lcall_resp);
            break;
        }
        case ENCLAVE_IOCTL_XPMEM_REMOVE: {
            struct pisces_xpmem_remove_lcall remove_lcall;
            struct pisces_xpmem_remove_lcall * remove_lcall_resp = NULL;

            if (copy_from_user(&(remove_lcall.pisces_remove), (void *)arg,
                        sizeof(struct pisces_xpmem_remove))) {
                printk(KERN_ERR "Error copying buffer from userspace\n");
                ret = -EFAULT;
                break;
            }

            remove_lcall.lcall.lcall = PISCES_LCALL_XPMEM_REMOVE;
            remove_lcall.lcall.data_len = sizeof(struct pisces_xpmem_remove_lcall) -
                        sizeof(struct pisces_lcall);

            status = pisces_lcall_exec((struct pisces_lcall *)&remove_lcall,
                        (struct pisces_lcall_resp **)&remove_lcall_resp);

            if (status != 0 || remove_lcall_resp->lcall_resp.status != 0) {
                ret = -EFAULT;
                break;
            }

            if (copy_to_user((void *)arg, &(remove_lcall_resp->pisces_remove),
                        sizeof(struct pisces_xpmem_remove))) {
                printk(KERN_ERR "Error copying buffer to userspace\n");
                ret = -EFAULT;
            }

            kmem_free(remove_lcall_resp);
            break;
        }
        case ENCLAVE_IOCTL_XPMEM_GET: {
            struct pisces_xpmem_get_lcall get_lcall;
            struct pisces_xpmem_get_lcall * get_lcall_resp = NULL;

            if (copy_from_user(&(get_lcall.pisces_get), (void *)arg,
                        sizeof(struct pisces_xpmem_get))) {
                printk(KERN_ERR "Error copying buffer from userspace\n");
                ret = -EFAULT;
                break;
            }

            get_lcall.lcall.lcall = PISCES_LCALL_XPMEM_GET;
            get_lcall.lcall.data_len = sizeof(struct pisces_xpmem_get_lcall) -
                        sizeof(struct pisces_lcall);

            status = pisces_lcall_exec((struct pisces_lcall *)&get_lcall,
                        (struct pisces_lcall_resp **)&get_lcall_resp);

            if (status != 0 || get_lcall_resp->lcall_resp.status != 0) {
                ret = -EFAULT;
                break;
            }

            if (copy_to_user((void *)arg, &(get_lcall_resp->pisces_get),
                        sizeof(struct pisces_xpmem_get))) {
                printk(KERN_ERR "Error copying buffer to userspace\n");
                ret = -EFAULT;
            }

            kmem_free(get_lcall_resp);
            break;
        }
        case ENCLAVE_IOCTL_XPMEM_RELEASE: {
            struct pisces_xpmem_release_lcall release_lcall;
            struct pisces_xpmem_release_lcall * release_lcall_resp = NULL;

            if (copy_from_user(&(release_lcall.pisces_release), (void *)arg,
                        sizeof(struct pisces_xpmem_release))) {
                printk(KERN_ERR "Error copying buffer from userspace\n");
                ret = -EFAULT;
                break;
            }

            release_lcall.lcall.lcall = PISCES_LCALL_XPMEM_RELEASE;
            release_lcall.lcall.data_len = sizeof(struct pisces_xpmem_release_lcall) -
                        sizeof(struct pisces_lcall);

            status = pisces_lcall_exec((struct pisces_lcall *)&release_lcall,
                        (struct pisces_lcall_resp **)&release_lcall_resp);

            if (status != 0 || release_lcall_resp->lcall_resp.status != 0) {
                ret = -EFAULT;
                break;
            }

            if (copy_to_user((void *)arg, &(release_lcall_resp->pisces_release),
                        sizeof(struct pisces_xpmem_release))) {
                printk(KERN_ERR "Error copying buffer to userspace\n");
                ret = -EFAULT;
            }

            kmem_free(release_lcall_resp);
            break;
        }
        case ENCLAVE_IOCTL_XPMEM_ATTACH: {
            struct pisces_xpmem_attach_lcall attach_lcall;
            struct pisces_xpmem_attach_lcall * attach_lcall_resp = NULL;

            if (copy_from_user(&(attach_lcall.pisces_attach), (void *)arg,
                        sizeof(struct pisces_xpmem_attach))) {
                printk(KERN_ERR "Error copying buffer from userspace\n");
                ret = -EFAULT;
                break;
            }

            attach_lcall.lcall.lcall = PISCES_LCALL_XPMEM_ATTACH;
            attach_lcall.lcall.data_len = sizeof(struct pisces_xpmem_attach_lcall) -
                        sizeof(struct pisces_lcall);

            status = pisces_lcall_exec((struct pisces_lcall *)&attach_lcall,
                        (struct pisces_lcall_resp **)&attach_lcall_resp);

            if (status != 0 || attach_lcall_resp->lcall_resp.status != 0) {
                ret = -EFAULT;
                break;
            }

            attach_lcall_resp->pisces_attach.vaddr = pisces_map_xpmem_pfn_range(
                        (struct xpmem_pfn *)attach_lcall_resp->pfns,
                        attach_lcall_resp->num_pfns);

            if ((long long)attach_lcall_resp->pisces_attach.vaddr <= 0) {
                printk(KERN_ERR "Cannot handle xpmem attach request - "
                        "failed to map pfn list\n");
                ret = -ENOMEM;
                break;
            }

            if (copy_to_user((void *)arg, &(attach_lcall_resp->pisces_attach),
                        sizeof(struct pisces_xpmem_attach))) {
                printk(KERN_ERR "Error copying buffer to userspace\n");
                ret = -EFAULT;
            }

            kmem_free(attach_lcall_resp);
            break;
        }
        case ENCLAVE_IOCTL_XPMEM_DETACH: {
            struct pisces_xpmem_detach_lcall detach_lcall;
            struct pisces_xpmem_detach_lcall * detach_lcall_resp = NULL;

            if (copy_from_user(&(detach_lcall.pisces_detach), (void *)arg,
                        sizeof(struct pisces_xpmem_detach))) {
                printk(KERN_ERR "Error copying buffer from userspace\n");
                ret = -EFAULT;
                break;
            }

            detach_lcall.lcall.lcall = PISCES_LCALL_XPMEM_DETACH;
            detach_lcall.lcall.data_len = sizeof(struct pisces_xpmem_detach_lcall) -
                        sizeof(struct pisces_lcall);

            status = pisces_lcall_exec((struct pisces_lcall *)&detach_lcall,
                        (struct pisces_lcall_resp **)&detach_lcall_resp);

            if (status != 0 || detach_lcall_resp->lcall_resp.status != 0) {
                ret = -EFAULT;
                break;
            }

            if (copy_to_user((void *)arg, &(detach_lcall_resp->pisces_detach),
                        sizeof(struct pisces_xpmem_detach))) {
                printk(KERN_ERR "Error copying buffer to userspace\n");
                ret = -EFAULT;
            }

            kmem_free(detach_lcall_resp);
            break;
        }
        default: {
            printk(KERN_ERR "Unhandled Portals ioctl (%d)\n", ioctl);
            ret = -EINVAL;
            break;
        }
    }

    return ret;
}


static struct kfs_fops
pisces_portals_fops = 
{
    .open =             portals_open,
    .close =            portals_close,
    .read =             portals_read,
    .write =            portals_write,
    .unlocked_ioctl =   portals_ioctl,
};


static int
pisces_portals_init(void)
{
    kfs_create("/pisces-portals",
            NULL,
            &pisces_portals_fops,
            0777,
            NULL,
            0);

    clear_ppe_resp();
    return 0;
}

DRIVER_INIT("kfs", pisces_portals_init);
