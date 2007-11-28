#ifndef _ARCH_X86_64_UNISTD_H
#define _ARCH_X86_64_UNISTD_H

/**
 * This file contains the system call numbers.
 *
 * NOTE: holes are not allowed.
 */

#ifndef __SYSCALL
#define __SYSCALL(a,b) 
#endif

/**
 * For now, define some dummy syscalls.
 */
#define __NR_write_console                       0
__SYSCALL(__NR_write_console, sys_write_console)

#endif
