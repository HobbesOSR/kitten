#ifndef _ASM_X86_TOPOLOGY_H
#define _ASM_X86_TOPOLOGY_H

#include <asm-generic/topology.h>

#ifdef CONFIG_NUMA
extern int get_mp_bus_to_node(int busnum);
extern void set_mp_bus_to_node(int busnum, int node);
#else
static inline int get_mp_bus_to_node(int busnum)
{
        return 0;
}
static inline void set_mp_bus_to_node(int busnum, int node)
{
}
#endif

#endif /* _ASM_X86_TOPOLOGY_H */
