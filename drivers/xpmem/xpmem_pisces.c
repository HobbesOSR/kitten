/* 
 * XPMEM extensions for multiple domain support.
 *
 * This file implements XPMEM requests for local processes executing with
 * Pisces cross-enclave mapping support
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 */

#include <xpmem.h>
#include <xpmem_extended.h>

#include <lwk/poll.h>
#include <lwk/waitq.h>
#include <arch/uaccess.h>

/* These will pack commands into request structures and write them to the Pisces
 * command list */

static int
xpmem_make_pisces(struct xpmem_partition * part, xpmem_segid_t * segid_p)
{
    struct pisces_xpmem_state * state = part->pisces_state;
    struct xpmem_cmd_ex * cmd = &(state->cmd);

    if (!state->initialized) {
	return -1;
    }

    printk("MAKE_PISCES\n");

    while (mutex_lock_interruptible(&(state->mutex)));

    spin_lock(&(state->lock));
    cmd->type = XPMEM_MAKE;
    cmd->make.segid = *segid_p;
    state->requested = 1;
    state->complete = 0;
    spin_unlock(&(state->lock));

    waitq_wakeup(&(state->pisces_wq));
    wait_event_interruptible(state->client_wq, (state->complete == 1));

    spin_lock(&(state->lock));
    *segid_p = cmd->make.segid;
    spin_unlock(&(state->lock));

    mutex_unlock(&(state->mutex));

    return 0;
}

static int
xpmem_remove_pisces(struct xpmem_partition * part, xpmem_segid_t segid)
{
    struct pisces_xpmem_state * state = part->pisces_state;
    struct xpmem_cmd_ex * cmd = &(state->cmd);

    if (!state->initialized) {
	return -1;
    }

    printk("REMOVE_PISCES\n");

    while (mutex_lock_interruptible(&(state->mutex)));

    spin_lock(&(state->lock));
    cmd->type = XPMEM_REMOVE;
    cmd->remove.segid = segid;
    state->requested = 1;
    state->processed = 0;
    state->complete = 0;
    spin_unlock(&(state->lock));

    waitq_wakeup(&(state->pisces_wq));
    wait_event_interruptible(state->client_wq, (state->complete == 1));

    mutex_unlock(&(state->mutex));
    return 0;
}

static int
xpmem_get_pisces(struct xpmem_partition * part, xpmem_segid_t segid, int flags,
	int permit_type, u64 permit_value, xpmem_apid_t * apid_p)
{
    struct pisces_xpmem_state * state = part->pisces_state;
    struct xpmem_cmd_ex * cmd = &(state->cmd);

    if (!state->initialized) {
	return -1;
    }

    printk("GET_PISCES\n");

    while (mutex_lock_interruptible(&(state->mutex)));

    spin_lock(&(state->lock));
    cmd->type = XPMEM_GET;
    cmd->get.segid = segid;
    cmd->get.flags = flags;
    cmd->get.permit_type = permit_type;
    cmd->get.permit_value = permit_value;
    state->requested = 1;
    state->processed = 0;
    state->complete = 0;
    spin_unlock(&(state->lock));

    waitq_wakeup(&(state->pisces_wq));
    wait_event_interruptible(state->client_wq, (state->complete == 1));

    mutex_unlock(&(state->mutex));
    return 0;
}

static int
xpmem_release_pisces(struct xpmem_partition * part, xpmem_apid_t apid)
{
    struct pisces_xpmem_state * state = part->pisces_state;
    struct xpmem_cmd_ex * cmd = &(state->cmd);

    if (!state->initialized) {
	return -1;
    }

    printk("RELEASE_PISCES\n");

    while (mutex_lock_interruptible(&(state->mutex)));

    spin_lock(&(state->lock));
    cmd->type = XPMEM_RELEASE;
    cmd->release.apid = apid;
    state->requested = 1;
    state->processed = 0;
    state->complete = 0;
    spin_unlock(&(state->lock));

    waitq_wakeup(&(state->pisces_wq));
    wait_event_interruptible(state->client_wq, (state->complete == 1));

    mutex_unlock(&(state->mutex));
    return 0;
}

