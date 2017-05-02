/*
 * XPMEM extensions for Palacios virtual machines
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 */
#include <lwk/cpuinfo.h>
#include <lwk/workq.h>
#include <lwk/pci/pci.h>
#include <lwk/pci/pcicfg_regs.h>

#include <arch/io.h>
#include <arch/fixmap.h>
#include <arch/apicdef.h>
#include <arch/io_apic.h>

#include <xpmem.h>
#include <xpmem_private.h>
#include <xpmem_iface.h>

#define XPMEM_VENDOR_ID     0xfff0
#define XPMEM_SUBVENDOR_ID  0xfff0
#define XPMEM_DEVICE_ID     0x100d


#define VMCALL      ".byte 0x0F,0x01,0xC1\r"
#define VMMCALL     ".byte 0x0F,0x01,0xD9\r"


extern struct cpuinfo cpu_info[NR_CPUS];


static int vmcall;


struct xpmem_cmd_bar_state {
    /* Hypercall ids */
    u32 xpmem_hcall_id;
    u32 xpmem_detach_hcall_id;
    u32 xpmem_irq_clear_hcall_id;
    u32 xpmem_read_cmd_hcall_id;
    u32 xpmem_read_apicid_hcall_id;
    u32 xpmem_request_irq_hcall_id;
    u32 xpmem_release_irq_hcall_id;
    u32 xpmem_deliver_irq_hcall_id;
    u32 xpmem_sig_clear_hcall_id;

    /* vmx capable */
    u8 vmx_capable;

    /* svm capable */
    u8 svm_capable;

    /* interrupt status */
    u8 irq_handled;

    /* size of command to read from device */
    u64 xpmem_cmd_size;

    /* size of pfn list to read from device */
    u64 xpmem_pfn_size;

    /* base gpa for xpmem attachments */
    u64 xpmem_gpa_base;

    /* size of xpmem gpa region */
    u64 xpmem_gpa_size;

    /* Running in a hobbes environment */
    u8 hobbes_enabled;

    /* Hobbes Enclave ID */
    u64 hobbes_enclave_id;
} __attribute__((packed));

struct xpmem_palacios_state {
    struct pci_dev           * pci_dev;         /* PCI device */

    pci_bar_t                  cmd_bar;		/* Cmd PCI Bar */
    void __iomem             * xpmem_cmd_bar;   /* Bar memory */
    struct xpmem_cmd_bar_state cmd_bar_state;   /* CMD Bar state */

    pci_bar_t                  sig_bar;         /* Sig PCI Bar */
    u8 __iomem		     * xpmem_sig_cnts;  /* Bar memory */
    spinlock_t                 sig_cnt_lock;    /* lock protecting sig counts */

    unsigned int               irq;             /* device irq number */
    struct work_struct         work;            /* work struct */

    int                        initialized;     /* state initialized */
    xpmem_link_t               link;            /* xpmem connection link */
};

static struct xpmem_palacios_state vmm_info;
static xpmem_link_t vmm_link = 0;
static DEFINE_SPINLOCK(vmm_link_lock);

/*
static void
xpmem_dump_bar(struct xpmem_cmd_bar_state * bar)
{
    printk("CMD BAR:\n"
	"\txpmem_hcall_id:              %u\n"
	"\txpmem_detach_hcall_id:       %u\n"
	"\txpmem_irq_clear_hcall_id:    %u\n"
	"\txpmem_read_cmd_hcall_id:     %u\n"
	"\txpmem_read_apicid_hcall_id:  %u\n"
	"\txpmem_request_irq_hcall_id:  %u\n"
	"\txpmem_release_irq_hcall_id:  %u\n"
	"\txpmem_deliver_irq_hcall_id:  %u\n"
	"\txpmem_sig_clear_hcall_id:    %u\n"
	"\tvmx_capable:                 %u\n"
	"\tsvm_capable:                 %u\n"
	"\tirq_handled:                 %u\n"
	"\txpmem_cmd_size:              %llu\n"
	"\txpmem_pfn_size:              %llu\n"
	"\txpmem_gpa_base:              0x%llx\n"
	"\txpmem_gpa_size:              %llu\n"
	"\thobbes_enabled:              %u\n",
	bar->xpmem_hcall_id,
	bar->xpmem_detach_hcall_id,
	bar->xpmem_irq_clear_hcall_id,
	bar->xpmem_read_cmd_hcall_id,
	bar->xpmem_read_apicid_hcall_id,
	bar->xpmem_request_irq_hcall_id,
	bar->xpmem_release_irq_hcall_id,
	bar->xpmem_deliver_irq_hcall_id,
	bar->xpmem_sig_clear_hcall_id,
	bar->vmx_capable,
	bar->svm_capable,
	bar->irq_handled,
	bar->xpmem_cmd_size,
	bar->xpmem_pfn_size,
	bar->xpmem_gpa_base,
	bar->xpmem_gpa_size,
	bar->hobbes_enabled);
}
*/

