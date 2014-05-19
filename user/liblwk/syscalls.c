/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/unistd.h>
#include <lwk/liblwk.h>
#include <lwk/if_arp.h>
#include <lwk/if.h>

/**
 * There is no way to specify inline assembly constraints for %r10 (arg4),
 * %r8 (arg5), and %r9 (arg6), so the macros below specify the registers
 * to use for local variables as a work-around.
 *
 * GCC BUG? -- For some unknown reason, the register specified to store
 *             a local variable is not always honored if the variable
 *             is assigned when it is declared.  Work-around by declaring
 *             and assigning on separate lines.
 */
#define SYSCALL0(name) 						\
int name(void) {						\
	int status;						\
	asm volatile(						\
		"syscall"					\
		: "=a" (status)					\
		: "0" (__NR_##name)				\
		: "memory", "rcx", "r11", "cc"			\
	);							\
	return status;						\
}

#define SYSCALL1(name, type1) 					\
int name(type1 arg1) {						\
	int status;						\
	register type1 rdi asm("rdi");				\
	rdi = arg1;						\
	asm volatile(						\
		"syscall"					\
		: "=a" (status)					\
		: "0" (__NR_##name),				\
		  "r" (rdi)					\
		: "memory", "rcx", "r11", "cc"			\
	);							\
	return status;						\
}

#define SYSCALL2(name, type1, type2) 				\
int name(type1 arg1, type2 arg2) {				\
	int status;						\
	register type1 rdi asm("rdi");				\
	register type2 rsi asm("rsi");				\
	rdi = arg1;						\
	rsi = arg2;						\
	asm volatile(						\
		"syscall"					\
		: "=a" (status)					\
		: "0" (__NR_##name),				\
		  "r" (rdi),					\
		  "r" (rsi)					\
		: "memory", "rcx", "r11", "cc"			\
	);							\
	return status;						\
}

#define SYSCALL3(name, type1, type2, type3) 			\
int name(type1 arg1, type2 arg2, type3 arg3) {			\
	int status;						\
	register type1 rdi asm("rdi");				\
	register type2 rsi asm("rsi");				\
	register type3 rdx asm("rdx");				\
	rdi = arg1;						\
	rsi = arg2;						\
	rdx = arg3;						\
	asm volatile(						\
		"syscall"					\
		: "=a" (status)					\
		: "0" (__NR_##name),				\
		  "r" (rdi),					\
		  "r" (rsi),					\
		  "r" (rdx)					\
		: "memory", "rcx", "r11", "cc"			\
	);							\
	return status;						\
}

#define SYSCALL4(name, type1, type2, type3, type4) 		\
int name(type1 arg1, type2 arg2, type3 arg3, type4 arg4) {	\
	int status;						\
	register type1 rdi asm("rdi");				\
	register type2 rsi asm("rsi");				\
	register type3 rdx asm("rdx");				\
	register type4 r10 asm("r10");				\
	rdi = arg1;						\
	rsi = arg2;						\
	rdx = arg3;						\
	r10 = arg4;						\
	asm volatile(						\
		"syscall"					\
		: "=a" (status)					\
		: "0" (__NR_##name),				\
		  "r" (rdi),					\
		  "r" (rsi),					\
		  "r" (rdx),					\
		  "r" (r10)					\
		: "memory", "rcx", "r11", "cc"			\
	);							\
	return status;						\
}

#define SYSCALL5(name, type1, type2, type3, type4, type5) 	\
int name(type1 arg1, type2 arg2, type3 arg3, type4 arg4,	\
         type5 arg5) {						\
	int status;						\
	register type1 rdi asm("rdi");				\
	register type2 rsi asm("rsi");				\
	register type3 rdx asm("rdx");				\
	register type4 r10 asm("r10");				\
	register type5 r8  asm("r8");				\
	rdi = arg1;						\
	rsi = arg2;						\
	rdx = arg3;						\
	r10 = arg4;						\
	r8  = arg5;						\
	asm volatile(						\
		"syscall"					\
		: "=a" (status)					\
		: "0" (__NR_##name),				\
		  "r" (rdi),					\
		  "r" (rsi),					\
		  "r" (rdx),					\
		  "r" (r10),					\
		  "r" (r8)					\
		: "memory", "rcx", "r11", "cc"			\
	);							\
	return status;						\
}

#define SYSCALL6(name, type1, type2, type3, type4, type5, type6)\
int name(type1 arg1, type2 arg2, type3 arg3, type4 arg4,	\
         type5 arg5, type6 arg6) {				\
	int status;						\
	register type1 rdi asm("rdi");				\
	register type2 rsi asm("rsi");				\
	register type3 rdx asm("rdx");				\
	register type4 r10 asm("r10");				\
	register type5 r8  asm("r8");				\
	register type6 r9  asm("r9");				\
	rdi = arg1;						\
	rsi = arg2;						\
	rdx = arg3;						\
	r10 = arg4;						\
	r8  = arg5;						\
	r9  = arg6;						\
	asm volatile(						\
		"syscall"					\
		: "=a" (status)					\
		: "0" (__NR_##name),				\
		  "r" (rdi),					\
		  "r" (rsi),					\
		  "r" (rdx),					\
		  "r" (r10),					\
		  "r" (r8),					\
		  "r" (r9)					\
		: "memory", "rcx", "r11", "cc"			\
	);							\
	return status;						\
}

/**
 * Physical memory management. 
 */
SYSCALL1(pmem_add, const struct pmem_region *);
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

/**
 * Task management.
 */
SYSCALL2(task_create, const start_state_t *, id_t *);
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
