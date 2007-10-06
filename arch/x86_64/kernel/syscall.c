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
 * Setup for the include of <arch/unistd.h> in the sys_call_table[]
 * definition below.
 */
#undef __SYSCALL
#define __SYSCALL(nr, sym) [ nr ] = sym, 
#undef _ARCH_X86_64_UNISTD_H

/**
 * Prototype for system call handler functions.
 */
typedef long (*sys_call_ptr_t)(void); 

/**
 * Dummy handler for unimplemented system calls.
 */
long sys_not_implemented(void)
{
	printk(KERN_DEBUG "System call not implemented!\n");
	return -ENOSYS;
}

/**
 * This is the system call table.  The system_call() function in entry.S
 * uses this table to determine the handler function to call for each
 * system call.  The table is indexed by system call number.
 */
const sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
	[0 ... __NR_syscall_max] = sys_not_implemented,
	#include <arch/unistd.h>
};

