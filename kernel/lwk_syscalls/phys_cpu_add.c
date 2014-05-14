#include <arch/uaccess.h>
#include <lwk/smp.h>

int
sys_phys_cpu_add(id_t phys_cpu_id, id_t apic_id)
{
    return phys_cpu_add(phys_cpu_id, apic_id);
}
