#include <lwk/types.h>
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <lwk/aspace.h>
#include <lwk/poll.h>
#include <lwk/kthread.h>
#include <lwk/list.h>
#include <arch/uaccess.h>
#include <arch/pgtable.h>

#include <arch/pisces/pisces.h>
#include <arch/pisces/pisces_boot_params.h>
#include <arch/pisces/pisces_xbuf.h>
#include <arch/pisces/pisces_lcall.h>
#include <arch/pisces/pisces_pagemap.h>

#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_iface.h>
#include <lwk/xpmem/xpmem_extended.h>


extern struct pisces_boot_params * pisces_boot_params;

/* Longcall structure */
struct pisces_xpmem_cmd_lcall {
    union {
	struct pisces_lcall lcall;
	struct pisces_lcall_resp lcall_resp;
    } __attribute__((packed));

    struct xpmem_cmd_ex xpmem_cmd;
} __attribute__ ((packed));

struct pisces_xpmem_state {
    /* state lock */
    spinlock_t                      lock;

    /* kernel thread for xpmem commands */
    struct task_struct            * xpmem_thread;
    int                             thread_active;
    int				    thread_should_exit;

    /* waitq for kern thread */
    waitq_t                         waitq;

    /* active xbuf data */
    u8                            * data;

    /* xbuf for IPI-based communication */
    struct pisces_xbuf_desc       * xbuf_desc;
    int				    connected;

    /* XPMEM kernel interface */
    xpmem_link_t		    link;
    struct xpmem_partition_state  * part;
};


/* Incoming XPMEM command from enclave - wake up kthread */
static void
xpmem_ctrl_handler(u8	* data, 
		   u32	  data_len, 
		   void * priv_data)
{
    struct pisces_xpmem_state * state = (struct pisces_xpmem_state *)priv_data;

    state->thread_active = 1;
    state->data          = data;

    mb();
    waitq_wakeup(&(state->waitq));
}


/* Format and send the longcall */
static int
pisces_xpmem_lcall_send(struct pisces_xpmem_cmd_lcall * xpmem_lcall, 
			u64				size)
{
    struct pisces_xpmem_cmd_lcall * xpmem_lcall_resp = NULL;
    int	   		     status           = 0;

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
    struct pisces_xpmem_cmd_lcall   lcall;

    if (!state->connected) {
	XPMEM_ERR("Cannot handle command: enclave channel not connected\n");
	return -1;
    }

    /* Copy command */
    memcpy(&(lcall.xpmem_cmd), cmd, sizeof(struct xpmem_cmd_ex));

    /* Perform xbuf send */
    return pisces_xpmem_lcall_send(&lcall, sizeof(struct pisces_xpmem_cmd_lcall));
}

static int
map_domain_region(u64 pfn_pa,
                  u64 size)
{
    paddr_t paddr_start;
    paddr_t paddr_end;

    /* Align regions on 2MB boundaries */
    paddr_start = round_down(pfn_pa, LARGE_PAGE_SIZE);
    paddr_end   = round_up(pfn_pa + size, LARGE_PAGE_SIZE);

    if (arch_aspace_map_pmem_into_kernel(paddr_start, paddr_end) != 0) {
        XPMEM_ERR("Cannot map domain pfn list into kernel");
        return -1;
    }   

    /* Now we can __va() it */
    return 0;
}


static int
xpmem_thread_fn(void * private)
{
    struct pisces_xpmem_state * state  = (struct pisces_xpmem_state *)private;
    struct xpmem_cmd_ex         cmd;

    while (1) {
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

	state->thread_active = 0;
	mb();

        /* Copy the xbuf data out and free */
	memcpy(&cmd, (struct xpmem_cmd_ex *)state->data, sizeof(struct xpmem_cmd_ex));
	kmem_free(state->data);

	/* Xbuf now complete */
	pisces_xbuf_complete(state->xbuf_desc, NULL, 0);

	if (cmd.type == XPMEM_ATTACH) {
	    if (map_domain_region(cmd.attach.pfn_pa, (cmd.attach.num_pfns * sizeof(u32))) != 0) {
		XPMEM_ERR("Failed to map pfn list into kernel memory\n");
		continue;
	    }
	}

	/* Process command */
	if (xpmem_cmd_deliver(state->part, state->link, &cmd) != 0) {
	    XPMEM_ERR("Failed to deliver command\n");
	}
    }

    return 0;
}



int
pisces_xpmem_init(void)
{
    struct pisces_xpmem_state * state  = NULL;
    int                         status = 0;

    state = kmem_alloc(sizeof(struct pisces_xpmem_state));
    if (state == NULL) {
	return -ENOMEM;
    }

    memset(state, 0, sizeof(struct pisces_xpmem_state));

    /* Init state */
    spin_lock_init(&(state->lock));
    waitq_init(&(state->waitq));

    state->data               = 0;
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