xpmem_link_t
xpmem_get_host_link(void)
{
    xpmem_link_t link;
    unsigned long flags;

    spin_lock_irqsave(&vmm_link_lock, flags);
    link = vmm_link;
    spin_unlock_irqrestore(&vmm_link_lock, flags);

    return link;
}

static void
xpmem_set_host_link(xpmem_link_t link)
{
    unsigned long flags;

    spin_lock_irqsave(&vmm_link_lock, flags);
    vmm_link = link;
    spin_unlock_irqrestore(&vmm_link_lock, flags);
}

static xpmem_link_t
xpmem_reset_host_link(void)
{
    xpmem_link_t  old_link;
    unsigned long flags;

    spin_lock_irqsave(&vmm_link_lock, flags);
    old_link = vmm_link;
    vmm_link = 0;
    spin_unlock_irqrestore(&vmm_link_lock, flags);

    return old_link;
}


static unsigned long long
do_xpmem_hcall(u32       hcall_id,
               uintptr_t arg1,
               uintptr_t arg2,
               uintptr_t arg3)
{
    unsigned long long ret = 0;

    if (vmcall) {
	__asm__ volatile(
	    VMCALL
	    : "=a"(ret)
	    : "a"(hcall_id), "b"(arg1), "c"(arg2), "d"(arg3)
	);
    } else {
	__asm__ volatile(
	    VMMCALL
	    : "=a"(ret)
	    : "a"(hcall_id), "b"(arg1), "c"(arg2), "d"(arg3)
	);
    }

    return ret;
}

static void
xpmem_cmd_hcall(struct xpmem_palacios_state * state, 
                struct xpmem_cmd_ex         * cmd)
{
    (void)do_xpmem_hcall(state->cmd_bar_state.xpmem_hcall_id, (uintptr_t)cmd, 0, 0);
}

static void
xpmem_detach_hcall(struct xpmem_palacios_state * state,
                   u64                           paddr)
{
    (void)do_xpmem_hcall(state->cmd_bar_state.xpmem_detach_hcall_id, (uintptr_t)paddr, 0, 0);
}

static void
xpmem_irq_clear_hcall(struct xpmem_palacios_state * state,
		      uint16_t                      idt_vec)
{
    (void)do_xpmem_hcall(state->cmd_bar_state.xpmem_irq_clear_hcall_id, idt_vec, 0, 0);
}

static void
xpmem_read_cmd_hcall(struct xpmem_palacios_state * state,
                     u64 cmd_va,
                     u64 pfn_va)
{
    (void)do_xpmem_hcall(state->cmd_bar_state.xpmem_read_cmd_hcall_id, (uintptr_t)cmd_va, (uintptr_t)pfn_va, 0);
}

static void
xpmem_read_apicid_hcall(struct xpmem_palacios_state * state,
                        unsigned int                  logical_cpu,
                        int                         * host_apicid_va)
{
    (void)do_xpmem_hcall(state->cmd_bar_state.xpmem_read_apicid_hcall_id, (uintptr_t)logical_cpu, (uintptr_t)host_apicid_va, 0);
}

