#ifndef _ASM_X86_64_VSYSCALL_H_
#define _ASM_X86_64_VSYSCALL_H_

enum vsyscall_num {
	__NR_vgettimeofday,
	__NR_vtime,
	__NR_vgetcpu,
};

#define VSYSCALL_START (-10UL << 20)
#define VSYSCALL_SIZE 1024
#define VSYSCALL_END (-2UL << 20)
#define VSYSCALL_MAPPED_PAGES 1
#define VSYSCALL_ADDR(vsyscall_nr) (VSYSCALL_START+VSYSCALL_SIZE*(vsyscall_nr))

#ifdef __KERNEL__
#include <lwk/init.h>

void __init vsyscall_map(void);
void __init vsyscall_init(void);


/**
 * Prototype for system call handler functions.
 */
typedef long (*syscall_ptr_t)(void); 

/** Register a system call.
 *
 * Some system calls are registered at run-time rather than compile
 * time.
 *
 * \note The type signature for handler is generic for any number
 * of arguments and may require a cast.
 *
 * \todo Put this is a non-architecture specific file?
 */
extern void
syscall_register(
	unsigned		nr,
	syscall_ptr_t		handler
);

#endif /* __KERNEL__ */

#endif /* _ASM_X86_64_VSYSCALL_H_ */
