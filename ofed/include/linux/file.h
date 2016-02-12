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

#include <arch-generic/fcntl.h>

//struct linux_file;



//extern struct kfs_fops linux_shim_fops;

/* Blow away any abstracted redefines (fs.h) */
//#undef file


static inline struct file *
fget(unsigned int fd)
{
	struct file * file = NULL;

	file = fdTableFile( current->fdTable, fd );
	int __attribute__((unused)) count;
	count = atomic_add_return( 1, &file->f_count );

	return file;
}

static inline void
fput(struct file *filp)
{

	if (filp == NULL) {
		return;
	}


	if ( atomic_dec_and_test( &filp->f_count ) ) {
	    //kmem_free(file);
	    // kfree(filp);
	}    

}



/*
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
*/

#define FASYNC_MAGIC 0x4601

struct fasync_struct {
	struct file          * fa_file;
	int                    magic;
	int                    fa_fd;
	struct fasync_struct * fa_next; /* singly linked list */
	id_t                   aspace_id;
	id_t                   task_id;
};




static inline int
fasync_helper(int                     fd, 
	      struct file           * filp,
	      int                     mode, 
	      struct fasync_struct ** fa) 
{
	int ret = 0;

__lock(&_lock);
	
	if ((mode)) {
		struct fasync_struct * new_fa = NULL;
		struct fasync_struct * tmp_fa = *fa;

		new_fa = kmem_alloc(sizeof(struct fasync_struct));
		memset(new_fa, 0, sizeof(struct fasync_struct));
		
		new_fa->magic     = FASYNC_MAGIC;
		new_fa->aspace_id = current->aspace->id;
		new_fa->task_id   = current->id;
		new_fa->fa_fd     = fd;
		new_fa->fa_file   = filp;

		while (tmp_fa) {

			if (tmp_fa->fa_file == filp) {
				tmp_fa->fa_fd = fd;
				kmem_free(new_fa);
				goto out;
			}
			
			tmp_fa = tmp_fa->fa_next;
		}
		

		new_fa->fa_next   = *fa;

		*fa               = new_fa;
		filp->f_flags    |= FASYNC; 
		ret               = 1; // success

	} else {
		struct fasync_struct * tmp_fa = *fa;
		struct fasync_struct * prv_fa = NULL;

		while (tmp_fa) {
			if (tmp_fa->fa_file != filp) {
				prv_fa = tmp_fa;
				tmp_fa = tmp_fa->fa_next;
				continue;
			}
			
			if (prv_fa) {
				prv_fa->fa_next = tmp_fa->fa_next;
			}
			
			tmp_fa->magic = 0; // poison the memory location

			kmem_free(tmp_fa);

			filp->f_flags &= ~FASYNC;
			ret            =  1; // success
		}
	}
 out:
__unlock(&_lock);

	return ret;
}

static inline void 
kill_fasync(struct fasync_struct ** fa, 
	    int                     sig,
	    int                     band)
{
	
	while (*fa) {
    		struct siginfo s;
		memset(&s, 0, sizeof(struct siginfo));
		
		BUG_ON((*fa)->magic != FASYNC_MAGIC);

		s.si_signo = sig;
		s.si_errno = 0;
		s.si_code  = SI_KERNEL;
		s.si_pid   = 0;
		s.si_uid   = 0;
		
		sigsend((*fa)->aspace_id, (*fa)->task_id, sig, &s);

		*fa = (*fa)->fa_next;
	}
}


static inline struct file *
_alloc_file(int mode, const struct file_operations *fops)
{
	struct file *filp;

	filp = kmem_alloc(sizeof(struct file));	

	if (filp == NULL) {
		return (NULL);
	}

	memset(filp, 0, sizeof(struct file));

	filp->f_op   = fops;
	filp->f_mode = mode;
	atomic_set(&filp->f_count, 1);

	return filp;
}

#define	alloc_file(mnt, root, mode, fops)	_alloc_file((mode), (fops))


//#define	fget	   linux_fget
//#define fput       linux_fput
//#define fd_install linux_fd_install 

#endif	/* _LINUX_FILE_H_ */
