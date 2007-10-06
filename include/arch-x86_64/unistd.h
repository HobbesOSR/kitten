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
#define __NR_read                                0
__SYSCALL(__NR_read, sys_not_implemented)
#define __NR_write                               1
__SYSCALL(__NR_write, sys_not_implemented)
#define __NR_open                                2
__SYSCALL(__NR_open, sys_not_implemented)
#define __NR_close                               3
__SYSCALL(__NR_close, sys_not_implemented)
#define __NR_stat                                4
__SYSCALL(__NR_stat, sys_not_implemented)
#define __NR_fstat                               5
__SYSCALL(__NR_fstat, sys_not_implemented)
#define __NR_lstat                               6
__SYSCALL(__NR_lstat, sys_not_implemented)
#define __NR_poll                                7
__SYSCALL(__NR_poll, sys_not_implemented)

#endif
