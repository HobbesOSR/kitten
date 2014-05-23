#include <lwk/types.h>
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <lwk/aspace.h>
#include <lwk/poll.h>
#include <arch/uaccess.h>

#include <arch/pisces/pisces.h>
#include <arch/pisces/pisces_boot_params.h>
#include <arch/pisces/pisces_xbuf.h>
#include <arch/pisces/pisces_lcall.h>
#include <arch/pisces/pisces_xpmem.h>
#include <arch/pisces/pisces_pagemap.h>


extern struct pisces_boot_params * pisces_boot_params;


/* We use the longcall channel to send requests/responses */
static int
pisces_xpmem_cmd_lcall(struct pisces_xpmem_cmd_lcall * xpmem_lcall, u64 size)
{
	struct pisces_xpmem_cmd_lcall * xpmem_lcall_resp = NULL;
	int status = 0;

	xpmem_lcall->lcall.lcall    = PISCES_LCALL_XPMEM_CMD_EX;
	xpmem_lcall->lcall.data_len = size - sizeof(struct pisces_lcall);

	status = pisces_lcall_exec((struct pisces_lcall       *)xpmem_lcall,
				   (struct pisces_lcall_resp **)&xpmem_lcall_resp);

	if ((status == 0) && 
	    (xpmem_lcall_resp->lcall_resp.status == 0)) {
		kmem_free(xpmem_lcall_resp);
	}

	return status;
}

static int
xpmem_open(struct inode * inodep, struct file * filp)
{
	filp->private_data = inodep->priv;
	return 0;
}

static int
xpmem_release(struct inode * inodep, struct file * filp)
{
	struct pisces_xpmem_state * state = filp->private_data;

	if (!state->initialized) {
		return -EBADF;
	}

	return 0;
}

static ssize_t
xpmem_read(struct file * filp, 
	   char __user * buffer, 
	   size_t        size, 
	   loff_t      * offp)
{
	struct pisces_xpmem_state * state = filp->private_data;
	struct xpmem_cmd_iter     * iter  = NULL;
	ssize_t       ret   = size;
	unsigned long flags = 0;

	if (!state->initialized) {
		return -EBADF;
	}

	if (size != sizeof(struct xpmem_cmd_ex)) {
		return -EINVAL;
	}

	spin_lock_irqsave(&(state->in_lock), flags);
	{
		if (list_empty(&(state->in_cmds))) {
			spin_unlock_irqrestore(&(state->in_lock), flags);
			return 0;
		}

		iter = list_first_entry(&(state->in_cmds), struct xpmem_cmd_iter, node);
		list_del(&(iter->node));
	}
	spin_unlock_irqrestore(&(state->in_lock), flags);

	if (copy_to_user(buffer, (void *)(iter->cmd), size)) {
		ret = -EFAULT;
	}

	kmem_free(iter->cmd);
	kmem_free(iter);
	return ret;
}

static ssize_t
xpmem_write(struct file       * filp, 
	    const char __user * buffer, 
	    size_t              size, 
	    loff_t            * offp)
{
	struct pisces_xpmem_state     * state = filp->private_data;
	struct pisces_xpmem_cmd_lcall * lcall = NULL;
	struct xpmem_cmd_ex cmd_ex;
	u64 pfn_len = 0;
	int status  = 0;
	ssize_t ret = size;

	if (!state->initialized) {
		return -EBADF;
	}

	if (size != sizeof(struct xpmem_cmd_ex)) {
		return -EINVAL;
	}

	if (copy_from_user((void *)&(cmd_ex), buffer, size)) {
		return -EFAULT;
	}

	if (cmd_ex.type == XPMEM_ATTACH_COMPLETE) {
		pfn_len = cmd_ex.attach.num_pfns * sizeof(u64);
	}

	switch (cmd_ex.type) {
        case XPMEM_MAKE:
        case XPMEM_REMOVE:
        case XPMEM_GET:
        case XPMEM_RELEASE:
        case XPMEM_ATTACH:
        case XPMEM_DETACH:
        case XPMEM_GET_COMPLETE:
        case XPMEM_RELEASE_COMPLETE:
        case XPMEM_ATTACH_COMPLETE:
        case XPMEM_DETACH_COMPLETE:

		lcall = kmem_alloc(sizeof(struct pisces_xpmem_cmd_lcall) + pfn_len);

		if (!lcall) {
			return -ENOMEM;
		}

		lcall->xpmem_cmd = cmd_ex;

		if (cmd_ex.type == XPMEM_ATTACH_COMPLETE) {
			if (pfn_len > 0) {
				memcpy(lcall->pfn_list, cmd_ex.attach.pfns, cmd_ex.attach.num_pfns * sizeof(u64));
				kmem_free(cmd_ex.attach.pfns);
			}
		}

		status = pisces_xpmem_cmd_lcall(lcall, sizeof(struct pisces_xpmem_cmd_lcall) + pfn_len);

		if (status) {
			ret = -EFAULT;
		}

		break;
        default:
		ret = -EINVAL;
		break;
	}

	kmem_free(lcall);
	return ret;
}

