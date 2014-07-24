/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2004-2007 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * Cross Partition Memory (XPMEM) structures and macros.
 */

#ifndef _XPMEM_H
#define _XPMEM_H

#include <lwk/types.h>
#include <lwk/kfs.h>
#include <arch/ioctl.h>


/*
 * basic argument type definitions
 */
typedef int64_t xpmem_segid_t;	/* segid returned from xpmem_make() */
typedef int64_t xpmem_apid_t;	/* apid returned from xpmem_get() */

struct xpmem_addr {
	xpmem_apid_t apid;	/* apid that represents memory */
	off_t offset;		/* offset into apid's memory */
};

#define XPMEM_MAXADDR_SIZE	(size_t)(-1L)

/*
 * path to XPMEM device
 */
#define XPMEM_DEV_PATH  "/xpmem"

/*
 * The following are the possible XPMEM related errors.
 */
#define XPMEM_ERRNO_NOPROC	2004	/* unknown thread due to fork() */

/*
 * flags for segment permissions
 */
#define XPMEM_RDONLY	0x1
#define XPMEM_RDWR	0x2

/*
 * Valid permit_type values for xpmem_make().
 */
#define XPMEM_PERMIT_MODE	0x1

/*
 * ioctl() commands used to interface to the kernel module.
 */
#define XPMEM_IOC_MAGIC		'x'
#define XPMEM_CMD_VERSION	_IO(XPMEM_IOC_MAGIC, 0)
#define XPMEM_CMD_MAKE		_IO(XPMEM_IOC_MAGIC, 1)
#define XPMEM_CMD_REMOVE	_IO(XPMEM_IOC_MAGIC, 2)
#define XPMEM_CMD_GET		_IO(XPMEM_IOC_MAGIC, 3)
#define XPMEM_CMD_RELEASE	_IO(XPMEM_IOC_MAGIC, 4)
#define XPMEM_CMD_ATTACH	_IO(XPMEM_IOC_MAGIC, 5)
#define XPMEM_CMD_DETACH	_IO(XPMEM_IOC_MAGIC, 6)
#define XPMEM_CMD_FORK_BEGIN	_IO(XPMEM_IOC_MAGIC, 7)
#define XPMEM_CMD_FORK_END	_IO(XPMEM_IOC_MAGIC, 8)

/*
 * Structures used with the preceding ioctl() commands to pass data.
 */
struct xpmem_cmd_make {
	uint64_t vaddr;
	size_t size;
	int permit_type;
	uint64_t permit_value;
	xpmem_segid_t segid;	/* returned on success */
};

struct xpmem_cmd_remove {
	xpmem_segid_t segid;
};

struct xpmem_cmd_get {
	xpmem_segid_t segid;
	int flags;
	int permit_type;
	uint64_t permit_value;
	xpmem_apid_t apid;	/* returned on success */
};

struct xpmem_cmd_release {
	xpmem_apid_t apid;
};

struct xpmem_cmd_attach {
	xpmem_apid_t apid;
	off_t offset;
	size_t size;
	uint64_t vaddr;
	int fd;
	int flags;
};

struct xpmem_cmd_detach {
	uint64_t vaddr;
};


#ifdef __KERNEL__

/*
 * Both the xpmem_segid_t and xpmem_apid_t are of type __s64 and designed
 * to be opaque to the user. Both consist of the same underlying fields.
 *
 * The 'uniq' field is designed to give each segid or apid a unique value.
 * Each type is only unique with respect to itself.
 *
 * An ID is never less than or equal to zero.
 */
struct xpmem_id {
    pid_t tgid;             /* thread group that owns ID */
    unsigned short uniq;  /* this value makes the ID unique */
};

#define XPMEM_MAX_UNIQ_ID   ((1 << (sizeof(short) * 8)) - 1)
#define XPMEM_MAX_UNIQ_APID 256
#define XPMEM_MAX_UNIQ_SEGID XPMEM_MAX_UNIQ_ID - XPMEM_MAX_UNIQ_APID

struct xpmem_partition * get_local_partition(void);


#endif /* __KERNEL__ */


#endif /* _XPMEM_H */
