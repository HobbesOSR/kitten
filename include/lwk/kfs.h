/** \file
 * Kernel virtual filesystem
 */
#ifndef _lwk_kfs_h_
#define _lwk_kfs_h_

#include <lwk/list.h>
#include <lwk/task.h>
#include <lwk/mutex.h>

/* fcntl.h */
#ifndef O_NONBLOCK
#define O_NONBLOCK      00004000
#endif

struct iovec;
struct kiocb;
struct poll_table_struct;
typedef u64 blkcnt_t;

struct inode
{
	struct htable *		files;
	struct hlist_node	ht_link;

	struct inode *          parent;
	char			name[ 128 ];
	const struct kfs_fops *	fops;
	unsigned int		mode;
	loff_t			size;

	void *			priv;
	size_t			priv_len;
	atomic_t		refs;

	/* for chrdevs and linux compatibility */
	dev_t                   i_rdev;
	struct cdev             *i_cdev;
	struct list_head        i_devices;

	/* for qib driver combpatibility */
	struct super_block      *i_sb;
	umode_t                 i_mode;
	uid_t                   i_uid;
	gid_t                   i_gid;
	blkcnt_t                i_blocks;
	struct timespec         i_atime;
	struct timespec         i_mtime;
	struct timespec         i_ctime;
	const struct inode_operations   *i_op;
	void                    *i_private;
	struct mutex            i_mutex;
	unsigned int            i_nlink;



	struct inode *link_target;
};
struct vm_area_struct;

struct kfs_fops
{
	struct inode * (*lookup)(
		struct inode *	root,
		const char *		name,
		unsigned		create_mode
	);

	int (*open) (struct inode *, struct file *);

	int (*close)(
		struct file *
	);
	ssize_t (*write) (struct file *, const char __user *, size_t,
			  loff_t *);
	ssize_t (*read) (struct file *, char __user *, size_t,
			 loff_t *);
	off_t (*lseek) ( struct file*, off_t, int );
	long (*unlocked_ioctl) (struct file *, unsigned int,
				unsigned long);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*fasync) (int, struct file *, int);
	int (*mmap) (struct file *, struct vm_area_struct *);
	int (*ioctl)(struct file *, int request, uaddr_t);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	int (*release) (struct inode *, struct file *);
	int (*readdir) (struct file *, uaddr_t, unsigned int);
	ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);

	struct module *owner; /* can we get rid of the module stuff? */
};

/** Do nothing file operations */
extern struct kfs_fops kfs_default_fops;

#define MAX_PATHLEN		1024

struct file
{
	struct inode *          inode;
	loff_t                  pos;
        const struct kfs_fops * f_op;
        unsigned int            f_mode;
	unsigned int            f_flags;
	struct dentry *f_dentry;
	void *                  private_data;
};

static inline struct file *get_current_file(int fd)
{
	if( fd < 0 || fd > MAX_FILES )
		return 0;
	return current->files[ fd ];
}

extern void kfs_init(void);

/* kfs inode manipulation */
extern struct inode *kfs_mkdirent(struct inode *,
				  const char *,
				  const struct kfs_fops *,
				  unsigned,
				  void *,
				  size_t);
extern struct inode *kfs_create(const char *,
				const struct kfs_fops *,
				unsigned,
				void *,
				size_t);
extern struct inode *kfs_lookup(struct inode *,
				const char *,
				unsigned);
extern struct inode *kfs_mkdir(char *,
			       unsigned);

extern struct inode *kfs_create_inode(void);
extern void kfs_destroy(struct inode *);
extern struct inode *kfs_link(struct inode *, struct inode *, const char *);

/* kfs file manipulation */
extern struct file *kfs_alloc_file(void);
extern int kfs_init_file(struct file *,unsigned int mode,
		const struct kfs_fops *fop);
extern struct file *kfs_open(struct inode *, int flags, mode_t mode);
extern void kfs_close(struct file *);
extern int kfs_open_path(const char *pathname, int flags, mode_t mode,
			 struct file **rv);
/* generally useful file ops */
extern int kfs_readdir(struct file *, uaddr_t, unsigned int);

/* current task file table manipulation */
extern int get_unused_fd(void);
extern void put_unused_fd(unsigned int fd);
extern struct file *fget(unsigned int fd);
extern void fput(struct file *);
extern void fd_install(unsigned int fd, struct file *file);

#endif /* _lwk_kfs_h_ */