static unsigned int
xpmem_poll(struct file              * filp,
	   struct poll_table_struct * pollp)
{
	struct pisces_xpmem_state * state = filp->private_data;
	unsigned int  ret = 0;
	unsigned long flags;

	if (!state->initialized) {
		return -EBADF;
	}

	ret = POLLOUT | POLLWRNORM;

	poll_wait(filp, &(state->user_waitq), pollp);

	spin_lock_irqsave(&(state->in_lock), flags);
	{
		if (!(list_empty(&(state->in_cmds)))) {
			ret |= (POLLIN | POLLRDNORM);
		}
	}
	spin_unlock_irqrestore(&(state->in_lock), flags);

	return ret;
}

static struct kfs_fops
xpmem_fops = 
	{
		.open       = xpmem_open,
		.release    = xpmem_release,
		.read       = xpmem_read,
		.write      = xpmem_write,
		.poll       = xpmem_poll
	};


static void
xpmem_handler(u8 * data, u32 data_len, void * priv_data)
{
	struct pisces_xpmem_state    * state      = (struct pisces_xpmem_state *)priv_data;
	struct pisces_xpmem_cmd_ctrl * xpmem_ctrl = (struct pisces_xpmem_cmd_ctrl *)data;
	struct xpmem_cmd_ex          * cmd_ex     = &(xpmem_ctrl->xpmem_cmd);
	struct xpmem_cmd_iter        * iter       = NULL;
	unsigned long flags;

	if (!state->initialized) {
		goto out;
	}

	if ((data_len     != sizeof(struct pisces_xpmem_cmd_ctrl)) && 
	    (cmd_ex->type != XPMEM_ATTACH_COMPLETE)) {
		printk(KERN_ERR "Invalid data in XPMEM control channel (len: %d, expected: %d)\n",
		       (int)data_len, (int)sizeof(struct pisces_xpmem_cmd_ctrl));
		goto out;
	}

	/* This appears to not sleep */
	iter = kmem_alloc(sizeof(struct xpmem_cmd_iter));

	if (!iter) {
		goto out;
	}

	/* This appears to not sleep */
	iter->cmd = kmem_alloc(sizeof(struct xpmem_cmd_ex));

	if (!iter->cmd) {
		kmem_free(iter);
		goto out;
	}
    
	*iter->cmd = *(cmd_ex);

	if (iter->cmd->type == XPMEM_ATTACH_COMPLETE) {
		struct xpmem_cmd_attach_ex * cmd_attach  = &(cmd_ex->attach);
		struct xpmem_cmd_attach_ex * iter_attach = &(iter->cmd->attach);

		iter_attach->pfns = kmem_alloc(cmd_attach->num_pfns * sizeof(u64));

		if (!iter_attach->pfns) {
			kmem_free(iter->cmd);
			kmem_free(iter);
			goto out;
		}

		memcpy(iter_attach->pfns, xpmem_ctrl->pfn_list, cmd_attach->num_pfns * sizeof(u64));
	}
    
	spin_lock_irqsave(&(state->in_lock), flags);
	{
		list_add_tail(&(iter->node), &(state->in_cmds));
	}
	spin_unlock_irqrestore(&(state->in_lock), flags);

	__asm__ __volatile__("":::"memory");
	waitq_wakeup(&(state->user_waitq));

 out:
	pisces_xbuf_complete(state->xbuf_desc, data, data_len); 
}


static int
xpmem_state_init(void)
{
	struct pisces_xpmem_state * state = kmem_alloc(sizeof(struct pisces_xpmem_state));

	if (!state) {
		return -ENOMEM;
	}

	memset(state, 0, sizeof(struct pisces_xpmem_state));

	state->xbuf_desc = pisces_xbuf_server_init((uintptr_t)__va(pisces_boot_params->xpmem_buf_addr),
						   pisces_boot_params->xpmem_buf_size,
						   xpmem_handler, state, -1, 0);

	if (!state->xbuf_desc) {
		printk(KERN_ERR "Could not initialize XPMEM xbuf channel\n");
		kmem_free(state);
		return -1;
	}

	if (!kfs_create("/pisces-xpmem",
			NULL,
			&xpmem_fops,
			0777,
			state,
			sizeof(struct pisces_xpmem_state))) {
		printk(KERN_ERR "Could not create XPMEM kfs device file\n");
		kmem_free(state);
		return -1;
	}

	waitq_init(&(state->user_waitq));
	spin_lock_init(&(state->state_lock));
	spin_lock_init(&(state->in_lock));
	INIT_LIST_HEAD(&(state->in_cmds));

	state->initialized = 1;

	return 0;
}

DRIVER_INIT("kfs", xpmem_state_init);
