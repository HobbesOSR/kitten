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

#define __NR_read                                0
__SYSCALL(__NR_read, syscall_not_implemented)
#define __NR_write                               1
__SYSCALL(__NR_write, sys_write)
#define __NR_open                                2
__SYSCALL(__NR_open, syscall_not_implemented)
#define __NR_close                               3
__SYSCALL(__NR_close, syscall_not_implemented)
#define __NR_stat                                4
__SYSCALL(__NR_stat, syscall_not_implemented)
#define __NR_fstat                               5
__SYSCALL(__NR_fstat, sys_fstat)
#define __NR_lstat                               6
__SYSCALL(__NR_lstat, syscall_not_implemented)
#define __NR_poll                                7
__SYSCALL(__NR_poll, syscall_not_implemented)

#define __NR_lseek                               8
__SYSCALL(__NR_lseek, syscall_not_implemented)
#define __NR_mmap                                9
__SYSCALL(__NR_mmap, sys_mmap)
#define __NR_mprotect                           10
__SYSCALL(__NR_mprotect, syscall_not_implemented)
#define __NR_munmap                             11
__SYSCALL(__NR_munmap, syscall_not_implemented)
#define __NR_brk                                12
__SYSCALL(__NR_brk, sys_brk)
#define __NR_rt_sigaction                       13
__SYSCALL(__NR_rt_sigaction, syscall_not_implemented)
#define __NR_rt_sigprocmask                     14
__SYSCALL(__NR_rt_sigprocmask, syscall_not_implemented)
#define __NR_rt_sigreturn                       15
__SYSCALL(__NR_rt_sigreturn, syscall_not_implemented)

#define __NR_ioctl                              16
__SYSCALL(__NR_ioctl, syscall_not_implemented)
#define __NR_pread64                            17
__SYSCALL(__NR_pread64, syscall_not_implemented)
#define __NR_pwrite64                           18
__SYSCALL(__NR_pwrite64, syscall_not_implemented)
#define __NR_readv                              19
__SYSCALL(__NR_readv, syscall_not_implemented)
#define __NR_writev                             20
__SYSCALL(__NR_writev, syscall_not_implemented)
#define __NR_access                             21
__SYSCALL(__NR_access, syscall_not_implemented)
#define __NR_pipe                               22
__SYSCALL(__NR_pipe, syscall_not_implemented)
#define __NR_select                             23
__SYSCALL(__NR_select, syscall_not_implemented)

#define __NR_sched_yield                        24
__SYSCALL(__NR_sched_yield, syscall_not_implemented)
#define __NR_mremap                             25
__SYSCALL(__NR_mremap, syscall_not_implemented)
#define __NR_msync                              26
__SYSCALL(__NR_msync, syscall_not_implemented)
#define __NR_mincore                            27
__SYSCALL(__NR_mincore, syscall_not_implemented)
#define __NR_madvise                            28
__SYSCALL(__NR_madvise, syscall_not_implemented)
#define __NR_shmget                             29
__SYSCALL(__NR_shmget, syscall_not_implemented)
#define __NR_shmat                              30
__SYSCALL(__NR_shmat, syscall_not_implemented)
#define __NR_shmctl                             31
__SYSCALL(__NR_shmctl, syscall_not_implemented)

#define __NR_dup                                32
__SYSCALL(__NR_dup, syscall_not_implemented)
#define __NR_dup2                               33
__SYSCALL(__NR_dup2, syscall_not_implemented)
#define __NR_pause                              34
__SYSCALL(__NR_pause, syscall_not_implemented)
#define __NR_nanosleep                          35
__SYSCALL(__NR_nanosleep, syscall_not_implemented)
#define __NR_getitimer                          36
__SYSCALL(__NR_getitimer, syscall_not_implemented)
#define __NR_alarm                              37
__SYSCALL(__NR_alarm, syscall_not_implemented)
#define __NR_setitimer                          38
__SYSCALL(__NR_setitimer, syscall_not_implemented)
#define __NR_getpid                             39
__SYSCALL(__NR_getpid, syscall_not_implemented)

