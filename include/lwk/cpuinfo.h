#ifndef _LWK_CPUINFO_H
#define _LWK_CPUINFO_H

#include <lwk/types.h>
#include <lwk/cpumask.h>
#include <lwk/cache.h>
#include <arch/cpuinfo.h>

/**
 * CPU information.
 * Each CPU in the system is described by one of these structures.
 */
struct cpuinfo {
	/* CPU's logical ID */
	uint16_t	logical_id;

	/* Topology information */
	uint16_t	phys_socket_id;	// Physical socket/package
	uint16_t	phys_core_id;	// Core ID in the socket/package
	uint16_t	phys_thread_id;	// Hardware Thread ID in core
	uint16_t	numa_node_id;	// NUMA node ID this CPU is in
	cpumask_t	numa_node_map;	// CPUs in this CPU's NUMA node
	cpumask_t	llc_share_map;	// CPUs sharing last level cache

	/* Architecture specific */
	struct arch_cpuinfo arch;
} ____cacheline_aligned;

extern struct cpuinfo cpu_info[NR_CPUS];

extern void print_cpuinfo(struct cpuinfo *);

#endif
