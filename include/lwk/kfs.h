/** \file
 * Kernel virtual filesystem
 */
#ifndef _lwk_kfs_h_
#define _lwk_kfs_h_

#include <lwk/list.h>
#include <lwk/task.h>

/* fcntl.h */
#ifndef O_NONBLOCK
#define O_NONBLOCK      00004000
#endif

extern int get_unused_fd(void);
extern void put_unused_fd(unsigned int fd);
extern struct file *fget(unsigned int fd);
extern void fput(struct file *);
extern void fd_install(unsigned int fd, struct file *file);

struct poll_table_struct;

struct inode
{
	struct htable *		files;
	struct hlist_node	ht_link;
	struct inode *          parent;
	char			name[ 128 ];
	const struct kfs_fops *	fops;
	unsigned int		mode;

	void *			priv;
	size_t			priv_len;
	atomic_t		refs;

	/* for chrdevs and linux compatibility */
	dev_t                   i_rdev;
	struct cdev             *i_cdev;
	struct list_head        i_devices;
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
	long (*unlocked_ioctl) (struct file *, unsigned int,
				unsigned long);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*fasync) (int, struct file *, int);
	int (*mmap) (struct file *, struct vm_area_struct *);
	int (*ioctl)(struct file *, int request, uaddr_t);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	int (*release) (struct inode *, struct file *);
	int (*readdir) (struct file *, uaddr_t, unsigned int);

	struct module *owner; /* can we get rid of the module stuff? */
};

/** Do nothing file operations */
extern struct kfs_fops kfs_default_fops;

#define MAX_PATHLEN		1024

struct file
{
	struct inode *          inode;
	loff_t                  pos;
	unsigned int            f_flags;
	void *                  private_data;
};


extern struct inode *
kfs_mkdirent(struct inode *parent,
	     const char *name,
	     const struct kfs_fops *fops,
	     unsigned mode,
	     void *priv,
	     size_t priv_len);


extern struct inode *
kfs_create(const char *full_filename,
	   const struct kfs_fops *fops,
	   unsigned mode,
	   void *priv,
	   size_t priv_len);


extern void
kfs_destroy(struct inode *inode);


static inline struct file *get_current_file(int fd)
{
	if( fd < 0 || fd > MAX_FILES )
		return 0;
	return current->files[ fd ];
}

extern struct inode * kfs_root;

extern struct inode *
kfs_lookup(struct inode *root,
	   const char *dirname,
	   unsigned create_mode);
extern int
kfs_open(const char *pathname, int flags, mode_t mode, struct file **rv);
extern void kfs_init(void);
extern int kfs_readdir(struct file *, uaddr_t, unsigned int);

#endif
