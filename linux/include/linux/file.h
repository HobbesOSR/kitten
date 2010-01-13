#ifndef __LINUX_FILE_H
#define __LINUX_FILE_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <lwk/posix_types.h>
#include <linux/fs.h>

/* dcache.h */
/**
 *      dget, dget_locked       -       get a reference to a dentry
 *      @dentry: dentry to get a reference to
 *
 *      Given a dentry or %NULL pointer increment the reference count
 *      if appropriate and return the dentry. A dentry will not be
 *      destroyed when it has references. dget() should never be
 *      called for dentries with zero reference counter. For these cases
 *      (preferably none, functions in dcache.c are sufficient for normal
 *      needs and they take necessary precautions) you should hold dcache_lock
 *      and call dget_locked() instead of dget().
 */

struct dentry {
	atomic_t d_count;
	unsigned int d_flags;           /* protected by d_lock */
	spinlock_t d_lock;              /* per dentry lock */
	int d_mounted;
	struct list_head d_lru;         /* LRU list */
	struct list_head d_subdirs;     /* our children */
	struct list_head d_alias;       /* inode alias list */
	unsigned long d_time;           /* used by d_revalidate */
	struct inode *d_inode;
};


static inline struct dentry *dget(struct dentry *dentry)
{
	if (dentry) {
		BUG_ON(!atomic_read(&dentry->d_count));
		atomic_inc(&dentry->d_count);
	}
	return dentry;
}

struct vfsmount;
#define get_empty_filp kfs_alloc_file
extern struct file *alloc_file(struct vfsmount *, struct dentry *dentry,
		fmode_t mode, const struct file_operations *fop);

#endif /* __LINUX_FILE_H */
