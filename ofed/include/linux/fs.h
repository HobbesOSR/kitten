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

/* 
 * FreeBSD calls to handle threads blocking on poll...
 *  We're just going to no-op them for now
 */ 
struct selinfo {
};
#define selwakeup(selinfo) 


/* The definitions are identical */
#define file_operations kfs_fops

struct linux_file {
	struct file	*_file;
	struct file_operations * fops;
	void 		*private_data;
	int		f_flags;
	int		f_mode;	/* Just starting mode. */

	struct dentry	*f_dentry;
	//	struct dentry	f_dentry_store;

	struct selinfo	f_selinfo;

	struct fasync_struct	*f_fasync;
	id_t             aspace_id;
};




struct fasync_struct {
	int     magic;
	int     fa_fd;
	struct  fasync_struct   *fa_next; /* singly linked list */
	id_t    aspace_id;
	id_t    task_id;
};



#define	file		linux_file


/* JRL: This should really be a list of structs... 
 *    For now we hope only one thread will every register for it
 */
static inline int
fasync_helper(int fd, struct file *filp,
	      int mode, struct fasync_struct **fa) 
{
	if ((mode)) {
		/* We don't handle multiple recipients */
		BUG_ON((*fa) != NULL);

		*fa              = filp->f_fasync;
		(*fa)->aspace_id = current->aspace->parent->id;
		(*fa)->task_id   = current->id;
	} else {		
		*fa = NULL;
	}
	return 0;
}

static inline void 
kill_fasync(struct fasync_struct **fa, int sig, int band)
{
	if ((*fa) != NULL) {
		struct siginfo s;
		memset(&s, 0, sizeof(struct siginfo));
		
		s.si_signo = SIGIO;
		s.si_errno = 0;
		s.si_code  = SI_KERNEL;
		s.si_pid   = 0;
		s.si_uid   = 0;
		
		sigsend((*fa)->aspace_id, (*fa)->task_id, sig, &s);
	}
}

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



#undef file

#endif	/* _LINUX_FS_H_ */
