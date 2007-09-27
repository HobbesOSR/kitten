#include <lwk/kernel.h>
#include <lwk/bootmem.h>
#include <lwk/cache.h>
#include <arch/sections.h>
#include <arch/pda.h>

/**
 * This initializes a per-CPU area for each CPU.
 *
 * TODO: The PDA and per-CPU areas are pretty tightly wound.  It should be
 *       possible to make the per-CPU area *be* the PDA, or put another way,
 *       point %GS at the per-CPU area rather than the PDA.  All of the PDA's
 *       current contents would become normal per-CPU variables.
 */
void __init
setup_per_cpu_areas(void)
{ 
	int i;
	unsigned long size;

	/*
 	 * There is an ELF section containing all per-CPU variables
 	 * surrounded by __per_cpu_start and __per_cpu_end symbols.
 	 * We create a copy of this ELF section for each CPU.
 	 */
	size = ALIGN(__per_cpu_end - __per_cpu_start, SMP_CACHE_BYTES);

	for_each_cpu_mask (i, cpu_present_map) {
		char *ptr;

		ptr = alloc_bootmem(size);
		if (!ptr)
			panic("Cannot allocate cpu data for CPU %d\n", i);

		/*
 		 * Pre-bias data_offset by subtracting its offset from
 		 * __per_cpu_start.  Later, per_cpu() will calculate a
 		 * per_cpu variable's address with:
 		 * 
 		 * addr = offset_in_percpu_ELF_section + data_offset
 		 *      = (__per_cpu_start + offset)   + (ptr - __per_cpu_start)
 		 *      =                    offset    +  ptr
 		 */
		cpu_pda(i)->data_offset = ptr - __per_cpu_start;

		memcpy(ptr, __per_cpu_start, __per_cpu_end - __per_cpu_start);
	}
} 