static int
xpmem_attach_pisces(struct xpmem_partition * part, xpmem_apid_t apid, off_t off,
	size_t size, u64 * vaddr)
{
    struct pisces_xpmem_state * state = part->pisces_state;
    struct xpmem_cmd_ex * cmd = &(state->cmd);
    u64 * pfns;
    u64 num_pfns;

    if (!state->initialized) {
	return -1;
    }

    printk("ATTACH_PISCES\n");

    while (mutex_lock_interruptible(&(state->mutex)));

    spin_lock(&(state->lock));
    cmd->type = XPMEM_ATTACH;
    cmd->attach.apid = apid;
    cmd->attach.off = off;
    cmd->attach.size = size;
    state->requested = 1;
    state->processed = 0;
    state->complete = 0;
    spin_unlock(&(state->lock));

    waitq_wakeup(&(state->pisces_wq));
    wait_event_interruptible(state->client_wq, (state->complete == 1));

    spin_lock(&(state->lock));
    num_pfns = cmd->attach.num_pfns;
    pfns = cmd->attach.pfns;

    if (!pfns || num_pfns == 0) {
	*vaddr = 0;
    } else {
	*vaddr = (u64)xpmem_map_pfn_range(pfns, num_pfns);
	kmem_free(pfns);
    }

    spin_unlock(&(state->lock));
    mutex_unlock(&(state->mutex));
    return 0;
}

static int
xpmem_detach_pisces(struct xpmem_partition * part, u64 vaddr)
{
    struct pisces_xpmem_state * state = part->pisces_state;
    struct xpmem_cmd_ex * cmd = &(state->cmd);

    if (!state->initialized) {
	return -1;
    }

    printk("DETACH_PISCES\n");

    while (mutex_lock_interruptible(&(state->mutex)));

    spin_lock(&(state->lock));
    cmd->type = XPMEM_DETACH;
    cmd->detach.vaddr = vaddr;
    state->requested = 1;
    state->processed = 0;
    state->complete = 0;
    spin_unlock(&(state->lock));

    waitq_wakeup(&(state->pisces_wq));
    wait_event_interruptible(state->client_wq, (state->complete == 1));

    mutex_unlock(&(state->mutex));
    return 0;
}


struct xpmem_extended_ops 
pisces_ops = {
    .make	= xpmem_make_pisces,
    .remove	= xpmem_remove_pisces,
    .get	= xpmem_get_pisces,
    .release	= xpmem_release_pisces,
    .attach	= xpmem_attach_pisces,
    .detach	= xpmem_detach_pisces,
};


int
xpmem_pisces_init(struct xpmem_partition * part)
{
    part->pisces_state = kmem_alloc(sizeof(struct pisces_xpmem_state));
    if (!part->pisces_state) {
	return -1;
    }

    spin_lock_init(&(part->pisces_state->lock));
    mutex_init(&(part->pisces_state->mutex));
    waitq_init(&(part->pisces_state->client_wq));
    waitq_init(&(part->pisces_state->pisces_wq));

    part->pisces_state->initialized = 1;

    return 0;
}

int
xpmem_pisces_deinit(struct xpmem_partition * part) 
{
    if (!part) {
	return 0;
    }

    if (part->pisces_state) {
	kmem_free(part->pisces_state);
    }

    part->pisces_state = NULL;
    return 0;
}

static int
local_open(struct inode * inodep, struct file * filp)
{
    filp->private_data = inodep->priv;
    return 0;
}

static int
local_release(struct inode * inodep, struct file * filp)
{
    struct pisces_xpmem_state * pisces_state = (struct pisces_xpmem_state *)filp->private_data;

    if (!pisces_state || !pisces_state->initialized) {
	return -EBADF;
    }

    return 0;
}

static ssize_t
local_read(struct file * filp, char __user * buffer, size_t size, loff_t * offp)
{
    struct pisces_xpmem_state * pisces_state = (struct pisces_xpmem_state *)filp->private_data;

    if (!pisces_state || !pisces_state->initialized) {
        return -EBADF;
    }

    if (size != sizeof(struct xpmem_cmd_ex)) {
        return -EINVAL;
    }

    spin_lock(&(pisces_state->lock));
    if ((!pisces_state->requested) || (pisces_state->processed)) {
        spin_unlock(&(pisces_state->lock));
        return 0;
    }

    if (copy_to_user(buffer, (void *)&(pisces_state->cmd), size)) {
        spin_unlock(&(pisces_state->lock));
        return -EFAULT;
    }

    pisces_state->processed = 1;
    spin_unlock(&(pisces_state->lock));
    return size;
}

