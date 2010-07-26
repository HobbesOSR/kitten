#ifndef __LINUX_FILE_H
#define __LINUX_FILE_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/rculist.h>
#include <lwk/posix_types.h>
#include <lwk/spinlock.h>
#include <lwk/list.h>

struct file_operations;

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

extern spinlock_t dcache_lock;
#define DCACHE_UNHASHED         0x0010

struct dentry {
	atomic_t d_count;
	unsigned int d_flags;           /* protected by d_lock */
	spinlock_t d_lock;              /* per dentry lock */
	int d_mounted;
	struct hlist_node d_hash;
	struct list_head d_lru;         /* LRU list */
	struct list_head d_subdirs;     /* our children */
	struct list_head d_alias;       /* inode alias list */
	unsigned long d_time;           /* used by d_revalidate */
	struct inode *d_inode;
};

static inline void __d_drop(struct dentry *dentry)
{
	if (!(dentry->d_flags & DCACHE_UNHASHED)) {
		dentry->d_flags |= DCACHE_UNHASHED;
		hlist_del_rcu(&dentry->d_hash);
	}
}

static inline int d_unhashed(struct dentry *dentry)
{
	return (dentry->d_flags & DCACHE_UNHASHED);
}

static inline struct dentry *dget(struct dentry *dentry)
{
	if (dentry) {
		BUG_ON(!atomic_read(&dentry->d_count));
		atomic_inc(&dentry->d_count);
	}
	return dentry;
}

struct vfsmount;
extern void d_instantiate(struct dentry *, struct inode *);
extern struct dentry * dget_locked(struct dentry *);
extern void d_delete(struct dentry *);
extern void dput(struct dentry *);

#endif /* __LINUX_FILE_H */
