#ifndef _LWK_HIO_H
#define _LWK_HIO_H

#include <lwk/init.h>
#include <lwk/xpmem/xpmem.h>

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
#define HIO_MAX_SEGC	16


typedef struct {
	xpmem_segid_t segid;
	uint64_t      size;
	uint64_t      page_size;
	void        * target_vaddr;
} hio_segment_t;

typedef struct {
	uint32_t  uniq_id;

	id_t	  aspace_id;
	id_t	  thread_id;
	id_t	  rank_id;

	uint32_t  syscall_nr;
	uint32_t  argc;
	uintptr_t args[HIO_MAX_ARGC];

	/* Output parameters */
	uintptr_t     ret_val;
	uint32_t      segc;
	hio_segment_t segs[HIO_MAX_SEGC];
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


struct iovec;
struct pollfd;
struct old_utsname;
struct linux_dirent;
struct linux_dirent64;
struct __old_kernel_stat;
struct statfs;
struct stat;
struct sockaddr;
struct msghdr;
struct shmid_ds;

/* Declarations of HIO system calls */
/* Generally these match the Linux defintions in Linux source code include/linux/syscalls.h */
extern long hio_open(const char __user *filename, int flags, int mode);
extern long hio_close(unsigned int fd);
extern long hio_read(unsigned int fd, char __user *buf, size_t count);
extern long hio_write(unsigned int fd, const char __user *buf, size_t count);
extern long hio_readv(unsigned long fd, const struct iovec __user *vec, unsigned long vlen); 
extern long hio_writev(unsigned long fd, const struct iovec __user *vec, unsigned long vlen);
extern long hio_lseek(unsigned int fd, off_t offset, unsigned int origin);
extern long hio_unlink(const char __user *pathname);
extern long hio_readlink(const char __user *path, char __user *buf, int bufsiz);
extern long hio_faccessat(int dfd, const char __user *filename, int mode);
extern long hio_ftruncate(unsigned int fd, unsigned long length);
extern long hio_poll(struct pollfd __user *ufds, unsigned int nfds, int timeout);
extern long hio_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg);
extern long hio_uname(struct old_utsname __user *);
extern long hio_getpid(void);
extern long hio_gettid(void);
extern long hio_set_tid_address(int __user *tidptr);
extern long hio_mmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags, unsigned long fd, unsigned long pgoff); 
extern long hio_munmap(unsigned long addr, size_t len);
extern long hio_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);
extern long hio_openat(int dfd, const char __user *filename, int flags, int mode);
extern long hio_getdents(unsigned int fd, struct linux_dirent __user *dirent, unsigned int count);
extern long hio_getdents64(unsigned int fd, struct linux_dirent64 __user *dirent, unsigned int count);
extern long hio_stat(const char __user *filename, struct __old_kernel_stat __user *statbuf);
extern long hio_statfs(const char __user * path, struct statfs __user *buf);
extern long hio_fstat(unsigned int fd, struct __old_kernel_stat __user *statbuf);
extern long hio_newfstatat(int dfd, const char __user *filename, struct stat __user *statbuf, int flag);
extern long hio_socket(int, int, int);
extern long hio_accept(int, struct sockaddr __user *, int __user *);
extern long hio_sendto(int, void __user *, size_t, unsigned, struct sockaddr __user *, int);
extern long hio_recvfrom(int, void __user *, size_t, unsigned, struct sockaddr __user *, int __user *);
extern long hio_bind(int, struct sockaddr __user *, int);
extern long hio_listen(int, int);
extern long hio_connect(int, struct sockaddr __user *, int);
extern long hio_getsockopt(int fd, int level, int optname, char __user *optval, int __user *optlen);
extern long hio_setsockopt(int fd, int level, int optname, char __user *optval, int optlen);
extern long hio_getsockname(int, struct sockaddr __user *, int __user *);
extern long hio_recvmsg(int fd, struct msghdr __user *msg, unsigned flags);
extern long hio_shmat(int shmid, char __user *shmaddr, int shmflg);
extern long hio_shmget(key_t key, size_t size, int flag);
extern long hio_shmctl(int shmid, int cmd, struct shmid_ds __user *buf);

#endif /* __KERNEL__ */
#endif /* _LWK_HIO_H */