#define __NR_sendfile                           40
__SYSCALL(__NR_sendfile, syscall_not_implemented)
#define __NR_socket                             41
__SYSCALL(__NR_socket, syscall_not_implemented)
#define __NR_connect                            42
__SYSCALL(__NR_connect, syscall_not_implemented)
#define __NR_accept                             43
__SYSCALL(__NR_accept, syscall_not_implemented)
#define __NR_sendto                             44
__SYSCALL(__NR_sendto, syscall_not_implemented)
#define __NR_recvfrom                           45
__SYSCALL(__NR_recvfrom, syscall_not_implemented)
#define __NR_sendmsg                            46
__SYSCALL(__NR_sendmsg, syscall_not_implemented)
#define __NR_recvmsg                            47
__SYSCALL(__NR_recvmsg, syscall_not_implemented)

#define __NR_shutdown                           48
__SYSCALL(__NR_shutdown, syscall_not_implemented)
#define __NR_bind                               49
__SYSCALL(__NR_bind, syscall_not_implemented)
#define __NR_listen                             50
__SYSCALL(__NR_listen, syscall_not_implemented)
#define __NR_getsockname                        51
__SYSCALL(__NR_getsockname, syscall_not_implemented)
#define __NR_getpeername                        52
__SYSCALL(__NR_getpeername, syscall_not_implemented)
#define __NR_socketpair                         53
__SYSCALL(__NR_socketpair, syscall_not_implemented)
#define __NR_setsockopt                         54
__SYSCALL(__NR_setsockopt, syscall_not_implemented)
#define __NR_getsockopt                         55
__SYSCALL(__NR_getsockopt, syscall_not_implemented)

#define __NR_clone                              56
__SYSCALL(__NR_clone, syscall_not_implemented)
#define __NR_fork                               57
__SYSCALL(__NR_fork, syscall_not_implemented)
#define __NR_vfork                              58
__SYSCALL(__NR_vfork, syscall_not_implemented)
#define __NR_execve                             59
__SYSCALL(__NR_execve, syscall_not_implemented)
#define __NR_exit                               60
__SYSCALL(__NR_exit, syscall_not_implemented)
#define __NR_wait4                              61
__SYSCALL(__NR_wait4, syscall_not_implemented)
#define __NR_kill                               62
__SYSCALL(__NR_kill, syscall_not_implemented)
#define __NR_uname                              63
__SYSCALL(__NR_uname, sys_uname)

#define __NR_semget                             64
__SYSCALL(__NR_semget, syscall_not_implemented)
#define __NR_semop                              65
__SYSCALL(__NR_semop, syscall_not_implemented)
#define __NR_semctl                             66
__SYSCALL(__NR_semctl, syscall_not_implemented)
#define __NR_shmdt                              67
__SYSCALL(__NR_shmdt, syscall_not_implemented)
#define __NR_msgget                             68
__SYSCALL(__NR_msgget, syscall_not_implemented)
#define __NR_msgsnd                             69
__SYSCALL(__NR_msgsnd, syscall_not_implemented)
#define __NR_msgrcv                             70
__SYSCALL(__NR_msgrcv, syscall_not_implemented)
#define __NR_msgctl                             71
__SYSCALL(__NR_msgctl, syscall_not_implemented)

#define __NR_fcntl                              72
__SYSCALL(__NR_fcntl, syscall_not_implemented)
#define __NR_flock                              73
__SYSCALL(__NR_flock, syscall_not_implemented)
#define __NR_fsync                              74
__SYSCALL(__NR_fsync, syscall_not_implemented)
#define __NR_fdatasync                          75
__SYSCALL(__NR_fdatasync, syscall_not_implemented)
#define __NR_truncate                           76
__SYSCALL(__NR_truncate, syscall_not_implemented)
#define __NR_ftruncate                          77
__SYSCALL(__NR_ftruncate, syscall_not_implemented)
#define __NR_getdents                           78
__SYSCALL(__NR_getdents, syscall_not_implemented)
#define __NR_getcwd                             79
__SYSCALL(__NR_getcwd, syscall_not_implemented)

#define __NR_chdir                              80
__SYSCALL(__NR_chdir, syscall_not_implemented)
#define __NR_fchdir                             81
__SYSCALL(__NR_fchdir, syscall_not_implemented)
#define __NR_rename                             82
__SYSCALL(__NR_rename, syscall_not_implemented)
#define __NR_mkdir                              83
__SYSCALL(__NR_mkdir, syscall_not_implemented)
#define __NR_rmdir                              84
__SYSCALL(__NR_rmdir, syscall_not_implemented)
#define __NR_creat                              85
__SYSCALL(__NR_creat, syscall_not_implemented)
#define __NR_link                               86
__SYSCALL(__NR_link, syscall_not_implemented)
#define __NR_unlink                             87
__SYSCALL(__NR_unlink, syscall_not_implemented)

