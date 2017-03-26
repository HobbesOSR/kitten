#include <arch/apic.h>

#include <xpmem_private.h>

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
