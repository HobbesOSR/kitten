#include <lwk/kernel.h>
#include <lwk/linkage.h>
#include <lwk/cache.h>
#include <lwk/errno.h>
#include <arch/asm-offsets.h>
#include <arch/vsyscall.h>

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
 * Dummy handler for unimplemented system calls.
 */
static long
syscall_not_implemented_silent(void)
{
	return -ENOSYS;
}


long
syscall_not_implemented(void)
{
	unsigned long syscall_number;

	/* On entry to function, syscall # is in %rax register */
	asm volatile("mov %%rax, %0" : "=r"(syscall_number)::"%rax");

	printk(KERN_DEBUG "System call not implemented! "
	                  "(syscall_number=%lu)\n", syscall_number);

	// Only warn once about each systemcall
	syscall_register( syscall_number, syscall_not_implemented_silent );
	return -ENOSYS;
}

/**
 * This is the system call table.  The system_call() function in entry.S
 * uses this table to determine the handler function to call for each
 * system call.  The table is indexed by system call number.
 */
syscall_ptr_t sys_call_table[__NR_syscall_max+1] = {
	[0 ... __NR_syscall_max] = syscall_not_implemented,
	#include <arch/unistd.h>
};


void
syscall_register(
	unsigned		nr,
	syscall_ptr_t		handler
)
{
	if( sys_call_table[ nr ] != syscall_not_implemented
	&&  sys_call_table[ nr ] != syscall_not_implemented_silent )
		printk( KERN_WARNING
			"%s: Overwriting syscall %d\n",
			__func__,
			nr
		);

	sys_call_table[ nr ] = handler;
}
