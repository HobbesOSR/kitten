/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ARM64_TOPOLOGY_H
#define __ARM64_TOPOLOGY_H

#include <lwk/cpumask.h>
#include <lwk/types.h>

struct cpu_topology {
	int thread_id;
	int core_id;
	int cluster_id;
	cpumask_t thread_sibling;
	cpumask_t core_sibling;

	uint8_t affinities[4];
};

extern struct cpu_topology cpu_topology[NR_CPUS];

#define topology_physical_package_id(cpu)	(cpu_topology[cpu].cluster_id)
#define topology_core_id(cpu)		(cpu_topology[cpu].core_id)
#define topology_core_cpumask(cpu)	(&cpu_topology[cpu].core_sibling)
#define topology_sibling_cpumask(cpu)	(&cpu_topology[cpu].thread_sibling)

void init_cpu_topology(void);
void store_cpu_topology(unsigned int cpuid);
const struct cpumask *cpu_coregroup_mask(int cpu);


int get_cpu_affinity(int       cpuid, 
		     uint8_t * aff_0, 
		     uint8_t * aff_1, 
		     uint8_t * aff_2,
		     uint8_t * aff_3);


#if 0
#ifdef CONFIG_NUMA

struct pci_bus;
int pcibus_to_node(struct pci_bus *bus);
#define cpumask_of_pcibus(bus)	(pcibus_to_node(bus) == -1 ?		\
				 cpu_all_mask :				\
				 cpumask_of_node(pcibus_to_node(bus)))

#endif /* CONFIG_NUMA */
#endif

#endif /* _ASM_ARM_TOPOLOGY_H */
