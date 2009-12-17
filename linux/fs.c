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