static void
xpmem_request_irq_hcall(struct xpmem_palacios_state * state,
                        int                           guest_vector,
                        int                         * host_vector_va)
{
    (void)do_xpmem_hcall(state->cmd_bar_state.xpmem_request_irq_hcall_id, (uintptr_t)guest_vector, (uintptr_t)host_vector_va, 0);
}

static void
xpmem_release_irq_hcall(struct xpmem_palacios_state * state,
                        int                           host_vector)
{
    (void)do_xpmem_hcall(state->cmd_bar_state.xpmem_release_irq_hcall_id, (uintptr_t)host_vector, 0, 0);
}

static void
xpmem_deliver_irq_hcall(struct xpmem_palacios_state * state,
                        xpmem_domid_t                 segid,
                        xpmem_sigid_t                 sigid,
                        xpmem_domid_t                 domid)
{
    (void)do_xpmem_hcall(state->cmd_bar_state.xpmem_deliver_irq_hcall_id, (uintptr_t)segid, (uintptr_t)sigid, (uintptr_t)domid);
}

static void
xpmem_sig_clear_hcall(struct xpmem_palacios_state * state,
                      int                           guest_vector,
                      uint8_t                       cnt)
{
    (void)do_xpmem_hcall(state->cmd_bar_state.xpmem_sig_clear_hcall_id, (uintptr_t)guest_vector, (uintptr_t)cnt, 0);
}


static void
read_bar(void __iomem * xpmem_bar, 
         void         * dst, 
         u64            len)
{
    u32 i = 0;
    for (i = 0; i < len; i++) {
	*((u8 *)(dst + i)) = *((u8 *)(xpmem_bar + i));
    }
}

/* 
 * Work queue for interrupt processing.
 *
 * xpmem_cmd_deliver could sleep, so we can't do this directly from the
 * interrupt handler
 */
void
__xpmem_work_fn(struct xpmem_palacios_state * state)
{
    u64   cmd_size = 0;
    u64   pfn_size = 0;
    u64 * pfn_buf  = NULL;

    struct xpmem_cmd_ex cmd;

    /* Read BAR header */
    read_bar(state->xpmem_cmd_bar, 
             (void *)&(state->cmd_bar_state), 
             sizeof(state->cmd_bar_state));

    /* Grab size fields */
    cmd_size = state->cmd_bar_state.xpmem_cmd_size;
    pfn_size = state->cmd_bar_state.xpmem_pfn_size;

    /* Could be a spurious IRQ */
    if (cmd_size != sizeof(struct xpmem_cmd_ex)) {
        return;
    }

    if (pfn_size > 0) {
        pfn_buf = kmem_alloc(pfn_size);
        if (!pfn_buf) {
            return;
        }
    }

    /* Read command from BAR */
    xpmem_read_cmd_hcall(
        state, 
        (u64)(void *)&cmd,
        (u64)__pa(pfn_buf)
    );

    /* Save pointer to pfn list */
    if (pfn_size > 0) {
        cmd.attach.pfn_list_pa = (u64)__pa(pfn_buf);
    }

    /* Clear device interrupt flag */
    xpmem_irq_clear_hcall(state, state->irq);

    /* Deliver the command */
    xpmem_cmd_deliver(state->link, &cmd);
}


void 
xpmem_work_fn(struct work_struct * work)
{
    struct xpmem_palacios_state * state = NULL;

    state = container_of(work, struct xpmem_palacios_state, work);

    __xpmem_work_fn(state);

    xpmem_put_link_data(state->link);
}


/*
 * Interrupt handler for Palacios XPMEM device.
 */
static irqreturn_t 
irq_handler(int    irq, 
            void * data)
{
    /* We can access the state directly, because the conn will not go away as
     * long as the irq handler is still active. However, we need an additional
     * ref to protect the work queue which we are about to schedule
     */
    struct xpmem_palacios_state * state = (struct xpmem_palacios_state *)data; 

    /* The irq can fire as a result of free_irq(), which is invoked when the 
     * conn is killed, so the link data won't exist
     */
    if (!xpmem_get_link_data(state->link))
        return IRQ_NONE;

    schedule_work(&(state->work));
    return IRQ_HANDLED;
}



