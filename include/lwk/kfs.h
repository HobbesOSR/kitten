/** \file
 * Kernel virtual filesystem
 */
#ifndef _lwk_kfs_h_
#define _lwk_kfs_h_

#include <lwk/list.h>
#include <lwk/task.h>
#include <lwk/waitq.h>
#include <lwk/mutex.h>
#include <lwk/fdTable.h>

/* fcntl.h */
#ifndef O_NONBLOCK
#define O_NONBLOCK      00004000
#endif

#ifndef O_EXCL
#define O_EXCL      00000200
#endif

struct iovec;
struct kiocb;
struct poll_table_struct;
typedef u64 blkcnt_t;

struct inode;
struct dentry;
struct nameidata;
struct inode_operations {
#if 0
	struct inode * (*lookup)(
		struct inode *	root,
		const char *		name,
		unsigned		create_mode
	);
#endif
	struct dentry * (*lookup) (struct inode *, struct dentry *,
				   struct nameidata *);

        int (*create) (struct inode *, int mode );
        int (*unlink) (struct inode * );
};

struct inode
{
	struct htable *		files;
	struct hlist_node	ht_link;

	struct inode *          parent;
	char			name[ 128 ];
	unsigned int		mode;
	loff_t			size;

	atomic_t		i_count;

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
	const struct kfs_fops *	i_fop;
	void                  * i_private;
	unsigned int            i_nlink;
	struct inode *link_target;

	void 			*priv;
	int			priv_len;
};
struct vm_area_struct;

typedef int (*dirent_filler)(uaddr_t buf, unsigned int count,
			     struct inode *inode, const char *name, int namelen,
			     loff_t offset, unsigned int type);

struct file;
struct kfs_fops
{
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
	int (*readdir) (struct file *, uaddr_t, unsigned int, dirent_filler f);
	ssize_t (*aio_write) (struct kiocb *, const struct iovec *,
			      unsigned long, loff_t);

	struct module *owner; /* can we get rid of the module stuff? */
};

/** Do nothing file operations */
extern struct kfs_fops kfs_default_fops;

#define MAX_PATHLEN		1024

#define PIPE_END_READ  0
#define PIPE_END_WRITE 1

#define PIPE_BUFFER_MAX 1024

struct pipe {
   	char *        buffer;
	int           amount;       // num chars in buffer
	unsigned char eof;          // EOF?
	waitq_t       buffer_wait;
	spinlock_t    buffer_lock;
	int           ref_count;
};

struct file
{
	struct inode *          inode;
	loff_t                  pos;
        const struct kfs_fops * f_op;
        unsigned int            f_mode;
	unsigned int            f_flags;
	struct dentry *         f_dentry;
	void *                  private_data;
	atomic_t		f_count;
	unsigned char		pipe_end_type;
	struct pipe *		pipe;          // if this is NULL, it's not a pipe
};

static inline bool pipe_buffer_full(struct pipe *p) {
   return (p->amount >= PIPE_BUFFER_MAX);
}

static inline bool pipe_buffer_empty(struct pipe *p) {
   return (p->amount == 0);
}

static inline int pipe_amount_free(struct pipe *p) {
   return (PIPE_BUFFER_MAX - p->amount);
}

static inline struct file *get_current_file(int fd)
{
	return fdTableFile( current->fdTable, fd );
}

extern void kfs_init(void);
extern void kfs_init_stdio(struct task_struct *);

/* kfs inode manipulation */
extern struct inode *kfs_mkdirent(struct inode *,
				  const char *,
				  const struct inode_operations *,
				  const struct kfs_fops *,
				  unsigned,
				  void *,
				  size_t);
extern struct inode *kfs_create(const char *,
				const struct inode_operations *,
			        const struct kfs_fops *,
				unsigned,
				void *,
				size_t);

extern struct inode *kfs_link(struct inode *, struct inode *, const char *);

/* generally useful file ops */
extern int kfs_readdir(struct file *, uaddr_t, unsigned int, dirent_filler f);

extern void kfs_close( struct file* );

#endif /* _lwk_kfs_h_ */