#define __NR_symlink                            88
__SYSCALL(__NR_symlink, syscall_not_implemented)
#define __NR_readlink                           89
__SYSCALL(__NR_readlink, syscall_not_implemented)
#define __NR_chmod                              90
__SYSCALL(__NR_chmod, syscall_not_implemented)
#define __NR_fchmod                             91
__SYSCALL(__NR_fchmod, syscall_not_implemented)
#define __NR_chown                              92
__SYSCALL(__NR_chown, syscall_not_implemented)
#define __NR_fchown                             93
__SYSCALL(__NR_fchown, syscall_not_implemented)
#define __NR_lchown                             94
__SYSCALL(__NR_lchown, syscall_not_implemented)
#define __NR_umask                              95
__SYSCALL(__NR_umask, syscall_not_implemented)

#define __NR_gettimeofday                       96
__SYSCALL(__NR_gettimeofday, sys_gettimeofday)
#define __NR_getrlimit                          97
__SYSCALL(__NR_getrlimit, syscall_not_implemented)
#define __NR_getrusage                          98
__SYSCALL(__NR_getrusage, syscall_not_implemented)
#define __NR_sysinfo                            99
__SYSCALL(__NR_sysinfo, syscall_not_implemented)
#define __NR_times                             100
__SYSCALL(__NR_times, syscall_not_implemented)
#define __NR_ptrace                            101
__SYSCALL(__NR_ptrace, syscall_not_implemented)
#define __NR_getuid                            102
__SYSCALL(__NR_getuid, sys_getuid)
#define __NR_syslog                            103
__SYSCALL(__NR_syslog, syscall_not_implemented)

/* at the very end the stuff that never runs during the benchmarks */
#define __NR_getgid                            104
__SYSCALL(__NR_getgid, sys_getgid)
#define __NR_setuid                            105
__SYSCALL(__NR_setuid, syscall_not_implemented)
#define __NR_setgid                            106
__SYSCALL(__NR_setgid, syscall_not_implemented)
#define __NR_geteuid                           107
__SYSCALL(__NR_geteuid, sys_getuid)
#define __NR_getegid                           108
__SYSCALL(__NR_getegid, sys_getgid)
#define __NR_setpgid                           109
__SYSCALL(__NR_setpgid, syscall_not_implemented)
#define __NR_getppid                           110
__SYSCALL(__NR_getppid, syscall_not_implemented)
#define __NR_getpgrp                           111
__SYSCALL(__NR_getpgrp, syscall_not_implemented)

#define __NR_setsid                            112
__SYSCALL(__NR_setsid, syscall_not_implemented)
#define __NR_setreuid                          113
__SYSCALL(__NR_setreuid, syscall_not_implemented)
#define __NR_setregid                          114
__SYSCALL(__NR_setregid, syscall_not_implemented)
#define __NR_getgroups                         115
__SYSCALL(__NR_getgroups, syscall_not_implemented)
#define __NR_setgroups                         116
__SYSCALL(__NR_setgroups, syscall_not_implemented)
#define __NR_setresuid                         117
__SYSCALL(__NR_setresuid, syscall_not_implemented)
#define __NR_getresuid                         118
__SYSCALL(__NR_getresuid, syscall_not_implemented)
#define __NR_setresgid                         119
__SYSCALL(__NR_setresgid, syscall_not_implemented)

#define __NR_getresgid                         120
__SYSCALL(__NR_getresgid, syscall_not_implemented)
#define __NR_getpgid                           121
__SYSCALL(__NR_getpgid, syscall_not_implemented)
#define __NR_setfsuid                          122
__SYSCALL(__NR_setfsuid, syscall_not_implemented)
#define __NR_setfsgid                          123
__SYSCALL(__NR_setfsgid, syscall_not_implemented)
#define __NR_getsid                            124
__SYSCALL(__NR_getsid, syscall_not_implemented)
#define __NR_capget                            125
__SYSCALL(__NR_capget, syscall_not_implemented)
#define __NR_capset                            126
__SYSCALL(__NR_capset, syscall_not_implemented)