/* Callback for commands being issued by the XPMEM name/forwarding service */
static int
xpmem_cmd_fn(struct xpmem_cmd_ex * cmd, 
             void                * priv_data)
{
    struct xpmem_palacios_state * state = (struct xpmem_palacios_state *)priv_data;
    xpmem_pfn_range_t           * range = NULL;

    /* Invoke hypercall */
    xpmem_cmd_hcall(state, cmd);

    if (cmd->type == XPMEM_ATTACH_COMPLETE) {
        range = __va(cmd->attach.pfn_list_pa);
        kmem_free(range);
    }

    return 0;
}

/* Callback for signals issued to a segid */
static int
xpmem_segid_fn(xpmem_segid_t segid,
               xpmem_sigid_t sigid,
               xpmem_domid_t domid,
               void        * priv_data)
{
    struct xpmem_palacios_state * state = (struct xpmem_palacios_state *)priv_data;

    /* Invoke hypercall */
    xpmem_deliver_irq_hcall(state, segid, sigid, domid);

    return 0;
}

static void
xpmem_kill_fn(void * priv_data)
{
    struct xpmem_palacios_state * state = priv_data;

    /* Free irq */
    irq_free(state->irq, (void *)state);
    
#if 0
    /* Disable the pci device */
    pci_disable_device(state->pci_dev);

    /* Flush the work queue */
    flush_scheduled_work();

    /* Unmap bars */
    pci_iounmap(state->pci_dev, state->xpmem_cmd_bar);
    pci_iounmap(state->pci_dev, state->xpmem_sig_cnts);
#endif

    state->initialized = 0;
}


static const struct pci_device_id 
xpmem_ids[] =
{
    { 
	.vendor_id = XPMEM_VENDOR_ID,
	.device_id = XPMEM_DEVICE_ID,
	0, 0, 0, 0
    },
    { 0 }
};

