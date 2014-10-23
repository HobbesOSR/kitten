#include <lwk/types.h>
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <lwk/aspace.h>
#include <lwk/poll.h>
#include <lwk/kthread.h>
#include <lwk/list.h>
#include <arch/uaccess.h>

#include <arch/pisces/pisces.h>
#include <arch/pisces/pisces_boot_params.h>
#include <arch/pisces/pisces_xbuf.h>
#include <arch/pisces/pisces_lcall.h>
#include <arch/pisces/pisces_pagemap.h>

#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_iface.h>
#include <lwk/xpmem/xpmem_extended.h>


extern struct pisces_boot_params * pisces_boot_params;

/* Longcall structures */
struct pisces_xpmem_cmd_lcall {
    union {
	struct pisces_lcall lcall;
	struct pisces_lcall_resp lcall_resp;
    } __attribute__((packed));

    struct xpmem_cmd_ex xpmem_cmd;
    u8		    pfn_list[0];
} __attribute__ ((packed));

/* Ctrl command structures */
struct pisces_xpmem_cmd_ctrl {
    struct xpmem_cmd_ex xpmem_cmd;
    u8		    pfn_list[0];
} __attribute__ ((packed));


struct xpmem_queue_entry {
    struct xpmem_cmd_ex * cmd;
    struct list_head      node;
};

struct pisces_xpmem_state {
    /* state lock */
    spinlock_t                      lock;

    /* kernel thread for xpmem commands */
    struct task_struct            * xpmem_thread;
    int                             thread_active;
    int				    thread_should_exit;

    /* waitq for kern thread */
    waitq_t                         waitq;

    /* Command queue */
    struct list_head                cmd_queue;

    /* xbuf for IPI-based communication */
    struct pisces_xbuf_desc       * xbuf_desc;
    int				    connected;

    /* XPMEM kernel interface */
    xpmem_link_t		    link;
    struct xpmem_partition_state  * part;
};



/* Incoming XPMEM command from enclave - copy it out and enqueue it */
static void
xpmem_ctrl_handler(u8	* data, 
		   u32	  data_len, 
		   void * priv_data)
{
    struct pisces_xpmem_state    * state      = (struct pisces_xpmem_state *)priv_data;
    struct pisces_xpmem_cmd_ctrl * xpmem_ctrl = (struct pisces_xpmem_cmd_ctrl *)data;
    struct xpmem_queue_entry     * entry      = NULL;
    struct xpmem_cmd_ex	         * cmd        = NULL;
    u64			           pfn_len    = 0;


    /* This appears to not sleep */
    cmd = kmem_alloc(sizeof(struct xpmem_cmd_ex));
    if (!cmd) {
	XPMEM_ERR("Pisces: out of memory\n");
	goto out;
    }

    /* Copy commands */
    *cmd = xpmem_ctrl->xpmem_cmd;

    /* Copy and pfn lists, if present */
    if (cmd->type == XPMEM_ATTACH_COMPLETE) {
	pfn_len = cmd->attach.num_pfns * sizeof(u64);
    }

    if (pfn_len > 0) {
	cmd->attach.pfns = kmem_alloc(pfn_len);
	if (!cmd->attach.pfns) {
	    XPMEM_ERR("Pisces: out of memory\n");
	    kmem_free(cmd);
	    goto out;
	}

	memcpy(cmd->attach.pfns, 
	       xpmem_ctrl->pfn_list, 
	       cmd->attach.num_pfns * sizeof(u64)
	);
    }

    /* Create queue entry */
    entry = kmem_alloc(sizeof(struct xpmem_queue_entry));
    if (entry == NULL) {

	if (pfn_len > 0) {
	    kmem_free(cmd->attach.pfns);
	}
	kmem_free(cmd);

	goto out;
    }

    entry->cmd = cmd;

    spin_lock(&(state->lock));
    {
	list_add_tail(&(entry->node), &(state->cmd_queue));
	state->thread_active = 1;
    }
    spin_unlock(&(state->lock));

    mb();
    waitq_wakeup(&(state->waitq));

out:
    /* Xbuf complete */
    pisces_xbuf_complete(state->xbuf_desc, NULL, 0);

    /* Free ctrl structure */
    kmem_free(data);
}


/* Format and send the longcall */
static int
pisces_xpmem_lcall_send(struct pisces_xpmem_cmd_lcall * xpmem_lcall, 
			u64				size)
{
    struct pisces_xpmem_cmd_lcall * xpmem_lcall_resp = NULL;
    int				status		 = 0;

    xpmem_lcall->lcall.lcall    = PISCES_LCALL_XPMEM_CMD_EX;
    xpmem_lcall->lcall.data_len = size - sizeof(struct pisces_lcall);

    status = pisces_lcall_exec((struct pisces_lcall       *)xpmem_lcall,
			       (struct pisces_lcall_resp **)&xpmem_lcall_resp);

    if (status == 0) {
	status = xpmem_lcall_resp->lcall_resp.status;
	kmem_free(xpmem_lcall_resp);
    }

    return status;
}

