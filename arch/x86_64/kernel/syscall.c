#include <lwk/kernel.h>
#include <lwk/linkage.h>
#include <lwk/cache.h>
#include <lwk/errno.h>
#include <arch/asm-offsets.h>

/**
 * This generate prototypes for all system call handlers.
 */
#define __SYSCALL(nr, sym) extern long sym(void);
#undef _ARCH_X86_64_UNISTD_H
#include <arch/unistd.h>

/**
 * Setup for the include of <arch/unistd.h> in the syscall_table[]
 * definition below.
 */
#undef __SYSCALL
#define __SYSCALL(nr, sym) [ nr ] = sym, 
#undef _ARCH_X86_64_UNISTD_H

/**
 * Prototype for system call handler functions.
 */
typedef long (*syscall_ptr_t)(void); 

/**
 * Dummy handler for unimplemented system calls.
 */
long syscall_not_implemented(void)
{
	unsigned long syscall_number;

	/* On entry to function, syscall # is in %rax register */
	asm volatile("mov %%rax, %0" : "=r"(syscall_number)::"%rax");

	printk(KERN_DEBUG "System call not implemented! "
	                  "(syscall_number=%lu)\n", syscall_number);
	return -ENOSYS;
}

/**
 * This is the system call table.  The system_call() function in entry.S
 * uses this table to determine the handler function to call for each
 * system call.  The table is indexed by system call number.
 */
const syscall_ptr_t syscall_table[__NR_syscall_max+1] = {
	[0 ... __NR_syscall_max] = syscall_not_implemented,
	#include <arch/unistd.h>
};