static int 
xpmem_probe_driver(struct pci_dev             * dev, 
                   const struct pci_device_id * id)                   
{
    struct xpmem_palacios_state * state    = NULL;
    int irq_vector, ret;
    u16 cmd;

    printk("Probing XPMEM driver\n");

    if (dev->vendor != XPMEM_VENDOR_ID)
        return -1;

    if (dev->device != XPMEM_DEVICE_ID)
        return -1;

    state = &vmm_info;
    if (state->initialized)
        return -EBUSY;

    /* Map the BARs. Decoding the config space issues writes to the config
     * space, which trigger Palacios to map them into our address space at the
     * BIOS-issued address
     */

    /* Map BAR 0 (CMD BAR) */
    pcicfg_bar_decode(&(dev->cfg), 0, &(state->cmd_bar));
    state->xpmem_cmd_bar = __va(state->cmd_bar.address);

    /* Map BAR 1 (SIG BAR) */
    pcicfg_bar_decode(&(dev->cfg), 1, &(state->sig_bar));
    state->xpmem_sig_cnts = __va(state->sig_bar.address);

    /* Enable MMIO */
    cmd = pci_read(dev, PCIR_COMMAND, 2);
    if (!(cmd & PCIM_CMD_MEMEN)) {
	printk("XPMEM PCI device MEM resources disabled: enabling now\n");
	pci_mmio_enable(dev);

	cmd = pci_read(dev, PCIR_COMMAND, 2);
	if (!(cmd & PCIM_CMD_MEMEN)) {
	    XPMEM_ERR("Cannot enable MEM resources in XPMEM PCI device\n");
	    return -ENODEV;
	}
    }

    /* Enable INTx */
    cmd = pci_read(dev, PCIR_COMMAND, 2);
    if (cmd & PCIM_CMD_INTxDIS) {
	printk("PCI device INTx disabled: enabling now\n");
	pci_intx_enable(dev);

	cmd = pci_read(dev, PCIR_COMMAND, 2);
	if (cmd & PCIM_CMD_INTxDIS) {
	    XPMEM_ERR("Cannot enable INTx in XPMEM PCI device\n");
	    return -ENODEV;
	}
    }

    /* Enable PCI bus mastering */
    cmd = pci_read(dev, PCIR_COMMAND, 2);
    if (!(cmd & PCIM_CMD_BUSMASTEREN)) {
	printk("PCI device BUS master disabled: enabling now\n");
	pci_dma_enable(dev);

	cmd = pci_read(dev, PCIR_COMMAND, 2);
	if (!(cmd & PCIM_CMD_BUSMASTEREN)) {
	    XPMEM_ERR("Cannot enable BUS master in XPMEM PCI device\n");
	    return -ENODEV;
	}
    }

    spin_lock_init(&(state->sig_cnt_lock));

    /* Read Palacios hypercall ids from BAR 0 */
    read_bar(state->xpmem_cmd_bar, 
             (void *)&(state->cmd_bar_state), 
             sizeof(state->cmd_bar_state));

    if ( (state->cmd_bar_state.xpmem_hcall_id              == 0) ||
         (state->cmd_bar_state.xpmem_detach_hcall_id       == 0) ||
         (state->cmd_bar_state.xpmem_irq_clear_hcall_id    == 0) ||
         (state->cmd_bar_state.xpmem_read_cmd_hcall_id     == 0) ||
         (state->cmd_bar_state.xpmem_read_apicid_hcall_id  == 0) ||
         (state->cmd_bar_state.xpmem_request_irq_hcall_id  == 0) ||
         (state->cmd_bar_state.xpmem_release_irq_hcall_id  == 0) ||
         (state->cmd_bar_state.xpmem_deliver_irq_hcall_id  == 0)
       )
    {
        XPMEM_ERR("Palacios hypercall(s) not available");
        ret = -ENODEV;
        goto err;
    }

    if ( (state->cmd_bar_state.vmx_capable == 0) &&
         (state->cmd_bar_state.svm_capable == 0))
    {
        XPMEM_ERR("Palacios hypercall(s) not functional");
        ret = -ENODEV;
        goto err;
    }

    if (state->cmd_bar_state.vmx_capable > 0) {
        vmcall = 1;
    } else {
        vmcall = 0;
    }

    /* Initialize the rest of the state */
    INIT_WORK(&(state->work), xpmem_work_fn);

#if 0
    /* IRQ handler */
    irq_vector = ioapic_pcidev_vector(dev->cfg.bus, dev->cfg.slot, 0);
    if (irq_vector == -1) {
	XPMEM_ERR("Failed to find interrupt vector for XPMEM PCI device\n");
	ret = -ENODEV;
	goto err;
    }

    ret = irq_request(irq_vector, irq_handler, 0, "xpmem", (void *)state);
#else
    ret = irq_vector = irq_request_free_vector(irq_handler, 0, "xpmem", (void *)state);
#endif

    if (ret < 0) {
        XPMEM_ERR("Failed to request IRQ for Palacios device (irq = %d)", ret);
        goto err;
    }

    state->irq = irq_vector;

    /* Add connection to name/forwarding service */
    state->link = xpmem_add_connection(
            (void *)state,
            xpmem_cmd_fn, 
            xpmem_segid_fn,
            xpmem_kill_fn);

    ret = state->link;
    if (ret < 0) {
        XPMEM_ERR("Failed to register Palacios interface with name/forwarding service");
        goto err_connection;
    }

    /* Save vmm link */
    xpmem_set_host_link(state->link);

    /* Remember the device and signal initialization */
    state->pci_dev     = dev;
    state->initialized = 0;

    /* Signal device initialization by clearing irq status */
    xpmem_irq_clear_hcall(state, state->irq);

    printk("XPMEM: Palacios PCI device enabled (device IRQ vector=%d)\n", state->irq);
    return 0;

err_connection:
    irq_free(state->irq, (void *)state);

err:
    printk("XPMEM: Palacios PCI device initialization failed\n");
//    pci_disable_device(dev);
    return ret;
}