#define __NR_rt_sigpending                     127
__SYSCALL(__NR_rt_sigpending, syscall_not_implemented)
#define __NR_rt_sigtimedwait                   128
__SYSCALL(__NR_rt_sigtimedwait, syscall_not_implemented)
#define __NR_rt_sigqueueinfo                   129
__SYSCALL(__NR_rt_sigqueueinfo, syscall_not_implemented)
#define __NR_rt_sigsuspend                     130
__SYSCALL(__NR_rt_sigsuspend, syscall_not_implemented)
#define __NR_sigaltstack                       131
__SYSCALL(__NR_sigaltstack, syscall_not_implemented)
#define __NR_utime                             132
__SYSCALL(__NR_utime, syscall_not_implemented)
#define __NR_mknod                             133
__SYSCALL(__NR_mknod, syscall_not_implemented)

/* Only needed for a.out */
#define __NR_uselib                            134
__SYSCALL(__NR_uselib, syscall_not_implemented)
#define __NR_personality                       135
__SYSCALL(__NR_personality, syscall_not_implemented)

#define __NR_ustat                             136
__SYSCALL(__NR_ustat, syscall_not_implemented)
#define __NR_statfs                            137
__SYSCALL(__NR_statfs, syscall_not_implemented)
#define __NR_fstatfs                           138
__SYSCALL(__NR_fstatfs, syscall_not_implemented)
#define __NR_sysfs                             139
__SYSCALL(__NR_sysfs, syscall_not_implemented)

#define __NR_getpriority                       140
__SYSCALL(__NR_getpriority, syscall_not_implemented)
#define __NR_setpriority                       141
__SYSCALL(__NR_setpriority, syscall_not_implemented)
#define __NR_sched_setparam                    142
__SYSCALL(__NR_sched_setparam, syscall_not_implemented)
#define __NR_sched_getparam                    143
__SYSCALL(__NR_sched_getparam, syscall_not_implemented)
#define __NR_sched_setscheduler                144
__SYSCALL(__NR_sched_setscheduler, syscall_not_implemented)
#define __NR_sched_getscheduler                145
__SYSCALL(__NR_sched_getscheduler, syscall_not_implemented)
#define __NR_sched_get_priority_max            146
__SYSCALL(__NR_sched_get_priority_max, syscall_not_implemented)
#define __NR_sched_get_priority_min            147
__SYSCALL(__NR_sched_get_priority_min, syscall_not_implemented)
#define __NR_sched_rr_get_interval             148
__SYSCALL(__NR_sched_rr_get_interval, syscall_not_implemented)

#define __NR_mlock                             149
__SYSCALL(__NR_mlock, syscall_not_implemented)
#define __NR_munlock                           150
__SYSCALL(__NR_munlock, syscall_not_implemented)
#define __NR_mlockall                          151
__SYSCALL(__NR_mlockall, syscall_not_implemented)
#define __NR_munlockall                        152
__SYSCALL(__NR_munlockall, syscall_not_implemented)

#define __NR_vhangup                           153
__SYSCALL(__NR_vhangup, syscall_not_implemented)

#define __NR_modify_ldt                        154
__SYSCALL(__NR_modify_ldt, syscall_not_implemented)

#define __NR_pivot_root                        155
__SYSCALL(__NR_pivot_root, syscall_not_implemented)

#define __NR__sysctl                           156
__SYSCALL(__NR__sysctl, syscall_not_implemented)

#define __NR_prctl                             157
__SYSCALL(__NR_prctl, syscall_not_implemented)
#define __NR_arch_prctl                        158
__SYSCALL(__NR_arch_prctl, sys_arch_prctl)

#define __NR_adjtimex                          159
__SYSCALL(__NR_adjtimex, syscall_not_implemented)

#define __NR_setrlimit                         160
__SYSCALL(__NR_setrlimit, syscall_not_implemented)

#define __NR_chroot                            161
__SYSCALL(__NR_chroot, syscall_not_implemented)

#define __NR_sync                              162
__SYSCALL(__NR_sync, syscall_not_implemented)

#define __NR_acct                              163
__SYSCALL(__NR_acct, syscall_not_implemented)

#define __NR_settimeofday                      164
__SYSCALL(__NR_settimeofday, sys_settimeofday)

#define __NR_mount                             165
__SYSCALL(__NR_mount, syscall_not_implemented)
#define __NR_umount2                           166
__SYSCALL(__NR_umount2, syscall_not_implemented)

