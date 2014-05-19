#include <arch/uaccess.h>
#include <lwk/smp.h>

int
sys_phys_cpu_remove(id_t phys_cpu_id, id_t apic_id)
{
    return phys_cpu_remove(phys_cpu_id, apic_id);
}