static ssize_t
local_write(struct file * filp, const char __user * buffer, size_t size, loff_t * offp)
{
    struct pisces_xpmem_state * pisces_state = (struct pisces_xpmem_state *)filp->private_data;
    ssize_t ret = size;

    if (!pisces_state || !pisces_state->initialized) {
        return -EBADF;
    }

    if (size != sizeof(struct xpmem_cmd_ex)) {
        return -EINVAL;
    }

    spin_lock(&(pisces_state->lock));
    if ((!pisces_state->requested || !pisces_state->processed)) {
        printk(KERN_ERR "XPMEM local channel not writeable - no requests in process\n");
        return 0;
    }

    if (copy_from_user((void *)&(pisces_state->cmd), buffer, sizeof(struct xpmem_cmd_ex))) {
        return -EFAULT;
    }

    switch (pisces_state->cmd.type) {
	case XPMEM_MAKE_COMPLETE:
	case XPMEM_REMOVE_COMPLETE:
	case XPMEM_GET_COMPLETE:
	case XPMEM_RELEASE_COMPLETE:
	case XPMEM_ATTACH_COMPLETE:
	case XPMEM_DETACH_COMPLETE:
	    pisces_state->complete = 1;
	    pisces_state->requested = 0;
	    pisces_state->processed = 0;
	    waitq_wakeup(&(pisces_state->client_wq));
	    break;

	default:
	    printk(KERN_ERR "Invalid local XPMEM write: %d\n", pisces_state->cmd.type);
	    ret = -EINVAL;
    }

    spin_unlock(&(pisces_state->lock));
    return ret;
}

static unsigned int
local_poll(struct file * filp, struct poll_table_struct * pollp)
{
    unsigned int ret = 0;
    struct pisces_xpmem_state * pisces_state = (struct pisces_xpmem_state *)filp->private_data;

    if (!pisces_state || !pisces_state->initialized) {
	return -EBADF;
    }

    poll_wait(filp, &(pisces_state->pisces_wq), pollp);

    spin_lock(&(pisces_state->lock));
    if (pisces_state->requested) {
	if (pisces_state->processed) {
	    ret = POLLOUT | POLLWRNORM;
	} else {
	    ret = POLLIN | POLLRDNORM;
	}
    }
    spin_unlock(&(pisces_state->lock));

    return ret;
}

static struct kfs_fops
local_fops = {
    .open	= local_open,
    .release	= local_release,
    .read	= local_read,
    .write	= local_write,
    .poll	= local_poll
};



static int
remote_open(struct inode * inodep, struct file * filp)
{
    filp->private_data = inodep->priv;
    return 0;
}

static int
remote_release(struct inode * inodep, struct file * filp)
{
    struct pisces_xpmem_state * pisces_state = (struct pisces_xpmem_state *)filp->private_data;

    if (!pisces_state || !pisces_state->initialized) {
	return -EBADF;
    }

    return 0;
}

static ssize_t
remote_read(struct file * filp, char __user * buffer, size_t size, loff_t * off_p)
{
    struct pisces_xpmem_state * pisces_state = (struct pisces_xpmem_state *)filp->private_data;

    if (!pisces_state || !pisces_state->initialized) {
	return -EBADF;
    }

    if (size != sizeof(struct xpmem_cmd_ex)) {
	return -EINVAL;
    }

    spin_lock(&(pisces_state->lock));
    if (!pisces_state->remote_requested) {
	spin_unlock(&(pisces_state->lock));
	return 0;
    }

    if (copy_to_user(buffer, (void *)&(pisces_state->remote_cmd), size)) {
	spin_unlock(&(pisces_state->lock));
	return -EFAULT;
    }

    pisces_state->remote_requested = 0;
    spin_unlock(&(pisces_state->lock));
    return size;
}

