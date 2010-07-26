#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/kobj_map.h>
#include <linux/cdev.h>
#include <linux/mount.h>
#include <linux/string.h>
/* fs/fcntl.c */

DEFINE_SPINLOCK(dcache_lock);

int fasync_helper(int fd, struct file * filp, int on, struct fasync_struct **fapp)
{
	return 0;
}

void __kill_fasync(struct fasync_struct *fa, int sig, int band)
{

}

void kill_fasync(struct fasync_struct **fp, int sig, int band)
{
	/* First a quick test without locking: usually
	 * the list is empty.
	 */
	if (*fp) {
		__kill_fasync(*fp, sig, band);
	}
}

/* fs/libfs.c */
int get_sb_pseudo(struct file_system_type *fs_type, char *name,
	const struct super_operations *ops, unsigned long magic,
	struct vfsmount *mnt)
{
	struct inode *root;
	struct dentry *dentry;

	root =  kmem_alloc(sizeof(struct inode));
	dentry =  kmem_alloc(sizeof(struct dentry));
	root->mode = S_IFDIR | S_IRUSR | S_IWUSR;
	dentry->d_inode = root;
	atomic_set(&dentry->d_count, 1);
	mnt->mnt_root = dentry;
	return 0;
}

/* fs/super.c */
void kill_litter_super(struct super_block *sb)
{

}

int simple_fill_super(struct super_block *s, int magic, struct tree_descr *files)
{
	printk("simple_fill_super: not implemented\n");
	return 0;
}

void deactivate_super(struct super_block *s)
{
	printk("deactivate_super: not implemented\n");
}

int get_sb_single(struct file_system_type *fs_type,
		int flags, void *data,
		int (*fill_super)(struct super_block *, void *, int),
		struct vfsmount *mnt)
{
	return 0;
}


struct vfsmount *kern_mount_data(struct file_system_type *type, void *data)
{
	struct vfsmount *mnt;
	int error;
	mnt = kmem_alloc(sizeof(struct vfsmount));
	mnt->mnt_devname = kstrdup(type->name, GFP_KERNEL);
	/* get_sb sets  mnt->mnt_root; */
	error = type->get_sb(type, (1<<22), type->name, NULL, mnt);
	/* if (error)
	   remove allocated space, return Error */
	return mnt;
}

/* fs/filesystem.c */

static struct file_system_type *file_systems;
static DEFINE_RWLOCK(file_systems_lock);


static struct file_system_type **find_filesystem(const char *name, unsigned len)
{
	struct file_system_type **p;
	for (p=&file_systems; *p; p=&(*p)->next)
		if (strlen((*p)->name) == len &&
		    strncmp((*p)->name, name, len) == 0)
			break;
	return p;
}

/**
 *	register_filesystem - register a new filesystem
 *	@fs: the file system structure
 *
 *	Adds the file system passed to the list of file systems the kernel
 *	is aware of for mount and other syscalls. Returns 0 on success,
 *	or a negative errno code on an error.
 *
 *	The &struct file_system_type that is passed is linked into the kernel
 *	structures and must not be freed until the file system has been
 *	unregistered.
 */

int register_filesystem(struct file_system_type * fs)
{
	int res = 0;
	struct file_system_type ** p;

	BUG_ON(strchr(fs->name, '.'));
	if (fs->next)
		return -EBUSY;
//	INIT_LIST_HEAD(&fs->fs_supers);
	write_lock(&file_systems_lock);
	p = find_filesystem(fs->name, strlen(fs->name));
	if (*p)
		res = -EBUSY;
	else
		*p = fs;
	write_unlock(&file_systems_lock);
	return res;
}

EXPORT_SYMBOL(register_filesystem);

/**
 *	unregister_filesystem - unregister a file system
 *	@fs: filesystem to unregister
 *
 *	Remove a file system that was previously successfully registered
 *	with the kernel. An error is returned if the file system is not found.
 *	Zero is returned on a success.
 *
 *	Once this function has returned the &struct file_system_type structure
 *	may be freed or reused.
 */

int unregister_filesystem(struct file_system_type * fs)
{
	struct file_system_type ** tmp;

	write_lock(&file_systems_lock);
	tmp = &file_systems;
	while (*tmp) {
		if (fs == *tmp) {
			*tmp = fs->next;
			fs->next = NULL;
			write_unlock(&file_systems_lock);
			return 0;
		}
		tmp = &(*tmp)->next;
	}
	write_unlock(&file_systems_lock);
	return -EINVAL;
}

EXPORT_SYMBOL(unregister_filesystem);

struct inode *igrab(struct inode *inode) {
	return inode;
}
void iput(struct inode *inode) {

}

/**
 * d_delete - delete a dentry
 * @dentry: The dentry to delete
 *
 * Turn the dentry into a negative dentry if possible, otherwise
 * remove it from the hash queues so it can be deleted later
 */
void d_delete(struct dentry * dentry)
{
	printk("d_delete : not implemented\n");
}

/* This should be called _only_ with dcache_lock held */

static inline struct dentry * __dget_locked(struct dentry *dentry)
{
	atomic_inc(&dentry->d_count);
	printk("__dget_locked : not properly implemented\n");
	/* dentry_lru_del_init(dentry); */
	return dentry;
}

struct dentry * dget_locked(struct dentry *dentry)
{
	return __dget_locked(dentry);
}

void dput(struct dentry *dentry)
{
}

int d_invalidate(struct dentry * dentry)
{
	printk("d_invalidate: not implemented\n");
	return 0;
}

void d_instantiate(struct dentry *entry, struct inode * inode)
{
	printk("d_instantiate: not implemented\n");
}


struct dentry *lookup_one_len(const char *name, struct dentry *base, int len)
{
	printk("lookup_one_len : not implemented\n");
	return NULL;
}

struct inode *new_inode(struct super_block *sb)
{
	printk("new_inode: not implemented\n");
	return NULL;
}

void synchronize_sched(void)
{
	cond_resched();
}

ssize_t simple_read_from_buffer(void __user *to, size_t count, loff_t *ppos,
				const void *from, size_t available)
{
	loff_t pos = *ppos;
	size_t ret;

	if (pos < 0)
		return -EINVAL;
	if (pos >= available || !count)
		return 0;
	if (count > available - pos)
		count = available - pos;
	ret = copy_to_user(to, from + pos, count);
	if (ret == count)
		return -EFAULT;
	count -= ret;
	*ppos = pos + count;
	return count;
}

int simple_rmdir(struct inode *dir, struct dentry *dentry)
{
	printk("simple_rmdir: not implemented\n");
/*	if (!simple_empty(dentry))
		return -ENOTEMPTY;

	drop_nlink(dentry->d_inode);
	simple_unlink(dir, dentry);
	drop_nlink(dir); */
	return 0;
}

int simple_unlink(struct inode *dir, struct dentry *dentry)
{
	printk("simple_unlink: not implemented\n");
/*	struct inode *inode = dentry->d_inode;

	inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	drop_nlink(inode);
	dput(dentry); */
	return 0;
}
struct dentry *simple_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
/*	static const struct dentry_operations simple_dentry_operations = {
		.d_delete = simple_delete_dentry,
	};

	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);
	dentry->d_op = &simple_dentry_operations;
	d_add(dentry, NULL); */
	printk("simple_lookup: not implemented\n");
	return NULL;
}

const struct inode_operations simple_dir_inode_operations = {
	.lookup         = simple_lookup,
};