#define __NR_swapon                            167
__SYSCALL(__NR_swapon, syscall_not_implemented)
#define __NR_swapoff                           168
__SYSCALL(__NR_swapoff, syscall_not_implemented)

#define __NR_reboot                            169
__SYSCALL(__NR_reboot, syscall_not_implemented)

#define __NR_sethostname                       170
__SYSCALL(__NR_sethostname, syscall_not_implemented)
#define __NR_setdomainname                     171
__SYSCALL(__NR_setdomainname, syscall_not_implemented)

#define __NR_iopl                              172
__SYSCALL(__NR_iopl, syscall_not_implemented)
#define __NR_ioperm                            173
__SYSCALL(__NR_ioperm, syscall_not_implemented)

#define __NR_create_module                     174
__SYSCALL(__NR_create_module, syscall_not_implemented)
#define __NR_init_module                       175
__SYSCALL(__NR_init_module, syscall_not_implemented)
#define __NR_delete_module                     176
__SYSCALL(__NR_delete_module, syscall_not_implemented)
#define __NR_get_kernel_syms                   177
__SYSCALL(__NR_get_kernel_syms, syscall_not_implemented)
#define __NR_query_module                      178
__SYSCALL(__NR_query_module, syscall_not_implemented)

#define __NR_quotactl                          179
__SYSCALL(__NR_quotactl, syscall_not_implemented)

#define __NR_nfsservctl                        180
__SYSCALL(__NR_nfsservctl, syscall_not_implemented)

#define __NR_getpmsg                           181	/* reserved for LiS/STREAMS */
__SYSCALL(__NR_getpmsg, syscall_not_implemented)
#define __NR_putpmsg                           182	/* reserved for LiS/STREAMS */
__SYSCALL(__NR_putpmsg, syscall_not_implemented)

#define __NR_afs_syscall                       183	/* reserved for AFS */ 
__SYSCALL(__NR_afs_syscall, syscall_not_implemented)

#define __NR_tuxcall      		184 /* reserved for tux */
__SYSCALL(__NR_tuxcall, syscall_not_implemented)

#define __NR_security			185
__SYSCALL(__NR_security, syscall_not_implemented)

#define __NR_gettid		186
__SYSCALL(__NR_gettid, syscall_not_implemented)

