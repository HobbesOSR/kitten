#include <lwk/types.h>
#include <lwk/kfs.h>
#include <lwk/driver.h>
#include <lwk/aspace.h>
#include <lwk/poll.h>
#include <lwk/kthread.h>
#include <lwk/list.h>
#include <arch/uaccess.h>
#include <arch/pgtable.h>
#include <arch/apic.h>

#include <arch/pisces/pisces.h>
#include <arch/pisces/pisces_boot_params.h>
#include <arch/pisces/pisces_xbuf.h>
#include <arch/pisces/pisces_lcall.h>
#include <arch/pisces/pisces_pagemap.h>

#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_iface.h>
#include <lwk/xpmem/xpmem_extended.h>

#define MAX_CMDS 16


extern struct pisces_boot_params * pisces_boot_params;

static int should_exit = 0;
DEFINE_SPINLOCK(exit_lock);

/* Longcall structure */
struct pisces_xpmem_cmd_lcall {
    union {
	struct pisces_lcall lcall;
	struct pisces_lcall_resp lcall_resp;
    } __attribute__((packed));

    struct xpmem_cmd_ex xpmem_cmd;
} __attribute__ ((packed));


struct xpmem_cmd_entry {
    struct xpmem_cmd_ex * cmd;
    int                   in_use;
};

struct xpmem_cmd_ringbuf {
    struct xpmem_cmd_entry entries[MAX_CMDS];
    int                    offset;
    spinlock_t             lock;
    atomic_t               active_entries;
};

struct pisces_xpmem_state {
    /* kernel thread for xpmem commands */
    struct task_struct	    * xpmem_thread;

    /* waitq for kern thread */
    waitq_t		      waitq;

    /* active xbuf data */
    struct xpmem_cmd_ringbuf  xpmem_buf;

    /* xbuf for IPI-based communication */
    struct pisces_xbuf_desc * xbuf_desc;

    /* XPMEM kernel interface */
    xpmem_link_t	      link;
};


static inline int next_offset(int off)
{
    return (off + 1) % MAX_CMDS;
}

static inline int prev_offset(int off)
{
    return (off == 0) ? MAX_CMDS - 1 : off - 1;
}

static int
push_active_entry(struct xpmem_cmd_ringbuf * buf,
                  struct xpmem_cmd_ex      * cmd)
{
    unsigned long flags = 0;
    int start, ret = -1;

    spin_lock_irqsave(&(buf->lock), flags);

    start = buf->offset;

    do {
	struct xpmem_cmd_entry * entry = &(buf->entries[buf->offset]);

	if (!entry->in_use) {
	    entry->in_use = 1;
	    entry->cmd    = cmd;
	    ret           = 0;
	    break;
	}

	buf->offset = next_offset(buf->offset);
    } while (buf->offset != start);

    spin_unlock_irqrestore(&(buf->lock), flags);

    return ret;
}

static void
pop_active_entry(struct xpmem_cmd_ringbuf * buf,
                 struct xpmem_cmd_ex      * cmd)
{
    unsigned long flags = 0;
    int start, ret = -1;

    spin_lock_irqsave(&(buf->lock), flags);

    start = buf->offset;

    do {
	struct xpmem_cmd_entry * entry = &(buf->entries[buf->offset]);

	if (entry->in_use) {
	    memcpy(cmd, entry->cmd, sizeof(struct xpmem_cmd_ex));
	    kmem_free(entry->cmd);
	    entry->in_use = 0;
	    ret           = 0;
	    break;
	}

	buf->offset = prev_offset(buf->offset);
    } while (buf->offset != start);

    spin_unlock_irqrestore(&(buf->lock), flags);

    /* We were kicked, but nothing is active */
    if (ret != 0)
	panic("XEMEM control channel BUG");
}

/* Incoming XPMEM command from enclave - enqueue and wake up kthread */
static void
xpmem_ctrl_handler(u8	* data, 
		   u32	  data_len, 
		   void * priv_data)
{
    struct pisces_xpmem_state * state = (struct pisces_xpmem_state *)priv_data;
    struct xpmem_cmd_ringbuf  * buf   = &(state->xpmem_buf);

    if (data_len != sizeof(struct xpmem_cmd_ex)) {
	printk(KERN_ERR "ERROR: DROPPING XPMEM CMD: MALFORMED CMD\n");
	return;
    }

    /* Find open entry */
    if (push_active_entry(buf, (struct xpmem_cmd_ex *)data) != 0) {
	printk(KERN_ERR "ERROR: DROPPING XPMEM CMD: NO ENTRIES AVAILABLE\n");
	return;
    }

    atomic_inc(&(buf->active_entries));
    mb();
    waitq_wakeup(&(state->waitq));

    /* Xbuf now complete */
    pisces_xbuf_complete(state->xbuf_desc, NULL, 0);
}