/* We use the Longcall channel to send commands */
static int
xpmem_cmd_fn(struct xpmem_cmd_ex * cmd,
	     void		 * priv_data)
{
    struct pisces_xpmem_state	  * state   = (struct pisces_xpmem_state *)priv_data;
    struct pisces_xpmem_cmd_lcall * lcall   = NULL;
    u64				    pfn_len = 0;  
    int				    status  = 0;

    if (!state->connected) {
	XPMEM_ERR("Cannot handle command: enclave channel not connected\n");
	return -1;
    }


    if (cmd->type == XPMEM_ATTACH_COMPLETE) {
	pfn_len = cmd->attach.num_pfns * sizeof(u64);
    }

    /* Allocate memory for xpmem lcall structure */
    lcall = kmem_alloc(sizeof(struct pisces_xpmem_cmd_lcall) + pfn_len);
    if (!lcall) {
	XPMEM_ERR("Out of memory\n");
	return -1;
    }

    /* Copy command */
    lcall->xpmem_cmd = *cmd;

    /* Copy pfn list */
    if (pfn_len > 0) {
	memcpy(lcall->pfn_list,
	       cmd->attach.pfns,
	       cmd->attach.num_pfns * sizeof(u64)
	);
    }

    /* Perform xbuf send */
    status = pisces_xpmem_lcall_send(lcall, sizeof(struct pisces_xpmem_cmd_lcall) + pfn_len);

    /* Free lcall structure */
    kmem_free(lcall);

    return status;
}


static int
xpmem_thread_fn(void * private)
{
    struct pisces_xpmem_state * state  = (struct pisces_xpmem_state *)private;
    struct xpmem_queue_entry  * entry  = NULL;
    struct xpmem_cmd_ex       * cmd    = NULL;
    unsigned long               flags  = 0;
    int                         type   = 0;
    int                         status = 0;

    while (1) {
	mb();

	spin_lock_irqsave(&(state->lock), flags);
	{
	    if (list_empty(&(state->cmd_queue))) {
		state->thread_active = 0;
		mb();
	    }
	}
	spin_unlock_irqrestore(&(state->lock), flags);

	/* Wait on waitq */
	wait_event_interruptible(
	        state->waitq,
		((state->thread_active      == 1) ||
		 (state->thread_should_exit == 1)
		)
	);
	
	mb();
	if (state->thread_should_exit == 1) {
	    break;
	}

	/* Dequeue command */
	spin_lock_irqsave(&(state->lock), flags);
	{
	    if (list_empty(&(state->cmd_queue))) {
		cmd   = NULL;
	    } else {

		entry = list_first_entry(&(state->cmd_queue),
			    struct xpmem_queue_entry,
			    node);
		cmd   = entry->cmd;

		list_del(&(entry->node));
		kmem_free(entry);
	    }
	}
	spin_unlock_irqrestore(&(state->lock), flags);

	if (cmd == NULL) {
	    XPMEM_ERR("Empty command queue...\n");
	    continue;
	}
	
	/* Save cmd type */
	type = cmd->type;

	/* Process command */
	status = xpmem_cmd_deliver(state->part, state->link, cmd);

	if (status != 0) {
	    XPMEM_ERR("Failed to deliver command\n");
	}

	if ((type                 == XPMEM_ATTACH_COMPLETE) &&
	    (cmd->attach.num_pfns  > 0))
	{
	    kmem_free(cmd->attach.pfns);
	}

	kmem_free(cmd);
    }

    return 0;
}



int
pisces_xpmem_init(void)
{
    struct pisces_xpmem_state * state  = NULL;
    int                         status = 0;

    state  = kmem_alloc(sizeof(struct pisces_xpmem_state));
    if (state == NULL) {
	return -ENOMEM;
    }

    memset(state, 0, sizeof(struct pisces_xpmem_state));

    /* Init state */
    spin_lock_init(&(state->lock));
    INIT_LIST_HEAD(&(state->cmd_queue));
    waitq_init(&(state->waitq));

    state->thread_active      = 0;
    state->thread_should_exit = 0;

    /* Initialize xbuf channel */
    state->xbuf_desc = pisces_xbuf_server_init(
	    (uintptr_t)__va(pisces_boot_params->xpmem_buf_addr),
	    pisces_boot_params->xpmem_buf_size,
	    xpmem_ctrl_handler, state, -1, 0);

    if (!state->xbuf_desc) {
	XPMEM_ERR("Could not initialize XPMEM xbuf channel\n");
	status = -EFAULT;
	goto err_xbuf;
    }

    /* Get xpmem partition */
    state->part = xpmem_get_partition();
    if (!state->part) {
	XPMEM_ERR("Cannot retrieve local XPMEM partition\n");
	status = -EFAULT;
	goto err_partition;
    }

    /* Add connection link for enclave */
    state->link = xpmem_add_connection(
	    state->part,
	    XPMEM_CONN_REMOTE,
	    xpmem_cmd_fn,
	    state);

    if (state->link <= 0) {
	XPMEM_ERR("Cannot create XPMEM connection\n");
	status = -EFAULT;
	goto err_connection;
    }

    /* Create kernel thread */
    state->xpmem_thread = kthread_run(
            xpmem_thread_fn,
	    state,
	    "xpmem-thread"
    );

    if (state->xpmem_thread == NULL) {
	XPMEM_ERR("Cannot create kernel thread\n");
	status = -EFAULT;
	goto err_thread;
    }

    /* We are connected */
    state->connected = 1;

    printk("Initialized Pisces XPMEM cross-enclave connection\n");

    return 0;

err_thread:
    xpmem_remove_connection(state->part, state->link);

err_connection:
err_partition:
    pisces_xbuf_server_deinit(state->xbuf_desc);

err_xbuf:
    kmem_free(state);

    return status;
}
