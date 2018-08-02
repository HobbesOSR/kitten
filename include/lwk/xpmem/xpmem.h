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
#include <arch/ioctl.h>


/* Well-known XPMEM segids */
#define XPMEM_MAX_WK_SEGID    31
#define XPMEM_GIT_ROOT_SEGIDS 25

/*
 * basic argument type definitions
 */
typedef int64_t xpmem_segid_t;	/* segid returned from xpmem_make() */
typedef int64_t xpmem_apid_t;	/* apid returned from xpmem_get() */
typedef int64_t xpmem_domid_t;

struct xpmem_addr {
	xpmem_apid_t apid;	/* apid that represents memory */
	off_t offset;		/* offset into apid's memory */
};

#define XPMEM_MAXADDR_SIZE	(size_t)(-1L)
#define XPMEM_MAXNAME_SIZE      128

/*
 * path to XPMEM device
 */
#define XPMEM_DEV_PATH  "/dev/xpmem"

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
 * Valid permit_type values for xpmem_make()/xpmem_get().
 */
#define XPMEM_PERMIT_MODE	0x1
#define XPMEM_GLOBAL_MODE	0x2

/* Valid flags for xpmem_make_hobbes() */
#define XPMEM_MEM_MODE          0x1
#define XPMEM_SIG_MODE          0x2
#define XPMEM_REQUEST_MODE	0x4

/* Valid flags for xpmem_attach() */
#define XPMEM_NOCACHE_MODE	0x1

/*
 * ioctl() commands used to interface to the kernel module.
 */
#define XPMEM_IOC_MAGIC		'x'
#define XPMEM_CMD_VERSION	_IO(XPMEM_IOC_MAGIC, 0)
#define XPMEM_CMD_MAKE		_IO(XPMEM_IOC_MAGIC, 1)
#define XPMEM_CMD_REMOVE	_IO(XPMEM_IOC_MAGIC, 2)
#define XPMEM_CMD_GET		_IO(XPMEM_IOC_MAGIC, 3)
#define XPMEM_CMD_RELEASE	_IO(XPMEM_IOC_MAGIC, 4)
#define XPMEM_CMD_SIGNAL	_IO(XPMEM_IOC_MAGIC, 5)
#define XPMEM_CMD_ATTACH	_IO(XPMEM_IOC_MAGIC, 6)
#define XPMEM_CMD_DETACH	_IO(XPMEM_IOC_MAGIC, 7)
#define XPMEM_CMD_FORK_BEGIN	_IO(XPMEM_IOC_MAGIC, 8)
#define XPMEM_CMD_FORK_END	_IO(XPMEM_IOC_MAGIC, 9)
#define XPMEM_CMD_GET_DOMID	_IO(XPMEM_IOC_MAGIC, 10)

/*
 * Structures used with the preceding ioctl() commands to pass data.
 */
struct xpmem_cmd_make {
	uint64_t vaddr;
	size_t size;
	int flags;
	int permit_type;
	int64_t permit_value;
	xpmem_segid_t segid;	/* returned on success */
	int fd;
};

struct xpmem_cmd_remove {
	xpmem_segid_t segid;
};

struct xpmem_cmd_get {
	xpmem_segid_t segid;
	int flags;
	int permit_type;
	int64_t permit_value;
	xpmem_apid_t apid;	/* returned on success */
};

struct xpmem_cmd_release {
	xpmem_apid_t apid;
};

struct xpmem_cmd_signal {
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

struct xpmem_cmd_domid {
	xpmem_domid_t domid;
};



#ifdef __KERNEL__

#include <lwk/kfs.h>

/* Export the xpmem interface in kernel */
/*
extern int kernel_xpmem_init(void);
extern int kernel_xpmem_finish(void);
*/
extern int xpmem_make(vaddr_t, size_t, int, void *, int, xpmem_segid_t, xpmem_segid_t *, int *);
extern int xpmem_remove(xpmem_segid_t);
extern int xpmem_get(xpmem_segid_t, int, int, void *, xpmem_apid_t *);
extern int xpmem_release(xpmem_apid_t);
extern int xpmem_signal(xpmem_apid_t);
extern int xpmem_attach(xpmem_apid_t, off_t, size_t, vaddr_t, int, vaddr_t *);
extern int xpmem_detach(vaddr_t);

#endif /* __KERNEL__ */


#endif /* _XPMEM_H */
