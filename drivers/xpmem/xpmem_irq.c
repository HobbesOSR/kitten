#include <lwk/cpuinfo.h>
#include <arch/apic.h>

#include <xpmem_private.h>

extern struct cpuinfo cpu_info[NR_CPUS];

int
xpmem_request_irq(irqreturn_t (*callback)(int, void *),
		  void	    * priv_data)
{
    int irq = 0;

    irq = irq_request_free_vector(callback, 0, "xpmem-irq", priv_data);
    if (irq < 0)
	XPMEM_ERR("Cannot allocate irq");

    return irq;
}

void
xpmem_release_irq(int 	 irq,
		  void * priv_data)
{
    irq_free(irq, priv_data);
}

int
xpmem_irq_to_vector(int irq)
{
    return irq;
}

void
xpmem_send_ipi_to_apic(unsigned int apic_id,
		       unsigned int vector)
{
    unsigned long irqflags;

    local_irq_save(irqflags);
    {
	lapic_send_ipi_to_apic(apic_id, vector);
    }
    local_irq_restore(irqflags);
}

int
xpmem_deliver_host_irq(xpmem_segid_t segid,
                       xpmem_sigid_t sigid,
		       xpmem_domid_t domid)
{
    struct xpmem_signal * sig = (struct xpmem_signal *)&sig;

    /* No guest support at this point in Kitten, so we know we are in
     * a host enclave. This is just an IPI */
    xpmem_send_ipi_to_apic(sig->apic_id, sig->vector);

    return 0;
}

int
xpmem_request_host_vector(int vector)
{
    return vector;
}

void
xpmem_release_host_vector(int vector)
{
}

int
xpmem_get_host_apic_id(int cpu)
{
    return cpu_info[cpu].arch.apic_id;
}