static ssize_t
remote_write(struct file * filp, const char __user * buffer, size_t size, loff_t * off_p)
{
    struct pisces_xpmem_state * pisces_state = (struct pisces_xpmem_state *)filp->private_data;
    struct xpmem_cmd_ex * cmd = NULL;
    ssize_t ret = size;
    int status;

    if (!pisces_state || !pisces_state->initialized) {
	return -EBADF;
    }

    if (size != sizeof(struct xpmem_cmd_ex)) {
	return -EINVAL;
    }

    spin_lock(&(pisces_state->lock));
    if (pisces_state->remote_requested) {
	printk(KERN_ERR "XPMEM: remote channel not writable - request already in process\n");
	return 0;
    }

    cmd = &(pisces_state->remote_cmd);
    if (copy_from_user((void *)cmd, buffer, sizeof(struct xpmem_cmd_ex))) {
	return -EFAULT;
    }

    switch (cmd->type) {
	case XPMEM_GET: {
	    status = xpmem_get_remote(&(cmd->get));
	    if (status) {
		cmd->get.apid = -1;
		ret = status;
	    }

	    pisces_state->remote_requested = 1;
	    cmd->type = XPMEM_GET_COMPLETE;
	    break;
	}

	case XPMEM_RELEASE: {
	    status = xpmem_release_remote(&(cmd->release));
	    if (status) {
		ret = status;
	    }

	    pisces_state->remote_requested = 1;
	    cmd->type = XPMEM_RELEASE_COMPLETE;
	    break;
	}

	case XPMEM_ATTACH: {
	    status = xpmem_attach_remote(&(cmd->attach));
	    if (status) {
		cmd->attach.num_pfns = 0;
		cmd->attach.pfns = NULL;
		ret = status;
	    }

	    printk("Created pfn list for remote XPMEM attach:\n");
	    {
	        u64 i;
	        for (i = 0; i < cmd->attach.num_pfns; i++) {
	    	    printk("%llu  \n", (unsigned long long)cmd->attach.pfns[i]);
		}
		printk("\n");
	    }

	    pisces_state->remote_requested = 1;
	    cmd->type = XPMEM_ATTACH_COMPLETE;
	    break;
	}

	case XPMEM_DETACH: {
	    status = xpmem_detach_remote(&(cmd->detach));
	    if (status) {
		ret = status;
	    }

	    pisces_state->remote_requested = 1;
	    cmd->type = XPMEM_DETACH_COMPLETE;
	    break;
	}

	default: {
	    printk(KERN_ERR "Invalid remote XPMEM write: %d\n", cmd->type);
	    ret = -EINVAL;
	    break;
	}
    }
	
    spin_unlock(&(pisces_state->lock));
    return ret;
}

static unsigned int
remote_poll(struct file * filp, struct poll_table_struct * pollp)
{
    unsigned int ret = 0;
    struct pisces_xpmem_state * pisces_state = (struct pisces_xpmem_state *)filp->private_data;

    if (!pisces_state || !pisces_state->initialized) {
	return -EBADF;
    }

    poll_wait(filp, &(pisces_state->pisces_wq), pollp);

    spin_lock(&(pisces_state->lock));
    if (pisces_state->remote_requested) {
	ret = POLLIN | POLLRDNORM;
    } else {
	ret = POLLOUT | POLLWRNORM;
    }
    spin_unlock(&(pisces_state->lock));

    return ret;
}

static struct kfs_fops
remote_fops = {
    .open	= remote_open,
    .release	= remote_release,
    .read	= remote_read,
    .write	= remote_write,
    .poll	= remote_poll
};


int
xpmem_local_connect(struct pisces_xpmem_state * state)
{
    int ret;

    if (!state || !state->initialized) {
	return -1;
    }

    ret = 0;
    spin_lock(&(state->lock));
    state->local_inodep = kfs_create("/xpmem-local",
	    NULL,
	    &local_fops,
	    0777,
	    state,
	    sizeof(struct pisces_xpmem_state));

    if (!state->local_inodep) {
	printk(KERN_ERR "XPMEM: Error creating local XPMEM channel! Is it already created?\n");
	ret = -1;
    }

    spin_unlock(&(state->lock));
    return ret;
}

int
xpmem_local_disconnect(struct pisces_xpmem_state * state)
{
    if (!state || !state->initialized || !state->local_inodep) {
	return -1;
    }

    kfs_destroy(state->local_inodep);
    return 0;
}


int
xpmem_remote_connect(struct pisces_xpmem_state * state)
{
    int ret;

    if (!state || !state->initialized) {
	return -1;
    }

    ret = 0;
    spin_lock(&(state->lock));
    state->remote_inodep = kfs_create("/xpmem-remote",
	    NULL,
	    &remote_fops,
	    0777,
	    state,
	    sizeof(struct pisces_xpmem_state));

    if (!state->remote_inodep) {
	printk(KERN_ERR "XPMEM: Error creating remote XPMEM channel! Is it alread created?\n");
	ret = -1;
    }

    spin_unlock(&(state->lock));
    return ret;
}

int
xpmem_remote_disconnect(struct pisces_xpmem_state * state)
{
    if (!state || !state->initialized || !state->remote_inodep) {
	return -1;
    }

    kfs_destroy(state->remote_inodep);
    return 0;
}
