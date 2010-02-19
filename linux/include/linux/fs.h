#ifndef _LINUX_FS_H
#define _LINUX_FS_H

#include <lwk/kfs.h>
#include <linux/err.h>
#define file_operations kfs_fops

#include <linux/kdev_t.h>

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
};

struct file_system_type {
	const char *name;
	int (*get_sb) (struct file_system_type *, int,
		const char *, void *, struct vfsmount *);
	void (*kill_sb) (struct super_block *);
	struct file_system_type * next;
};

struct super_operations {
};

extern int get_sb_pseudo(struct file_system_type *, char *,
	const struct super_operations *ops, unsigned long,
	struct vfsmount *mnt);
void kill_litter_super(struct super_block *sb);
extern int register_filesystem(struct file_system_type *);
extern int unregister_filesystem(struct file_system_type *);

extern struct vfsmount *kern_mount_data(struct file_system_type *, void *data);
#define kern_mount(type) kern_mount_data(type, NULL)


extern int register_chrdev_region(dev_t, unsigned, const char *);
extern void unregister_chrdev_region(dev_t, unsigned);
extern int register_chrdev(unsigned int, const char *,
               const struct file_operations *);
extern void unregister_chrdev(unsigned int, const char *);


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

static inline unsigned iminor(const struct inode *inode)
{
	return MINOR(inode->i_rdev);
}


#endif /* _LINUX_FS_H */
