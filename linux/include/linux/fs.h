#ifndef _LINUX_FS_H
#define _LINUX_FS_H

#include <lwk/kfs.h>
#include <linux/err.h>
#define simple_dir_operations kfs_default_fops

#define file_operations kfs_fops
#define i_fop fops
#include <linux/namei.h>
#include <linux/kdev_t.h>
#include <linux/file.h>
#define CHRDEV_MAJOR_HASH_SIZE     255

/* file is open for reading */
#define FMODE_READ              ((__force fmode_t)1)
/* file is open for writing */
#define FMODE_WRITE             ((__force fmode_t)2)
/* file is seekable */
#define FMODE_LSEEK             ((__force fmode_t)4)
/* file can be accessed using pread */
#define FMODE_PREAD             ((__force fmode_t)8)
/* file can be accessed using pwrite */
#define FMODE_PWRITE            ((__force fmode_t)16)
/* File is opened for execution with sys_execve / sys_uselib */
#define FMODE_EXEC              ((__force fmode_t)32)
/* File is opened with O_NDELAY (only set for block devices) */
#define FMODE_NDELAY            ((__force fmode_t)64)
/* File is opened with O_EXCL (only set for block devices) */
#define FMODE_EXCL              ((__force fmode_t)128)
/* File is opened using open(.., 3, ..) and is writeable only for ioctls
   (specialy hack for floppy.c) */
#define FMODE_WRITE_IOCTL       ((__force fmode_t)256)

#define fops_get(fops) \
	(((fops) && try_module_get((fops)->owner) ? (fops) : NULL))
#define fops_put(fops) \
	do { if (fops) module_put((fops)->owner); } while(0)

struct vfsmount;

struct super_block {
	struct dentry           *s_root;
};
struct tree_descr { char *name; const struct file_operations *ops; int mode; };

extern int simple_fill_super(struct super_block *, int, struct tree_descr *);
void deactivate_super(struct super_block *sb);

struct file_system_type {
	const char *name;
	int (*get_sb) (struct file_system_type *, int,
		const char *, void *, struct vfsmount *);
	void (*kill_sb) (struct super_block *);
	struct file_system_type * next;
	struct module *owner;
};

extern int get_sb_single(struct file_system_type *fs_type,
	int flags, void *data,
	int (*fill_super)(struct super_block *, void *, int),
	struct vfsmount *mnt);


struct inode_operations {
	struct dentry * (*lookup) (struct inode *,struct dentry *, struct nameidata *);
};

extern struct inode *new_inode(struct super_block *);

struct super_operations {
};

static inline void file_accessed(struct file *file)
{
	printk("file_accessed: not implemented\n");
/*	if (!(file->f_flags & O_NOATIME))
	touch_atime(file->f_path.mnt, file->f_path.dentry); */
}


extern int get_sb_pseudo(struct file_system_type *, char *,
	const struct super_operations *ops, unsigned long,
	struct vfsmount *mnt);
void kill_litter_super(struct super_block *sb);
extern int register_filesystem(struct file_system_type *);
extern int unregister_filesystem(struct file_system_type *);

extern const struct inode_operations simple_dir_inode_operations;

extern struct vfsmount *kern_mount_data(struct file_system_type *, void *data);
#define kern_mount(type) kern_mount_data(type, NULL)

extern struct file *alloc_file(struct vfsmount *, struct dentry *dentry,
		fmode_t mode, const struct file_operations *fop);

extern int register_chrdev_region(dev_t, unsigned, const char *);
extern void unregister_chrdev_region(dev_t, unsigned);
extern int register_chrdev(unsigned int, const char *,
	       const struct file_operations *);
extern void unregister_chrdev(unsigned int, const char *);
extern int alloc_chrdev_region(dev_t *, unsigned, unsigned, const char *);


struct fasync_struct {
	int     magic;
	int     fa_fd;
	struct  fasync_struct   *fa_next; /* singly linked list */
	struct  file            *fa_file;
};

/* SMP safe fasync helpers: */
extern int fasync_helper(int, struct file *, int, struct fasync_struct **);
/* can be called from interrupts */
extern void kill_fasync(struct fasync_struct **, int, int);
/* only for net: no internal synchronization */
extern void __kill_fasync(struct fasync_struct *, int, int);

extern void iput(struct inode *);
extern struct inode * igrab(struct inode *);

static inline unsigned iminor(const struct inode *inode)
{
	return MINOR(inode->i_rdev);
}

static inline void inc_nlink(struct inode *inode)
{
	inode->i_nlink++;
}

extern ssize_t simple_read_from_buffer(void __user *to, size_t count,
			loff_t *ppos, const void *from, size_t available);

extern int simple_unlink(struct inode *, struct dentry *);
extern int simple_rmdir(struct inode *, struct dentry *);

#endif /* _LINUX_FS_H */