#define __NR_readahead		187
__SYSCALL(__NR_readahead, syscall_not_implemented)
#define __NR_setxattr		188
__SYSCALL(__NR_setxattr, syscall_not_implemented)
#define __NR_lsetxattr		189
__SYSCALL(__NR_lsetxattr, syscall_not_implemented)
#define __NR_fsetxattr		190
__SYSCALL(__NR_fsetxattr, syscall_not_implemented)
#define __NR_getxattr		191
__SYSCALL(__NR_getxattr, syscall_not_implemented)
#define __NR_lgetxattr		192
__SYSCALL(__NR_lgetxattr, syscall_not_implemented)
#define __NR_fgetxattr		193
__SYSCALL(__NR_fgetxattr, syscall_not_implemented)
#define __NR_listxattr		194
__SYSCALL(__NR_listxattr, syscall_not_implemented)
#define __NR_llistxattr		195
__SYSCALL(__NR_llistxattr, syscall_not_implemented)
#define __NR_flistxattr		196
__SYSCALL(__NR_flistxattr, syscall_not_implemented)
#define __NR_removexattr	197
__SYSCALL(__NR_removexattr, syscall_not_implemented)
#define __NR_lremovexattr	198
__SYSCALL(__NR_lremovexattr, syscall_not_implemented)
#define __NR_fremovexattr	199
__SYSCALL(__NR_fremovexattr, syscall_not_implemented)
#define __NR_tkill	200
__SYSCALL(__NR_tkill, syscall_not_implemented)
#define __NR_time      201
__SYSCALL(__NR_time, syscall_not_implemented)
#define __NR_futex     202
__SYSCALL(__NR_futex, syscall_not_implemented)
#define __NR_sched_setaffinity    203
__SYSCALL(__NR_sched_setaffinity, syscall_not_implemented)
#define __NR_sched_getaffinity     204
__SYSCALL(__NR_sched_getaffinity, syscall_not_implemented)
#define __NR_set_thread_area	205
__SYSCALL(__NR_set_thread_area, syscall_not_implemented)
#define __NR_io_setup	206
__SYSCALL(__NR_io_setup, syscall_not_implemented)
#define __NR_io_destroy	207
__SYSCALL(__NR_io_destroy, syscall_not_implemented)
#define __NR_io_getevents	208
__SYSCALL(__NR_io_getevents, syscall_not_implemented)
#define __NR_io_submit	209
__SYSCALL(__NR_io_submit, syscall_not_implemented)
#define __NR_io_cancel	210
__SYSCALL(__NR_io_cancel, syscall_not_implemented)
#define __NR_get_thread_area	211
__SYSCALL(__NR_get_thread_area, syscall_not_implemented)
#define __NR_lookup_dcookie	212
__SYSCALL(__NR_lookup_dcookie, syscall_not_implemented)
#define __NR_epoll_create	213
__SYSCALL(__NR_epoll_create, syscall_not_implemented)
#define __NR_epoll_ctl_old	214
__SYSCALL(__NR_epoll_ctl_old, syscall_not_implemented)
#define __NR_epoll_wait_old	215
__SYSCALL(__NR_epoll_wait_old, syscall_not_implemented)
#define __NR_remap_file_pages	216
__SYSCALL(__NR_remap_file_pages, syscall_not_implemented)
#define __NR_getdents64	217
__SYSCALL(__NR_getdents64, syscall_not_implemented)
#define __NR_set_tid_address	218
__SYSCALL(__NR_set_tid_address, syscall_not_implemented)
#define __NR_restart_syscall	219
__SYSCALL(__NR_restart_syscall, syscall_not_implemented)
#define __NR_semtimedop		220
__SYSCALL(__NR_semtimedop, syscall_not_implemented)
#define __NR_fadvise64		221
__SYSCALL(__NR_fadvise64, syscall_not_implemented)
#define __NR_timer_create		222
__SYSCALL(__NR_timer_create, syscall_not_implemented)
#define __NR_timer_settime		223
__SYSCALL(__NR_timer_settime, syscall_not_implemented)
#define __NR_timer_gettime		224
__SYSCALL(__NR_timer_gettime, syscall_not_implemented)
#define __NR_timer_getoverrun		225
__SYSCALL(__NR_timer_getoverrun, syscall_not_implemented)
#define __NR_timer_delete	226
__SYSCALL(__NR_timer_delete, syscall_not_implemented)
#define __NR_clock_settime	227
__SYSCALL(__NR_clock_settime, syscall_not_implemented)
#define __NR_clock_gettime	228
__SYSCALL(__NR_clock_gettime, syscall_not_implemented)
#define __NR_clock_getres	229
__SYSCALL(__NR_clock_getres, syscall_not_implemented)
#define __NR_clock_nanosleep	230
__SYSCALL(__NR_clock_nanosleep, syscall_not_implemented)
#define __NR_exit_group		231
__SYSCALL(__NR_exit_group, syscall_not_implemented)
#define __NR_epoll_wait		232
__SYSCALL(__NR_epoll_wait, syscall_not_implemented)
#define __NR_epoll_ctl		233
__SYSCALL(__NR_epoll_ctl, syscall_not_implemented)
#define __NR_tgkill		234
__SYSCALL(__NR_tgkill, syscall_not_implemented)
#define __NR_utimes		235
__SYSCALL(__NR_utimes, syscall_not_implemented)
#define __NR_vserver		236
__SYSCALL(__NR_vserver, syscall_not_implemented)
#define __NR_mbind 		237
__SYSCALL(__NR_mbind, syscall_not_implemented)
#define __NR_set_mempolicy 	238
__SYSCALL(__NR_set_mempolicy, syscall_not_implemented)
#define __NR_get_mempolicy 	239
__SYSCALL(__NR_get_mempolicy, syscall_not_implemented)
#define __NR_mq_open 		240
__SYSCALL(__NR_mq_open, syscall_not_implemented)
#define __NR_mq_unlink 		241
__SYSCALL(__NR_mq_unlink, syscall_not_implemented)
#define __NR_mq_timedsend 	242
__SYSCALL(__NR_mq_timedsend, syscall_not_implemented)
#define __NR_mq_timedreceive	243
__SYSCALL(__NR_mq_timedreceive, syscall_not_implemented)
#define __NR_mq_notify 		244
__SYSCALL(__NR_mq_notify, syscall_not_implemented)
#define __NR_mq_getsetattr 	245
__SYSCALL(__NR_mq_getsetattr, syscall_not_implemented)
#define __NR_kexec_load 	246
__SYSCALL(__NR_kexec_load, syscall_not_implemented)
#define __NR_waitid		247
__SYSCALL(__NR_waitid, syscall_not_implemented)
#define __NR_add_key		248
__SYSCALL(__NR_add_key, syscall_not_implemented)
#define __NR_request_key	249
__SYSCALL(__NR_request_key, syscall_not_implemented)
#define __NR_keyctl		250
__SYSCALL(__NR_keyctl, syscall_not_implemented)
#define __NR_ioprio_set		251
__SYSCALL(__NR_ioprio_set, syscall_not_implemented)
#define __NR_ioprio_get		252
__SYSCALL(__NR_ioprio_get, syscall_not_implemented)
#define __NR_inotify_init	253
__SYSCALL(__NR_inotify_init, syscall_not_implemented)
#define __NR_inotify_add_watch	254
__SYSCALL(__NR_inotify_add_watch, syscall_not_implemented)
#define __NR_inotify_rm_watch	255
__SYSCALL(__NR_inotify_rm_watch, syscall_not_implemented)
#define __NR_migrate_pages	256
__SYSCALL(__NR_migrate_pages, syscall_not_implemented)
#define __NR_openat		257
__SYSCALL(__NR_openat, syscall_not_implemented)
#define __NR_mkdirat		258
__SYSCALL(__NR_mkdirat, syscall_not_implemented)
#define __NR_mknodat		259
__SYSCALL(__NR_mknodat, syscall_not_implemented)
#define __NR_fchownat		260
__SYSCALL(__NR_fchownat, syscall_not_implemented)
#define __NR_futimesat		261
__SYSCALL(__NR_futimesat, syscall_not_implemented)
#define __NR_newfstatat		262
__SYSCALL(__NR_newfstatat, syscall_not_implemented)
#define __NR_unlinkat		263
__SYSCALL(__NR_unlinkat, syscall_not_implemented)
#define __NR_renameat		264
__SYSCALL(__NR_renameat, syscall_not_implemented)
#define __NR_linkat		265
__SYSCALL(__NR_linkat, syscall_not_implemented)
#define __NR_symlinkat		266
__SYSCALL(__NR_symlinkat, syscall_not_implemented)
#define __NR_readlinkat		267
__SYSCALL(__NR_readlinkat, syscall_not_implemented)
#define __NR_fchmodat		268
__SYSCALL(__NR_fchmodat, syscall_not_implemented)
#define __NR_faccessat		269
__SYSCALL(__NR_faccessat, syscall_not_implemented)
#define __NR_pselect6		270
__SYSCALL(__NR_pselect6, syscall_not_implemented)
#define __NR_ppoll		271
__SYSCALL(__NR_ppoll,	syscall_not_implemented)
#define __NR_unshare		272
__SYSCALL(__NR_unshare,	syscall_not_implemented)
#define __NR_set_robust_list	273
__SYSCALL(__NR_set_robust_list, syscall_not_implemented)
#define __NR_get_robust_list	274
__SYSCALL(__NR_get_robust_list, syscall_not_implemented)
#define __NR_splice		275
__SYSCALL(__NR_splice, syscall_not_implemented)
#define __NR_tee		276
__SYSCALL(__NR_tee, syscall_not_implemented)
#define __NR_sync_file_range	277
__SYSCALL(__NR_sync_file_range, syscall_not_implemented)
#define __NR_vmsplice		278
__SYSCALL(__NR_vmsplice, syscall_not_implemented)
#define __NR_move_pages		279
__SYSCALL(__NR_move_pages, syscall_not_implemented)
#define __NR_utimensat		280
__SYSCALL(__NR_utimensat, syscall_not_implemented)
#define __IGNORE_getcpu		/* implemented as a vsyscall */
#define __NR_epoll_pwait	281
__SYSCALL(__NR_epoll_pwait, syscall_not_implemented)
#define __NR_signalfd		282
__SYSCALL(__NR_signalfd, syscall_not_implemented)
#define __NR_timerfd		283
__SYSCALL(__NR_timerfd, syscall_not_implemented)
#define __NR_eventfd		284
__SYSCALL(__NR_eventfd, syscall_not_implemented)
#define __NR_fallocate		285
__SYSCALL(__NR_fallocate, syscall_not_implemented)

#endif
