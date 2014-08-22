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
#ifndef	_LINUX_FILE_H_
#define	_LINUX_FILE_H_

/*
#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/refcount.h>
#include <sys/proc.h>
*/


#include <linux/fs.h>
#include <lwk/kfs.h>

struct linux_file;



extern struct kfs_fops linux_shim_fops;

/* Blow away any abstracted redefines (fs.h) */
#undef file


static inline struct linux_file *
linux_fget(unsigned int fd)
{
	struct file *file;

	file = fdTableFile( current->fdTable, fd );
	int __attribute__((unused)) count;
	count = atomic_add_return( 1, &file->f_count );

	return (struct linux_file *)file->private_data;
}

static inline void
linux_fput(struct linux_file *filp)
{
	struct file *file;

	if (filp->_file == NULL) {
		kfree(filp);
		return;
	}

	file = filp->_file;

	if ( atomic_dec_and_test( &file->f_count ) ) {
	    //kmem_free(file);
	    // kfree(filp);
	}    

}




static inline void
linux_fd_install(unsigned int fd, struct linux_file *filp)
{
	struct file *file;

	file = kmem_alloc(sizeof(*file));
	
	if (file) {

	    memset(file, 0, sizeof(*file));

	    file->f_mode       = filp->f_mode;
	    file->f_op         = &linux_shim_fops;
	    file->private_data = filp;
	    atomic_set(&file->f_count, 1);
	}

	filp->_file = file;

	fd_install(fd, file);
}


static inline struct linux_file *
_alloc_file(int mode, struct file_operations *fops)
{
	struct linux_file *filp;

	filp = kzalloc(sizeof(*filp), GFP_KERNEL);

	if (filp == NULL) {
		return (NULL);
	}

	filp->fops   = fops;
	filp->f_mode = mode;

	return filp;
}

#define	alloc_file(mnt, root, mode, fops)	_alloc_file((mode), (fops))

#define	file	   linux_file
#define	fget	   linux_fget
#define fput       linux_fput
#define fd_install linux_fd_install 

#endif	/* _LINUX_FILE_H_ */
