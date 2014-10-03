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


struct pisces_xpmem_state {
	/* xbuf for IPI-based communication */
	struct pisces_xbuf_desc       * xbuf_desc;
	int				connected;

	/* XPMEM kernel interface */
	xpmem_link_t			link;
	struct xpmem_partition_state  * part;
};



/* Incoming XPMEM command from enclave - copy it out and deliver to xpmem
 * partition */
static void
xpmem_ctrl_handler(u8	* data, 
		   u32	  data_len, 
		   void * priv_data)
{
	struct pisces_xpmem_state    * state	  = (struct pisces_xpmem_state *)priv_data;
	struct pisces_xpmem_cmd_ctrl * xpmem_ctrl = (struct pisces_xpmem_cmd_ctrl *)data;
	struct xpmem_cmd_ex	     * cmd	  = NULL;
	u64			       pfn_len	  = 0;
	int			       status	  = 0;


	/* This appears to not sleep */
	cmd = kmem_alloc(sizeof(struct xpmem_cmd_ex));
	if (!cmd) {
	    printk(KERN_ERR "Pisces XPMEM: out of memory\n");
	    status = -1;
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
		printk(KERN_ERR "Pisces XPMEM: out of memory\n");
		kmem_free(cmd);
		status = -1;
		goto out;
	    }

	    memcpy(cmd->attach.pfns, 
		   xpmem_ctrl->pfn_list, 
		   cmd->attach.num_pfns * sizeof(u64)
	    );
	}
    
 out:
	/* We want to signal the end of the xbuf communication as quickly as
	 * possible, because delivering the command may take some time */
	pisces_xbuf_complete(state->xbuf_desc, data, data_len);

	if (status == 0) {
	    xpmem_cmd_deliver(state->part, state->link, cmd);

	    if (pfn_len > 0) {
		kmem_free(cmd->attach.pfns);
	    }

	    kmem_free(cmd);
	}
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
	printk(KERN_ERR "Pisces XPMEM: cannot handle command: enclave channel not connected\n");
	return -1;
    }


    if (cmd->type == XPMEM_ATTACH_COMPLETE) {
	pfn_len = cmd->attach.num_pfns * sizeof(u64);
    }

    /* Allocate memory for xpmem lcall structure */
    lcall = kmem_alloc(sizeof(struct pisces_xpmem_cmd_lcall) + pfn_len);
    if (!lcall) {
	printk(KERN_ERR "Pisces XPMEM: out of memory\n");
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


int
pisces_xpmem_init(void)
{
	struct pisces_xpmem_state * state = kmem_alloc(sizeof(struct pisces_xpmem_state));

	if (!state) {
		return -ENOMEM;
	}

	memset(state, 0, sizeof(struct pisces_xpmem_state));

	/* Initialize xbuf channel */
	state->xbuf_desc = pisces_xbuf_server_init(
		(uintptr_t)__va(pisces_boot_params->xpmem_buf_addr),
		pisces_boot_params->xpmem_buf_size,
		xpmem_ctrl_handler, state, -1, 0);

	if (!state->xbuf_desc) {
		printk(KERN_ERR "Pisces XPMEM: could not initialize XPMEM xbuf channel\n");
		kmem_free(state);
		return -1;
	}

	/* We are connected */
	state->connected = 1;

	/* Get xpmem partition */
	state->part = xpmem_get_partition();
	if (!state->part) {
	    printk(KERN_ERR "Pisces XPMEM: cannot retrieve local XPMEM partition\n");
	    kmem_free(state);
	    return -1;
	}

	/* Add connection link for enclave */
	state->link = xpmem_add_connection(
		state->part,
		XPMEM_CONN_REMOTE,
		xpmem_cmd_fn,
		state);

	if (state->link <= 0) {
	    printk(KERN_ERR "Pisces XPMEM: cannot create XPMEM connection\n");
	    kmem_free(state);
	    return -1;
	}

	printk("Initialized Pisces XPMEM cross-enclave connection\n");

	return 0;
}