static void
xpmem_remove_driver(struct pci_dev * dev)
{
    xpmem_link_t link;

    if ((link = xpmem_reset_host_link()) > 0)
        xpmem_remove_connection(link);
}

static struct pci_driver
xpmem_driver =
{
    .name       = "pci_xpmem",
    .id_table   = xpmem_ids,
    .probe      = xpmem_probe_driver,
    .remove     = xpmem_remove_driver,
};


int
xpmem_detach_host_paddr(u64 paddr)
{
    struct xpmem_palacios_state * state = NULL;
    xpmem_link_t                  link  = xpmem_get_host_link();

    /* We might be in the host already */
    if (link == 0)
        return 0;

    state = xpmem_get_link_data(link);
    if (state == NULL)
        return -1;

    xpmem_detach_hcall(state, paddr);

    xpmem_put_link_data(state->link);

    return 0;
}


int
xpmem_request_host_vector(int vector)
{
    struct xpmem_palacios_state * state       = NULL;
    xpmem_link_t                  link        = xpmem_get_host_link();
    int                           host_vector = vector;

    if (link == 0)
        return host_vector;

    state = xpmem_get_link_data(link);
    if (state == NULL)
        return -EBUSY;

    xpmem_request_irq_hcall(state, vector, &host_vector);
    if (host_vector < 0)
        XPMEM_ERR("Cannot allocate host IDT vector");

    xpmem_put_link_data(link);
    return host_vector;
}

void
xpmem_release_host_vector(int host_vector)
{
    struct xpmem_palacios_state * state = NULL;
    xpmem_link_t                  link  = xpmem_get_host_link();

    if (link == 0)
        return;

    state = xpmem_get_link_data(link);
    if (state == NULL) {
        XPMEM_ERR("Leaking Host IDT vector %d", host_vector);
        return;
    }

    xpmem_release_irq_hcall(state, host_vector);
    xpmem_put_link_data(link);
}

int
xpmem_get_host_apic_id(int cpu)
{
    struct xpmem_palacios_state * state   = NULL;
    xpmem_link_t                  link    = xpmem_get_host_link();
    int                           apic_id = -1;

    if (link == 0)
        return cpu_info[cpu].arch.apic_id;

    state = xpmem_get_link_data(link);
    if (state == NULL)
        return -EBUSY;

    xpmem_read_apicid_hcall(state, cpu, &apic_id);
    if (apic_id < 0)
        XPMEM_ERR("Cannot read host apic id for cpu %d", cpu);

    xpmem_put_link_data(link);
    return apic_id;
}



int
xpmem_palacios_init(void) {
    int ret = 0;

    /* Register PCI driver */
    ret = pci_register_driver(&xpmem_driver);
    if (ret != 0) {
        XPMEM_ERR("Failed to register Palacios PCI device driver");
        return ret;
    }

    return 0;
}

int
xpmem_palacios_deinit(void)
{
    xpmem_link_t link;

    if ((link = xpmem_reset_host_link()) > 0)
        xpmem_remove_connection(link);

    pci_unregister_driver(&xpmem_driver);

    return 0;
}


uint32_t
xpmem_get_and_clear_host_sig_count(int guest_vector)
{
    struct xpmem_palacios_state * state = NULL;
    xpmem_link_t                  link  = xpmem_get_host_link();
    uint8_t                       sigs  = 0;
    unsigned long                 flags = 0;

    if (link == 0)
        return 1;

    state = xpmem_get_link_data(link);
    if (state == NULL)
        return 1;

    /* Determine how many irqs are in the device, and then clear them */
    spin_lock_irqsave(&(state->sig_cnt_lock), flags);
    {
        read_bar(&(state->xpmem_sig_cnts[guest_vector]), (void *)&sigs, sizeof(uint8_t));
    }
    spin_unlock_irqrestore(&(state->sig_cnt_lock), flags);

    xpmem_sig_clear_hcall(state, guest_vector, sigs);

    return (uint32_t)sigs;
}

// MODULE_DEVICE_TABLE(pci, xpmem_ids);
