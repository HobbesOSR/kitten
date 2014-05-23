#include <lwk/driver.h>
#include <lwk/interrupt.h>
#include <lwk/pci/pci.h>
#include <lwk/aspace.h>
#include <lwk/resource.h>
#include <lwk/pci/pcicfg_regs.h>
#include <lwk/pci/msidef.h>
#include <lwk/cpuinfo.h>
#include <arch/page.h>
#include <arch/proto.h>
#include <arch/io.h>
#include <arch/fixmap.h>
#include <arch/io_apic.h>
#include <arch/apic.h>
#include <arch/delay.h>
#include <arch/pisces/pisces_pci.h>

#ifdef DEBUG_IRQ_PROXY
#define debug(fmt, args...) printk(KERN_INFO "%s: " fmt, __func__, ## args)
#else
#define debug(fmt, args...)
#endif


extern struct cpuinfo cpu_info[NR_CPUS];

struct pisces_irq_proxy {
    pci_dev_t * dev;
    struct msi_msg old_msg;
    struct msi_msg new_msg;
    irq_handler_t handler;
    void * priv;
    int vector;
};

// currently only support one proxy
static struct pisces_irq_proxy proxy;

static void send_ipi_from_msi(struct msi_msg *msg) 
{
    // assume physical mode
    unsigned int icr = msg->vector 
        | (msg->delivery_mode << 8)
        | (msg->level << 14)
        | (msg->trigger_mode << 15);

    raw_lapic_send_ipi_to_apic(msg->dest, icr);
}


static irqreturn_t
pisces_interrupt_handler(int vector, void *priv)
{
    if (proxy.handler(vector, priv) == IRQ_NONE) {
        send_ipi_from_msi(&proxy.old_msg);
    }

    return IRQ_HANDLED;
}

static int setup_msi_proxy(struct pisces_irq_proxy * proxy)
{
    pcicfg_hdr_t * hdr = &(proxy->dev->cfg);

    if (!(hdr->msi.valid == true 
            && hdr->msi.msi_msgnum == 1
            && (hdr->msi.msi_ctrl & PCIM_MSICTRL_MSI_ENABLE))) {
        printk(KERN_ERR "Error: only support single vector MSI device\n");
        return -1;
    }

    pci_msi_disable(proxy->dev);

    read_msi_msg(proxy->dev, &(proxy->old_msg));

    {
        struct msi_msg *msg = &(proxy->old_msg);

        debug("dump old_msi_msg\n");
        debug("  address_lo %x (dest %u, redirect_hint %u, dest_mode %u)\n", 
                msg->address_lo, msg->dest, msg->redirect_hint, msg->dest_mode);
        debug("  data %x (vector %u, delivery_mode %u, level %u, trigger_mode %u)\n", 
                msg->data, msg->vector, msg->delivery_mode, msg->level, msg->trigger_mode);

        if (!(msg->redirect_hint == 0 && msg->dest_mode == 0)) {
            printk("Error: unsupported MSI mode (redirect_hint %u, dest_mode %u)",
                    msg->redirect_hint, msg->dest_mode);
            return -1;
        }
    }

    // change dest address, set to 0
    compose_msi_msg(&(proxy->new_msg), cpu_info[0].arch.apic_id, proxy->vector);
    debug("new_msi_msg: dest %d, vector %d\n", cpu_info[0].arch.apic_id, proxy->vector);

    write_msi_msg(proxy->dev, &(proxy->new_msg));

    // request irq
    irq_request(proxy->vector, &pisces_interrupt_handler, 0, "irq-proxy", proxy->priv);

    // enable MSI
    pci_msi_enable(proxy->dev); 

    return 0;
}

int pisces_setup_irq_proxy(
    pci_dev_t * pci_dev, 
    irq_handler_t handler, 
    int vector, 
    void *priv)
{
    pcicfg_hdr_t * hdr = &pci_dev->cfg;

    memset(&proxy, 0, sizeof(struct pisces_irq_proxy));
    proxy.dev = pci_dev;
    proxy.handler = handler;
    proxy.vector = vector;
    proxy.priv = priv;

    if (hdr->msi.valid == true && (hdr->msi.msi_ctrl & PCIM_MSICTRL_MSI_ENABLE)) {
        setup_msi_proxy(&proxy);
    } else {
        //TODO: add MSI-X proxy support
        printk(KERN_ERR "Error: unsupported device for irq proxy\n");
        return -1;
    }

    return 0;
}
