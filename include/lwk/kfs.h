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
extern struct kfs_file *fget(unsigned int fd);
extern void fput(struct kfs_file *);
extern void fd_install(unsigned int fd, struct kfs_file *file);

struct poll_table_struct;

struct inode
{
	dev_t                   i_rdev;
	struct cdev             *i_cdev;
	struct list_head        i_devices;
};
struct vm_area_struct;

struct kfs_fops
{
	struct kfs_file * (*lookup)(
		struct kfs_file *	root,
		const char *		name,
		unsigned		create_mode
	);

/*	int (*open)(
		struct kfs_file *
	);
*/
	int (*open) (struct inode *, struct kfs_file *);

	int (*close)(
		struct kfs_file *
	);
/*
	ssize_t (*read)(
		struct kfs_file *,
		uaddr_t,
		size_t
	);

	ssize_t (*write)(
		struct kfs_file *,
		uaddr_t,
		size_t
	);
*/
	ssize_t (*write) (struct kfs_file *, const char __user *, size_t, loff_t *);
	ssize_t (*read) (struct kfs_file *, char __user *, size_t, loff_t *);
	long (*unlocked_ioctl) (struct kfs_file *, unsigned int, unsigned long);
	long (*compat_ioctl) (struct kfs_file *, unsigned int, unsigned long);
	int (*fasync) (int, struct kfs_file *, int);
	int (*mmap) (struct kfs_file *, struct vm_area_struct *);
	int (*ioctl)(
		struct kfs_file *,
		int request,
		uaddr_t
	);
	struct module *owner;
	unsigned int (*poll) (struct kfs_file *, struct poll_table_struct *);
	int (*release) (struct inode *, struct kfs_file *);
};

/** Do nothing file operations */
extern struct kfs_fops kfs_default_fops;

#define MAX_PATHLEN		1024

struct kfs_file
{
	struct htable *		files;
	struct hlist_node	ht_link;

	struct kfs_file *	parent;
	char			name[ 128 ];
	const struct kfs_fops *	fops;
	unsigned		mode;
	void *			priv;
	size_t			priv_len;
	unsigned		refs;
	/* for IB support */
	void *                  private_data;
	unsigned int            f_flags;
	struct inode *          inode;
};


extern struct kfs_file *
kfs_mkdirent(
	struct kfs_file *	parent,
	const char *		name,
	const struct kfs_fops *	fops,
	unsigned		mode,
	void *			priv,
	size_t			priv_len
);


struct kfs_file *
kfs_create(
	const char *		full_filename,
	const struct kfs_fops *	fops,
	unsigned		mode,
	void *			priv,
	size_t			priv_len
);


void
kfs_destroy(
	struct kfs_file *	file
);


static inline struct kfs_file *
get_current_file(
	int			fd
)
{
	if( fd < 0 || fd > MAX_FILES )
		return 0;
	return current->files[ fd ];
}


extern struct kfs_file * kfs_root;


extern struct kfs_file *
kfs_lookup(
	struct kfs_file *	root,
	const char *		dirname,
	unsigned		create_mode
);


extern void
kfs_init( void );


#endif
