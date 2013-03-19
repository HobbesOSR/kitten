#ifndef _LWK_TYPES_H
#define _LWK_TYPES_H

#ifdef	__KERNEL__

#define BITS_TO_LONGS(bits) \
	(((bits)+BITS_PER_LONG-1)/BITS_PER_LONG)
#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]

#define BITS_PER_BYTE 8
#endif

#include <lwk/posix_types.h>
#include <arch/types.h>

#ifndef __KERNEL_STRICT_NAMES

#ifdef __KERNEL__
typedef __u32 __kernel_dev_t;

typedef __kernel_fd_set		fd_set;
typedef __kernel_dev_t		dev_t;
typedef __kernel_ino_t		ino_t;
typedef __kernel_mode_t		mode_t;
typedef __kernel_nlink_t	nlink_t;
typedef __kernel_off_t		off_t;
typedef __kernel_pid_t		pid_t;
typedef __kernel_daddr_t	daddr_t;
typedef __kernel_key_t		key_t;
typedef __kernel_suseconds_t	suseconds_t;
typedef __kernel_timer_t	timer_t;
typedef __kernel_clockid_t	clockid_t;
typedef __kernel_mqd_t		mqd_t;
#endif
typedef __kernel_uid_t		uid_t;
typedef __kernel_gid_t		gid_t;
#ifdef __KERNEL__
typedef __kernel_loff_t		loff_t;
#endif

/*
 * The following typedefs are also protected by individual ifdefs for
 * historical reasons:
 */
#ifndef _SIZE_T
#define _SIZE_T
typedef __kernel_size_t		size_t;
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
typedef __kernel_ssize_t	ssize_t;
#endif

#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef __kernel_ptrdiff_t	ptrdiff_t;
#endif

#ifndef _UINTPTR_T
#define _UINTPTR_T
typedef __kernel_uintptr_t	uintptr_t;
#endif

#ifndef _TIME_T
#define _TIME_T
typedef __kernel_time_t		time_t;
#endif

#ifndef _CLOCK_T
#define _CLOCK_T
typedef __kernel_clock_t	clock_t;
#endif

#ifndef _CADDR_T
#define _CADDR_T
typedef __kernel_caddr_t	caddr_t;
#endif

/* bsd */
typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned int		u_int;
typedef unsigned long		u_long;

/* sysv */
typedef unsigned char		unchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;

#ifndef __BIT_TYPES_DEFINED__
#define __BIT_TYPES_DEFINED__

typedef		__u8		u_int8_t;
typedef		__s8		int8_t;
typedef		__u16		u_int16_t;
typedef		__s16		int16_t;
typedef		__u32		u_int32_t;
typedef		__s32		int32_t;

#endif /* !(__BIT_TYPES_DEFINED__) */

/* user-space uses stdint.h for these */
#ifdef __KERNEL__

typedef		__u8		uint8_t;
typedef		__u16		uint16_t;
typedef		__u32		uint32_t;

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
typedef		__u64		uint64_t;
typedef		__u64		u_int64_t;
typedef		__s64		int64_t;
#endif

#endif /* __KERNEL_ */

/* this is a special 64bit data type that is 8-byte aligned */
#define aligned_u64 unsigned long long __attribute__((aligned(8)))

#endif /* __KERNEL_STRICT_NAMES */

#ifdef __KERNEL__
/* _Bool is a base type in C99... what a bad name! */
typedef _Bool			bool;
#else
#include <stdbool.h>
#include <stdint.h>
#endif

/*
 * Below are truly LWK-specific types that should never collide with
 * any application/library that wants lwk/types.h.
 */

/* Address types */
typedef	__kernel_uintptr_t	 addr_t;	/* general address */
typedef	__kernel_uintptr_t	paddr_t;	/* physical address */
typedef	__kernel_uintptr_t	vaddr_t;	/* virtual address */
typedef	__kernel_uintptr_t	kaddr_t;	/* kernel virtual address */
typedef	__kernel_uintptr_t	uaddr_t;	/* user virtual address */

/* Locality group ID */
typedef unsigned int		numa_node_t;

#ifdef __CHECKER__
#define __bitwise__ __attribute__((bitwise))
#else
#define __bitwise__
#endif
#ifdef __CHECK_ENDIAN__
#define __bitwise __bitwise__
#else
#define __bitwise
#endif

typedef __u16 __bitwise __le16;
typedef __u16 __bitwise __be16;
typedef __u32 __bitwise __le32;
typedef __u32 __bitwise __be32;
typedef __u64 __bitwise __le64;
typedef __u64 __bitwise __be64;

#ifdef __KERNEL__
typedef unsigned __bitwise__ gfp_t;

struct ustat {
	__kernel_daddr_t	f_tfree;
	__kernel_ino_t		f_tinode;
	char			f_fname[6];
	char			f_fpack[6];
};
#endif

#endif /* _LWK_TYPES_H */