/* Format and send the longcall */
static int
pisces_xpmem_lcall_send(struct pisces_xpmem_cmd_lcall * xpmem_lcall, 
			u64				size)
{
    struct pisces_xpmem_cmd_lcall * xpmem_lcall_resp = NULL;
    int				    status	     = 0;

    xpmem_lcall->lcall.lcall	= PISCES_LCALL_XPMEM_CMD_EX;
    xpmem_lcall->lcall.data_len = size - sizeof(struct pisces_lcall);

    status = pisces_lcall_exec((struct pisces_lcall	  *)xpmem_lcall,
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
    struct pisces_xpmem_cmd_lcall lcall;

    /* Copy command */
    memcpy(&(lcall.xpmem_cmd), cmd, sizeof(struct xpmem_cmd_ex));

    /* Perform xbuf send */
    return pisces_xpmem_lcall_send(&lcall, sizeof(struct pisces_xpmem_cmd_lcall));
}

static int
xpmem_segid_fn(xpmem_segid_t segid,
               xpmem_sigid_t sigid,
	       xpmem_domid_t domid,
	       void        * priv_data)
{
    struct xpmem_signal * signal = (struct xpmem_signal *)&sigid;
    unsigned long irq_state;

    local_irq_save(irq_state);
    {
	lapic_send_ipi_to_apic(signal->apic_id, signal->vector);
    }
    local_irq_restore(irq_state);

    return 0;
}

static void
xpmem_kill_fn(void * priv_data)
{
    struct pisces_xpmem_state * state = (struct pisces_xpmem_state *)priv_data;
    unsigned long               flags;

    /* De-init xbuf server */
    pisces_xbuf_server_deinit(state->xbuf_desc);

    /* Kill kernel thread - it will free state when it exists */
    spin_lock_irqsave(&exit_lock, flags);
    {
	should_exit = 1;

	mb();
	waitq_wakeup(&(state->waitq));
    }
    spin_unlock_irqrestore(&exit_lock, flags);
}

static int
map_domain_region(u64 pfn_pa,
		  u64 size)
{
    paddr_t paddr_start;
    paddr_t paddr_end;

    /* Align regions on 2MB boundaries */
    paddr_start = round_down(pfn_pa, LARGE_PAGE_SIZE);
    paddr_end	= round_up(pfn_pa + size, LARGE_PAGE_SIZE);

    if (arch_aspace_map_pmem_into_kernel(paddr_start, paddr_end) != 0) {
	printk(KERN_ERR "Cannot map domain pfn list into kernel\n");
	return -1;
    }	

    /* Now we can __va() it */
    return 0;
}


static int
xpmem_thread_fn(void * private)
{
    struct pisces_xpmem_state * state  = (struct pisces_xpmem_state *)private;
    struct xpmem_cmd_ringbuf  * buf    = &(state->xpmem_buf);
    struct xpmem_cmd_ex         cmd;
    unsigned long               flags;

    while (1) {
	/* Wait on waitq */
	wait_event_interruptible(
	    state->waitq,
	     ((atomic_read(&(buf->active_entries)) > 0) ||
	      (should_exit                          == 1)
	     )
	);

	mb();
	if (should_exit == 1)
	    break;
	
	/* Get an active entry */
	pop_active_entry(buf, &cmd);
	atomic_dec(&(buf->active_entries));

	if (cmd.type == XPMEM_ATTACH) {
	    if (map_domain_region(cmd.attach.pfn_pa, (cmd.attach.num_pfns * sizeof(u32))) != 0) {
		printk(KERN_ERR "Cannot map domain pfn list into kernel\n");
		continue;
	    }
	}

	/* Process command */
	if (xpmem_cmd_deliver(state->link, &cmd) != 0) {
	    printk(KERN_ERR "Cannot deliver xpmem command\n");
	}
    }

    /* Acquire lock to prevent from freeing underneath the thread that woke us up
     */
    spin_lock_irqsave(&exit_lock, flags);
    {
	kmem_free(state);
    }
    spin_unlock_irqrestore(&exit_lock, flags);

    return 0;
}



xpmem_link_t
pisces_xpmem_init(void)
{
    struct pisces_xpmem_state * state  = NULL;
    int status = 0;

    state = kmem_alloc(sizeof(struct pisces_xpmem_state));
    if (state == NULL) {
	return -ENOMEM;
    }

    memset(state, 0, sizeof(struct pisces_xpmem_state));


    /* Init state */
    waitq_init(&(state->waitq));


    /* Init ringbuf */
    memset(&(state->xpmem_buf), 0, sizeof(struct xpmem_cmd_ringbuf));
    atomic_set(&(state->xpmem_buf.active_entries), 0);
    spin_lock_init(&(state->xpmem_buf.lock));

    /* Add connection link for enclave */
    state->link = xpmem_add_connection(
	    state,
	    xpmem_cmd_fn,
	    xpmem_segid_fn,
	    xpmem_kill_fn);

    if (state->link <= 0) {
	printk(KERN_ERR "Cannot create XPMEM connection\n");
	status = -EFAULT;
	goto err_connection;
    }

    /* Initialize xbuf channel */
    state->xbuf_desc = pisces_xbuf_server_init(
	    (uintptr_t)__va(pisces_boot_params->xpmem_buf_addr),
	    pisces_boot_params->xpmem_buf_size,
	    xpmem_ctrl_handler, state, -1, 0);

    if (!state->xbuf_desc) {
	printk(KERN_ERR "Cannot initialize xpmem xbuf channel\n");
	status = -EFAULT;
	goto err_xbuf;
    }

  
    /* Create kernel thread */
    state->xpmem_thread = kthread_run(
	    xpmem_thread_fn,
	    state,
	    "xpmem-thread"
    );

    if (state->xpmem_thread == NULL) {
	printk(KERN_ERR "Cannot create kernel thread\n");
	status = -EFAULT;
	goto err_thread;
    }

    pisces_xbuf_enable(state->xbuf_desc);

    printk("Initialized Pisces XPMEM cross-enclave connection\n");

    return state->link;

err_thread:
    pisces_xbuf_server_deinit(state->xbuf_desc);

err_xbuf:
    xpmem_remove_connection(state->link);

err_connection:
    kmem_free(state);

    return status;
}


void
pisces_xpmem_deinit(xpmem_link_t link)
{
    xpmem_remove_connection(link);
}
