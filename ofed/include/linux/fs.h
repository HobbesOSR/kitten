/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUX_FS_H_
#define	_LINUX_FS_H_

#include <lwk/kfs.h>
#include <lwk/stat.h>
#include <lwk/dev.h>
#include <lwk/aspace.h>
//#include <arch/current.h>

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/semaphore.h>

struct module;
struct kiocb;
struct iovec;
struct dentry;
struct page;
struct file_lock;
struct pipe_inode_info;
struct vm_area_struct;
struct poll_table_struct;
struct files_struct;





typedef struct files_struct *fl_owner_t;


struct dentry {
	struct inode	*d_inode;
};



/* The definitions are identical */
#define file_operations kfs_fops



typedef int (*filldir_t)(void *, const char *, int, loff_t, u64, unsigned);




#define	fops_get(fops)	(fops)

/* file is open for reading */
#define FMODE_READ              ((__force fmode_t)1)
/* file is open for writing */
#define FMODE_WRITE             ((__force fmode_t)2)
/* File is opened for execution with sys_execve / sys_uselib */
#define FMODE_EXEC              ((__force fmode_t)32)

static inline int
register_chrdev_region(dev_t dev, unsigned range, const char *name)
{

	return 0;
}

static inline void
unregister_chrdev_region(dev_t dev, unsigned range)
{

	return;
}

static inline dev_t
iminor(struct inode *inode)
{

	return MINOR(inode->i_rdev);
}

static inline struct inode *
igrab(struct inode *inode)
{
    /*
	int error;

	error = vget(inode, 0, curthread);
	if (error)
		return (NULL);
    */
	return (inode);
}

static inline void
iput(struct inode *inode)
{
    /*
	vrele(inode);
    */
}





#endif	/* _LINUX_FS_H_ */
