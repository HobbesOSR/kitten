#ifndef _LWK_HIO_H
#define _LWK_HIO_H

#include <lwk/init.h>

#include <arch/bitops.h>

/**
 * We define a fixed size syscall_mask, user_syscall_mask_t, for use by
 * user-space.  This allows the kernel to change its notion of the number of
 * system calls supported, __NR_syscall_max, without breaking compatibility
 * with user-space.   */

#define SYSCALL_MIN_ID 0     // Do not change this
#define SYSCALL_MAX_ID 1023  // SYSCALL_MAX_ID+1 must be >= __NR_syscall_max and
                             // SYSCALL_MAX_ID must be multiple of (sizeof(unsigned long)*8)
typedef struct {
	unsigned long bits[(SYSCALL_MAX_ID+1)/(sizeof(unsigned long)*8)];
} user_syscall_mask_t;


#define HIO_MAX_ARGC	16

typedef struct {
	id_t	    aspace_id;
	id_t	    thread_id;
	id_t	    rank_id;

	uint32_t    syscall_nr;
	uint32_t    argc;
	uintptr_t   args[HIO_MAX_ARGC];
	uintptr_t   ret_val;

	uint32_t    uniq_id;
} hio_syscall_t;



#ifndef __KERNEL__

#include <lwk/cpumask.h>

#define syscall_set	cpu_set
#define syscall_clear	cpu_clear
#define syscalls_clear	cpus_clear
#define syscall_isset	cpu_isset

#else

#include <lwk/macros.h>
#include <lwk/bitmap.h>

#include <arch/asm-offsets.h>

#if (__NR_syscall_max > SYSCALL_MAX_ID+1)
#error "__NR_syscall_max must be <= SYSCALL_MAX_ID+1"
#endif

typedef struct syscall_mask { DECLARE_BITMAP(bits, __NR_syscall_max); } syscall_mask_t;

static inline user_syscall_mask_t
syscall_mask_kernel2user(const syscall_mask_t *kernel)
{
	user_syscall_mask_t user;

	memset(&user, 0, sizeof(user));
	memcpy(&user, kernel, min(sizeof(syscall_mask_t), sizeof(user_syscall_mask_t)));

	return user;
}

static inline syscall_mask_t
syscall_mask_user2kernel(const user_syscall_mask_t *user)
{
	syscall_mask_t kernel;

	memset(&kernel, 0, sizeof(kernel));
	memcpy(&kernel, user, min(sizeof(syscall_mask_t), sizeof(user_syscall_mask_t)));

	return kernel;
}

#define syscall_set(s, mask)	set_bit(s, mask.bits)
#define syscall_clear(s, mask)	clear_bit(s, mask.bits)
#define syscalls_clear(mask)	bitmap_zero(mask.bits, __NR_syscall_max)
#define syscall_isset(s, mask)	test_bit(s, mask.bits)


struct hio_implementation {
	int (*init)(void);
	int (*notify_new_syscall)(void);
};

void __init
hio_syscall_init(void);


/* Format and execute an HIO system call */
uintptr_t
hio_format_and_exec_syscall(uint32_t syscall_nr, uint32_t argc, ...);

/* Execute an HIO system call */
int
hio_issue_syscall(hio_syscall_t * new_syscall);

/* Cancel a previously issued system call */
void
hio_cancel_syscall(hio_syscall_t * syscall);

/* Return a completed HIO system call */
void
hio_return_syscall(hio_syscall_t * finished_syscall);

/* Returns number of pending system calls */
uint32_t
hio_get_num_pending_syscalls(void);

/* Returns new syscall to execute */
int
hio_get_pending_syscall(hio_syscall_t ** pending_syscall);


typedef uint32_t socklen_t;
struct sockaddr;
struct msghdr;

/* Declarations of HIO system calls */
extern int
hio_open(uaddr_t, int, mode_t);

extern int
hio_close(int);

extern ssize_t
hio_read(int, char __user *, size_t);

extern ssize_t
hio_write(int, uaddr_t, size_t);

extern off_t
hio_lseek( int fd, off_t offset, int whence );

extern int
hio_fcntl( unsigned int fd, unsigned int cmd, unsigned long arg );

int
hio_uname(struct utsname __user *name);

int
hio_getpid(void);

int
hio_gettid(void);

extern long
hio_mmap(unsigned long, unsigned long, unsigned long,
	 unsigned long, unsigned long, unsigned long);

extern int
hio_munmap(unsigned long, size_t);

extern int
hio_ioctl(int, int, uaddr_t);

extern int
hio_openat(int, uaddr_t, int, mode_t);

extern int
hio_getdents(unsigned int, uaddr_t, unsigned int);

extern int
hio_getdents64(unsigned int, uaddr_t, unsigned int);

extern int
hio_stat(const char *, uaddr_t);

extern int
hio_newfstatat(int, uaddr_t, uaddr_t, int);

extern int
hio_socket(int, int, int);

extern int
hio_accept(int, struct sockaddr *, socklen_t);

extern int
hio_bind(int, const struct sockaddr *, socklen_t);

extern int
hio_listen(int, int);

extern int
hio_connect(int, const struct sockaddr *, socklen_t);

extern int
hio_getsockopt(int, int, int, void *, socklen_t *);

extern int
hio_setsockopt(int, int, int, const void *, socklen_t);

extern int
hio_getsockname(int, struct sockaddr *, socklen_t *);

extern ssize_t
hio_sendto(int, const void *, size_t, int, const struct sockaddr *, 
	socklen_t);

extern ssize_t
hio_recvmsg(int, struct msghdr *, int);


#endif /* __KERNEL__ */
#endif /* _LWK_HIO_H */
