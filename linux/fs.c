#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/kobj_map.h>
#include <linux/cdev.h>

/* fs/fcntl.c */

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

/* file_table.c */
#include <linux/file.h>

struct file *alloc_file(struct vfsmount *mnt, struct dentry *dentry,
		fmode_t mode, const struct file_operations *fop)
{
	struct file *file = NULL;

//        file = get_empty_filp();
	if (!file)
		return NULL;

//        init_file(file, mnt, dentry, mode, fop);
	return file;
}

/* fs/libfs.c */
int get_sb_pseudo(struct file_system_type *fs_type, char *name,
	const struct super_operations *ops, unsigned long magic,
	struct vfsmount *mnt)
{
	return 0;
}

/* fs/super.c */
void kill_litter_super(struct super_block *sb)
{

}

struct vfsmount *kern_mount_data(struct file_system_type *type, void *data)
{
	return NULL;
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
