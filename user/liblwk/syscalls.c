/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/unistd.h>
#include <lwk/liblwk.h>
#include <lwk/if_arp.h>
#include <arch/syscall.h>
#include <lwk/if.h>


/**
 * Physical memory management. 
 */
SYSCALL1(pmem_add, const struct pmem_region *);
SYSCALL1(pmem_del, const struct pmem_region *);
SYSCALL1(pmem_update, const struct pmem_region *);
SYSCALL2(pmem_query, const struct pmem_region *, struct pmem_region *);
SYSCALL4(pmem_alloc, size_t, size_t,
         const struct pmem_region *, struct pmem_region *);
SYSCALL1(pmem_zero, const struct pmem_region *);

/**
 * Address space management.
 */
SYSCALL1(aspace_get_myid, id_t *);
SYSCALL3(aspace_create, id_t, const char *, id_t *);
SYSCALL1(aspace_destroy, id_t);
SYSCALL5(aspace_find_hole, id_t, vaddr_t, size_t, size_t, vaddr_t *);
SYSCALL6(aspace_add_region,
         id_t, vaddr_t, size_t, vmflags_t, vmpagesize_t, const char *);
SYSCALL3(aspace_del_region, id_t, vaddr_t, size_t);
SYSCALL4(aspace_map_pmem, id_t, paddr_t, vaddr_t, size_t);
SYSCALL3(aspace_unmap_pmem, id_t, vaddr_t, size_t);
SYSCALL4(aspace_smartmap, id_t, id_t, vaddr_t, size_t);
SYSCALL2(aspace_unsmartmap, id_t, id_t);
SYSCALL3(aspace_virt_to_phys, id_t, vaddr_t, paddr_t *);
SYSCALL1(aspace_dump2console, id_t);
SYSCALL2(aspace_update_user_cpumask, id_t, user_cpumask_t *);
SYSCALL2(aspace_get_rank, id_t, id_t *);
SYSCALL2(aspace_set_rank, id_t, id_t);
SYSCALL2(aspace_update_user_hio_syscall_mask, id_t, user_syscall_mask_t *);

/**
 * Task management.
 */
SYSCALL2(task_create, const start_state_t *, id_t *);
SYSCALL5(task_meas, id_t, id_t, uint64_t *, uint64_t *, uint64_t *);
SYSCALL1(task_switch_cpus, id_t);

/**
 * Network related system calls.
 */
SYSCALL1(lwk_arp, struct lwk_arpreq *);
SYSCALL1(lwk_ifconfig, struct lwk_ifreq *);

/**
 * ELF related system calls.
 */
SYSCALL2(elf_hwcap, id_t, uint32_t *);

/**
 * CPU Management system calls
 */
SYSCALL2(phys_cpu_add, id_t, id_t);
SYSCALL2(phys_cpu_remove, id_t, id_t);


/**
 * Palacios hypervisor control system calls.
 */
SYSCALL2(v3_start_guest, vaddr_t, size_t);

/**
 * Scheduling control system calls.
 */
SYSCALL2(sched_yield_task_to, int, int);
SYSCALL4(sched_setparams_task, int, int, int64_t, int64_t);
