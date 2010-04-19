/** \file
 * Kitten kernel VFS.
 *
 */
#include <lwk/driver.h>
#include <lwk/print.h>
#include <lwk/htable.h>
#include <lwk/list.h>
#include <lwk/string.h>
#include <lwk/task.h>
#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/dev.h>
#include <lwk/stat.h>
#include <lwk/aspace.h>

#ifdef CONFIG_LINUX
#  include <lwk/poll.h>
#endif /* CONFIG_LINUX */

#include <arch/uaccess.h>
#include <arch/vsyscall.h>
#include <arch/atomic.h>

#include <asm-generic/fcntl.h>

/* in-kernel dirent struct: should match the libc dirent semantics */
struct dirent {
	long d_ino;                 /* inode number */
	off_t d_off;                /* offset to next dirent */
	unsigned short d_reclen;    /* length of this dirent */
	char d_name [1];            /* filename (null-terminated) */
};

/** Generate a hash from a filename.
 *
 * \todo Actually perform a hash function!
 */
static uint64_t
kfs_hash_filename(const void * name,
		  size_t       bits)
{
	return 0;
}

/** Look for a filename up to the / character in the search term.
 *
 * \returns 0 if name_in_search == name_in_dir, -1 if lt and +1 if gt.
 */
static int
kfs_compare_filename(const void *name_in_search,
		     const void *name_in_dir)
{
	const char * search = name_in_search;
	const char * dir = name_in_dir;

	if(0)
		printk( "%s: Comparing '%s' to '%s'\n", __func__, search, dir );

	while(1)
	{
		char s = *search++;
		char d = *dir++;

		// Handle end of search term case
		if( s == '/' )
			return d != '\0';

		// Handle end case
		if( s == '\0' && d == '\0' )
			return 0;

		// Handle normal cases
		if( s < d )
			return -1;
		if( s > d )
			return +1;
	}

	// Not reached
}

static struct inode *kfs_root;

static int
kfs_stat(struct inode *inode, uaddr_t buf)
{
	struct stat rv;

	memset(&rv, 0x00, sizeof(struct stat));
	rv.st_mode = inode->mode;
	rv.st_rdev = inode->i_rdev;
	/* TODO: plenty of stuff to report back via stat, but we just
	   don't need it atm ... */

	if(copy_to_user((void *)buf, &rv, sizeof(struct stat)))
	  return -EFAULT;

	return 0;
}

static ssize_t
kfs_default_read(struct file * dir,
		 char *        buf,
		 size_t        len,
		 loff_t *      off
)
{
	size_t offset = 0;
	struct inode * inode;
	struct htable_iter iter = htable_iter( dir->inode->files );
	while( (inode = htable_next( &iter )) )
	{
		int rc = snprintf(
			(char*)buf + offset,
			len - offset,
			"%s\n",
			inode->name
		);

		if( rc > len - offset )
		{
			offset = len-1;
			goto done;
		}

		offset += rc;
	}

done:
	return offset;
}

static ssize_t
kfs_default_write(struct file *	file,
		  const char *	buf,
		  size_t        len,
		  loff_t *      off)
{
	return -1;
}


static int
kfs_default_close(struct file *	file)
{
	return 0;
}


static int
kfs_fill_dirent(uaddr_t buf, unsigned int count,
		struct inode *inode, const char *name, int namelen,
		loff_t offset, unsigned int type)
{
	struct dirent __user *de;
	unsigned short len = ALIGN(offsetof(struct dirent, d_name)
				   + namelen + 2, sizeof(long));
	if(count < len)
		return -EINVAL;

	de = (struct dirent *)buf;
	if(__put_user(offset, &de->d_off))
		return -EFAULT;
	if(__put_user((long)inode, &de->d_ino)) /* fake inode # */
		return -EFAULT;
	if(__put_user(len, &de->d_reclen))
		return -EFAULT;
	if(copy_to_user(de->d_name, name, namelen))
		return -EFAULT;
	if(__put_user(0, de->d_name + namelen))
		return -EFAULT;
	if(__put_user(type, (char __user *)de + len - 1))
		return -EFAULT;

	return len;
}

int
kfs_readdir(struct file * filp, uaddr_t buf, unsigned int count)
{
	struct inode *child;
	struct htable_iter iter = htable_iter( filp->inode->files );
	int res = 0, rv = 0, i;

	/* for directories, we just increase pos by 1 for every
	   child we successfully fill into the dirent buffer;
	   linux uses 0 and 1 for . and .., then inodes for the
	   rest */
	if(filp->pos == 0) {
		rv = kfs_fill_dirent(buf, count, filp->inode,
				     ".", 1, filp->pos + 1,
				     4 /* DT_DIR */);
		if(rv < 0)
			goto out;
		res += rv;
		count -= rv;
		filp->pos++;
		buf += rv;
	}
	if(filp->pos == 1) {
		rv = kfs_fill_dirent(buf, count, filp->inode,
				     "..", 2, filp->pos + 1,
				     4 /* DT_DIR */);
		if(rv < 0)
			goto out;
		res += rv;
		count -= rv;
		filp->pos++;
		buf += rv;
	}
	i = 2;
	while( (child = htable_next( &iter )) )
	{
		if(filp->pos > i++)
			continue;
		rv = kfs_fill_dirent(buf, count, child,
				     child->name,
				     strlen(child->name),
				     filp->pos + 1,
				     (child->mode >> 12) & 15);
		if(rv < 0)
			goto out;
		res += rv;
		count -= rv;
		filp->pos++;
		buf += rv;
	}
 out:
	if(rv < 0 && (rv != -EINVAL || res == 0))
		return rv;
	return res;
}

struct kfs_fops kfs_default_fops =
{
	.lookup		= 0, // use the kfs_lookup
	.read		= kfs_default_read,
	.write		= kfs_default_write,
	.close		= kfs_default_close,
	.readdir	= kfs_readdir,
};

static ssize_t
kfs_int_read(struct file * file,
	     char *        u_buf,
	     size_t        len,
	     loff_t *      off)
{
	char buf[ len > 32 ? 32 : len ];
	ssize_t rc;

	if( file->inode->priv_len == 0 )
		rc = snprintf( buf, sizeof(buf), "%ld",
			       *(long*) file->inode->priv );
	else
		rc = snprintf( buf, sizeof(buf), "%lx",
			       *(long*) file->inode->priv );
	if( rc < 0 )
		return -EFAULT;

	if( copy_to_user( (void*) u_buf, buf, rc+1 ) )
		return -EFAULT;

	return rc;
}

struct kfs_fops kfs_int_fops =
{
	.read		= kfs_int_read,
	.close		= kfs_default_close,
};

static ssize_t
kfs_string_read(struct file * file,
		char *        u_buf,
		size_t        len,
		loff_t *      off
)
{
	if( len > file->inode->priv_len )
		len = file->inode->priv_len;

	if( copy_to_user( (void*) u_buf, file->inode->priv, len ) )
		return -EFAULT;

	return len;
}


struct kfs_fops kfs_string_fops =
{
	.read		= kfs_string_read,
	.close		= kfs_default_close,
};

struct file *
kfs_alloc_file(void)
{
	struct file *file = kmem_alloc(sizeof(struct file));
	memset(file, 0x00, sizeof(struct file));

	return file;
}

int kfs_init_file(struct file * file,unsigned int mode,
		const struct kfs_fops *fop)
{
	file->f_mode = mode;
	file->f_op = fop;
	return 0;
}

static struct inode *
kfs_create_inode(void)
{
	struct inode *inode = kmem_alloc(sizeof(struct inode));
	memset(inode, 0x00, sizeof(struct inode));
	atomic_set(&inode->refs, 0);

	return inode;
}

struct inode *
kfs_mkdirent(struct inode *          parent,
	     const char *            name,
	     const struct kfs_fops * fops,
	     unsigned                mode,
	     void *                  priv,
	     size_t                  priv_len)
{
#if 0
	printk( "%s: Creating '%s' (parent %s)\n",
		__func__,
		name,
		parent ? parent->name : "NONE"
	);
#endif

	struct inode * inode = NULL;
	int new_entry = 0;

	if(parent) {
		if(parent->link_target)
			parent = parent->link_target;
		if(!S_ISDIR(parent->mode)) {
			printk(KERN_WARNING "%s is not a directory.\n", name);
			return NULL;
		}
	}

	// Try a lookup in the parent first, then allocate a new file
	if( parent && name )
		inode = htable_lookup( parent->files, name );
	if( !inode ) {
		inode = kfs_create_inode();

		if (!inode)
			return NULL;
		new_entry = 1;
	} else
		printk( KERN_WARNING "%s: '%s' already exists\n",
			__func__, name );

	// If this is a new allocation, create the directory table
	// \todo Do this only when a sub-file is created
	if( !inode->files && S_ISDIR(mode) )
		inode->files = htable_create(7,
					     offsetof( struct inode, name ),
					     offsetof( struct inode, ht_link ),
					     kfs_hash_filename,
					     kfs_compare_filename);

	// \todo This will overwrite any existing file; should it warn?
	inode->parent	= parent;
	inode->fops	= fops;
	inode->priv	= priv;
	inode->priv_len	= priv_len;
	inode->mode	= mode;
	/* note: refs is initialized to 0 in kfs_create_inode() */

	if( name )
	{
		// Copy up to the first / or nul, stopping before we over run
		int offset = 0;
		while( *name && *name != '/' &&
		       offset < sizeof(inode->name)-1 )
			inode->name[offset++] = *name++;
		inode->name[offset] = '\0';
	}

	if( parent && new_entry )
		htable_add( parent->files, inode );

	return inode;
}


void
kfs_destroy(struct inode *inode)
{
	// Should we check ref counts?
	/* TODO: yes, we should. but not right now. */
	htable_destroy( inode->files );
	kmem_free( inode );
}


struct inode *
kfs_lookup(struct inode *       root,
	   const char *		dirname,
	   unsigned		create_mode)
{
	// Special case -- use the root if root is null
	if( !root )
		root = kfs_root;

	while(1)
	{
		/* resolve possible link */
		if( root->link_target )
			root = root->link_target;

		// Trim any leading / characters
		while( *dirname && *dirname == '/' )
			dirname++;

		// Special case -- if the file is empty, return the root
		if( *dirname == '\0' )
			return root;

		// Find the next slash in the directory name
		char * next_slash = strchr( dirname, '/' );

		// If there is no next slash and we're in create mode,
		// then we have reached the end of the lookup and we
		// return a pointer to the directory that will contain
		// the file once the caller creates it.
		if( !next_slash && create_mode )
			return root;

		// If we have a mount point descend into its lookup routine
		if( root->fops->lookup )
			return root->fops->lookup( root, dirname, create_mode );

		/* current entry is not a directory, so we can't search any
		   further */
		if( !S_ISDIR(root->mode) )
			return NULL;

		// Search for the next / char
		struct inode *child = htable_lookup(root->files, dirname);

		// If it does not exist and we're not auto-creating
		// then return no match
		if( !child && !create_mode )
			return NULL;

		// If it does not exist, but we are auto-creating,
		// create a subdirectory entry for this position with
		// the default operations.
		if( !child )
			child = kfs_mkdirent(root,
					     dirname,
					     &kfs_default_fops,
					     (create_mode & ~S_IFMT) | S_IFDIR,
					     0,
					     0);

		// If we do not have another component to search,
		// we return the found child
		if( !next_slash )
			return child;

		// Move to the next component of the directory
		dirname = next_slash + 1;
		root = child;
	}
}


struct inode *
kfs_create(const char *            full_filename,
	   const struct kfs_fops * fops,
	   unsigned		   mode,
	   void *		   priv,
	   size_t		   priv_len)
{
	struct inode *dir;
	const char *filename = strrchr( full_filename, '/' );
	if( !filename )
	{
		printk( "%s: Non-absolute path name! '%s'\n",
			__func__, full_filename );
		return NULL;
	}

	filename++;

	dir = kfs_lookup( kfs_root, full_filename, 0777 );

	if( !dir )
		return NULL;

	// Create the file entry in the directory
	return kfs_mkdirent(dir, filename, fops, mode, priv, priv_len);
}

struct inode *
kfs_mkdir(char *   name,
	  unsigned mode)
{
	return kfs_create(name,
			  &kfs_default_fops,
			  (mode & ~S_IFMT) | S_IFDIR,
			  0,
			  0);
}

int
get_unused_fd(void)
{
	int fd;
	for( fd=0 ; fd<MAX_FILES ; fd++ )
	{
		if( !current->files[fd] )
			break;
	}

	if( fd == MAX_FILES )
		return -EMFILE;
	return fd;
}

void
put_unused_fd(unsigned int fd)
{
	current->files[fd] = NULL;
}

struct file *
fget(unsigned int fd)
{
	return current->files[fd];
}

void
fput(struct file *file)
{
	/* sync an release everything tied to the file */
}

void
fd_install(unsigned int fd, struct file *file)
{
	current->files[fd] = file;
}

struct file *
kfs_open(struct inode *inode, int flags, mode_t mode)
{
	struct file *file = kfs_alloc_file();
	if(NULL == file)
		return NULL;

	file->f_flags = flags;
	file->f_mode = mode;

	kfs_init_file(file, mode, inode->fops);
	file->inode = inode;
	atomic_inc(&inode->refs);

	return file;
}

#if 0
static void
get_full_path(struct inode *inode, char *buf)
{
  if(inode->parent)
    get_full_path(inode->parent, buf);
  strcat(buf, inode->name);
  strcat(buf, "/");
}
#endif

struct inode *
kfs_link(struct inode *target, struct inode *parent, const char *name)
{
	struct inode *link;

#if 0
	char p1[2048], p2[2048];

	*p1 = 0;
	get_full_path(parent, p1);
	*p2 = 0;
	get_full_path(target, p2);
	printk("linking %s%s -> %s\n", p1, name, p2);
#endif

	if(NULL != htable_lookup( parent->files, name )) {
	  printk(KERN_WARNING "link exists.\n");
	  return NULL;
	}

	link = kfs_create_inode();
	if(NULL == link)
		return NULL;

	strncpy(link->name, name, sizeof(link->name));
	link->name[sizeof(link->name) - 1] = '\0';

	link->mode = 0666 | S_IFLNK;
	if(S_ISDIR(target->mode))
		link->mode |= 0111 | S_IFDIR;

	link->link_target = target;

	link->parent = parent;
	htable_add(parent->files, link);

	return link;
}

void
kfs_close(struct file *file)
{
	if (file->inode)
		atomic_dec(&file->inode->refs);
	kmem_free(file);
}

int
kfs_open_path(const char *pathname, int flags, mode_t mode, struct file **rv)
{
	struct inode * inode = kfs_lookup( kfs_root, pathname, 0 );
	if( !inode )
		return -ENOENT;

	struct file *file = kfs_open(inode, flags, mode);
	if(NULL == file)
		return -ENOMEM;

	if( file->f_op->open
	    &&  file->f_op->open( file->inode , file ) < 0 ) {
		kfs_close(file);
		return -EACCES;
	}

	*rv = file;

	return 0;
}

/** Open system call
 *
 * \todo Implement flags and mode tests.
 */
static int
sys_open(uaddr_t u_pathname,
	 int     flags,
	 mode_t  mode)
{
	int fd;
	char pathname[ MAX_PATHLEN ];
	struct file *file;
	if( strncpy_from_user( pathname, (void*) u_pathname,
			       sizeof(pathname) ) < 0 )
		return -EFAULT;

	if(1)
		printk( "%s: Opening '%s' flags %x mode %x\n",
			__func__,
			pathname,
			flags,
			mode);

	if((fd = kfs_open_path(pathname, flags, mode, &file)) < 0) {
		printk("sys_open failed : %s (%d)\n",pathname,fd);
		return fd;
	}
	fd = get_unused_fd();
	current->files[fd] = file;

	return fd;
}

static ssize_t
sys_write(int     fd,
	  uaddr_t buf,
	  size_t  len)
{
	struct file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if( file->f_op->write )
		return file->f_op->write( file,
						 (const char __user *)buf,
						 len , (loff_t *) NULL );

	printk( KERN_WARNING "%s: fd %d (%s) has no write operation\n",
		__func__, fd, file->inode->name);
	return -EBADF;
}

static ssize_t
sys_read(int     fd,
	 uaddr_t buf,
	 size_t  len)
{
	struct file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if( file->f_op->read )
		return file->f_op->read( file, (char *)buf, len, NULL );

	printk( KERN_WARNING "%s: fd %d has no read operation\n",
		__func__, fd );
	return -EBADF;
}


static ssize_t
sys_close(int fd)
{
	int rv = 0;
	struct file * const file = get_current_file( fd );

	if( !file )
		return -EBADF;

	if( file->f_op->close )
		rv = file->f_op->close( file );

	if(rv == 0) {
		kfs_close(file);
		current->files[ fd ] = 0;
	}

	return rv;
}

static int
sys_dup2(int oldfd,
	 int newfd)
{
	struct file * const oldfile = get_current_file( oldfd );

	if( !oldfile )
		return -EBADF;

	if( newfd < 0 || newfd > MAX_FILES )
		return -EBADF;

	sys_close( newfd );

	current->files[ newfd ] = oldfile;

	return 0;
}


static int
sys_ioctl(int fd,
	  int request,
	  uaddr_t arg)
{
	struct file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if( file->f_op->unlocked_ioctl )
		return file->f_op->unlocked_ioctl( file, request, arg );

	if( file->f_op->ioctl )
		return file->f_op->ioctl( file, request, arg );

	printk("sys_ioctl %s : no ioctl!",file->inode->name);
	return -ENOTTY;
}

static int
sys_fcntl(int fd, int cmd, long arg)
{
	struct file * const file = get_current_file( fd );

	printk( "%s: %d %x %lx\n", __func__, fd, cmd, arg );

	if(NULL == file)
		return -EBADF;

	switch(cmd) {
	case F_GETFL:
		return file->f_flags;
	case F_SETFL:
		/* TODO: implement; currently we need at least O_NONBLOCK for
		   IB verbs interface */
		file->f_flags = arg;
		return 0;
	case F_GETFD:
	case F_SETFD:
		/* TODO: implement; we do need a place for per-fd flags first,
		   though */
		return 0;
	default:
		return -EINVAL;
	}

	/* not reached */
}

static off_t
sys_lseek( int fd, off_t offset, int whence )
{
	struct file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if( file->f_op->lseek )
		return file->f_op->lseek( file, offset, whence );

	printk( KERN_WARNING "%s: fd %d has no lseek operation\n",
		__func__, fd );
	return -EBADF;
}


static int
sys_getdents(unsigned int fd, uaddr_t dirp, unsigned int count)
{
	struct file * const file = get_current_file( fd );
	int res;

	if(NULL == file)
		return -EBADF;

	if(!S_ISDIR(file->inode->mode))
		return -ENOTDIR;

	res = file->f_op->readdir(file, dirp, count);

	return res;
}

static pid_t
sys_fork( void )
{
	printk( "%s: Attempting to fork!\n", __func__ );
	return -ENOSYS;
}

static int
sys_stat( const char *path, uaddr_t buf)
{
	struct inode * const inode = kfs_lookup(NULL, path, 0);
	if( !inode )
		return -EBADF;

	return kfs_stat(inode, buf);
}

static int
sys_fstat(int fd, uaddr_t buf)
{
	struct file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if(file->inode)
		return kfs_stat(file->inode, buf);

	printk(KERN_WARNING
	       "Attempting fstat() on fd %d with no backing inode.\n", fd);
	return 0;
}

#ifdef CONFIG_LINUX
struct pollfd {
	int   fd;         /* file descriptor */
	short events;     /* requested events */
	short revents;    /* returned events */
};

struct poll_list {
	struct poll_list *next;
	int len;
	struct pollfd entries[0];
};

struct poll_table_page {
	struct poll_table_page * next;
	struct poll_table_entry * entry;
	struct poll_table_entry entries[0];
};

typedef unsigned long int nfds_t;
#define POLLFD_PER_PAGE  ((PAGE_SIZE-sizeof(struct poll_list)) / \
				sizeof(struct pollfd))
#define N_STACK_PPS ((sizeof(stack_pps) - sizeof(struct poll_list))  /	\
			sizeof(struct pollfd))
#define POLL_TABLE_FULL(table) \
	((unsigned long)((table)->entry+1) > PAGE_SIZE + (unsigned long)(table))

static struct poll_table_entry *poll_get_entry(struct poll_wqueues *p)
{
	struct poll_table_page *table = p->table;

	if (p->inline_index < N_INLINE_POLL_ENTRIES)
		return p->inline_entries + p->inline_index++;

	if (!table || POLL_TABLE_FULL(table)) {
		struct poll_table_page *new_table;

		new_table = (struct poll_table_page *) __get_free_page(GFP_KERNEL);
		if (!new_table) {
			p->error = -ENOMEM;
			return NULL;
		}
		new_table->entry = new_table->entries;
		new_table->next = table;
		p->table = new_table;
		table = new_table;
	}

	return table->entry++;
}


static int __pollwake(waitq_entry_t *wait, unsigned mode, int sync, void *key)
{
	struct poll_wqueues *pwq = wait->private;
	//DECLARE_WAITQUEUE(dummy_wait, pwq->polling_task);

	/*
	 * Although this function is called under waitqueue lock, LOCK
	 * doesn't imply write barrier and the users expect write
	 * barrier semantics on wakeup functions.  The following
	 * smp_wmb() is equivalent to smp_wmb() in try_to_wake_up()
	 * and is paired with set_mb() in poll_schedule_timeout.
	 */
	smp_wmb();
	pwq->triggered = 1;

	/*
	 * Perform the default wake up operation using a dummy
	 * waitqueue.
	 *
	 * TODO: This is hacky but there currently is no interface to
	 * pass in @sync.  @sync is scheduled to be removed and once
	 * that happens, wake_up_process() can be used directly.
	 */
	return wake_up_process(pwq->polling_task);
}

static int pollwake(waitq_entry_t *wait, unsigned mode, int sync, void *key)
{
	struct poll_table_entry *entry;

	entry = container_of(wait, struct poll_table_entry, wait);
	if (key && !((unsigned long)key & entry->key))
		return 0;
	return __pollwake(wait, mode, sync, key);
}

/* Add a new entry */
static void __pollwait(struct file *filp, wait_queue_head_t *wait_address,
				poll_table *p)
{
	struct poll_wqueues *pwq = container_of(p, struct poll_wqueues, pt);
	struct poll_table_entry *entry = poll_get_entry(pwq);
	if (!entry)
		return;
	//get_file(filp); - add to f_count - not in kitten
	entry->filp = filp;
	entry->wait_address = wait_address;
	entry->key = p->key;
	init_waitqueue_func_entry(&entry->wait, pollwake);
	entry->wait.private = pwq;
	add_wait_queue(wait_address, &entry->wait);
}

static long estimate_accuracy(struct timespec *tv)
{
	/*

	 * TODO: Using realtime setting - because of simplicity.
	 * Realtime tasks get a slack of 0 for obvious reasons.
	 */
		 return 0;
}
void poll_initwait(struct poll_wqueues *pwq)
{
	init_poll_funcptr(&pwq->pt, __pollwait);
	pwq->polling_task = current;
	pwq->triggered = 0;
	pwq->error = 0;
	pwq->table = NULL;
	pwq->inline_index = 0;
}

static void free_poll_entry(struct poll_table_entry *entry)
{
	remove_wait_queue(entry->wait_address, &entry->wait);
	fput(entry->filp);
}

void poll_freewait(struct poll_wqueues *pwq)
{
	struct poll_table_page * p = pwq->table;
	int i;
	for (i = 0; i < pwq->inline_index; i++)
		free_poll_entry(pwq->inline_entries + i);
	while (p) {
		struct poll_table_entry * entry;
		struct poll_table_page *old;

		entry = p->entry;
		do {
			entry--;
			free_poll_entry(entry);
		} while (entry > p->entries);
		old = p;
		p = p->next;
		free_page((unsigned long) old);
	}
}

int poll_schedule_timeout(struct poll_wqueues *pwq, int state,
						  ktime_t *expires, unsigned long slack)
{
	int rc = -EINTR;

	set_current_state(state);
	if (!pwq->triggered) {
		if(!expires)
			/* wait indefinitely */
			schedule();
		else
			schedule_timeout(*expires);
		/* TODO: currently, we can never return EINTR since we don't implement
		   signals in the kernel. once we do, we should consider setting rc to
		   -EINTR here iff our sleep was interrupted by a signal. */
		rc = 0;
	}
	__set_current_state(TASK_RUNNING);

	/*
	 * Prepare for the next iteration.
	 *
	 * The following set_mb() serves two purposes.  First, it's
	 * the counterpart rmb of the wmb in pollwake() such that data
	 * written before wake up is always visible after wake up.
	 * Second, the full barrier guarantees that triggered clearing
	 * doesn't pass event check of the next iteration.  Note that
	 * this problem doesn't exist for the first iteration as
	 * add_wait_queue() has full barrier semantics.
	 */
	set_mb(pwq->triggered, 0);

	return rc;
}


/*
 * Fish for pollable events on the pollfd->fd file descriptor. We're only
 * interested in events matching the pollfd->events mask, and the result
 * matching that mask is both recorded in pollfd->revents and returned. The
 * pwait poll_table will be used by the fd-provided poll handler for waiting,
 * if non-NULL.
 */
static inline unsigned int do_pollfd(struct pollfd *pollfd, poll_table *pwait)
{
	unsigned int mask;
	int fd;

	mask = 0;
	fd = pollfd->fd;
	if (fd >= 0) {
		//int fput_needed;
		struct file * file;

		file = get_current_file( fd ); //fget_light(fd, &fput_needed);
		mask = POLLNVAL;
		if (file != NULL) {
			mask = DEFAULT_POLLMASK;
			if (file->f_op && file->f_op->poll) {
				if (pwait)
					pwait->key = pollfd->events |
							POLLERR | POLLHUP;
				mask = file->f_op->poll(file, pwait);
			}
			/* Mask out unneeded events. */
			mask &= pollfd->events | POLLERR | POLLHUP;
			//fput_light(file, fput_needed);
		}
	}
	pollfd->revents = mask;

	return mask;
}

static int do_poll(unsigned int nfds,  struct poll_list *list,
				   struct poll_wqueues *wait, struct timespec *end_time)
{
	poll_table* pt = &wait->pt;
	ktime_t expire, *to = NULL;
	int timed_out = 0, count = 0;
	unsigned long slack = 0;

	/* Optimise the no-wait case */
	if (end_time && !end_time->tv_sec && !end_time->tv_nsec) {
		pt = NULL;
		timed_out = 1;
	}

	if (end_time && !timed_out)
		slack = estimate_accuracy(end_time);

	for (;;) {
		struct poll_list *walk;

		for (walk = list; walk != NULL; walk = walk->next) {
			struct pollfd * pfd, * pfd_end;

			pfd = walk->entries;
			pfd_end = pfd + walk->len;
			for (; pfd != pfd_end; pfd++) {
				/*
				 * Fish for events. If we found one, record it
				 * and kill the poll_table, so we don't
				 * needlessly register any other waiters after
				 * this. They'll get immediately deregistered
				 * when we break out and return.
				 */
				if (do_pollfd(pfd, pt)) {
					count++;
					pt = NULL;
				}
			}
		}
		/*
		 * All waiters have already been registered, so don't provide
		 * a poll_table to them on the next loop iteration.
		 */
		pt = NULL;
		if (!count) {
			count = wait->error;
			if (signal_pending(current))
				count = -EINTR;
		}
		if (count || timed_out)
			break;

		/*
		 * If this is the first loop and we have a timeout
		 * given, then we convert to ktime_t and set the to
		 * pointer to the expiry value.
		 */
		if (end_time && !to) {
			expire = timespec_to_ktime(*end_time);
			to = &expire;
		}

		if (!poll_schedule_timeout(wait, TASK_INTERRUPTIBLE, to, slack))
			timed_out = 1;
	}
	return count;
}


static int
sys_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	struct poll_wqueues table;
	struct timespec end_time = { 0, 0 }, *end_time_p = NULL;
	int len, fdcount, size, err = -EFAULT;
	long stack_pps[256/sizeof(long)];
	struct poll_list *const head = (struct poll_list *)stack_pps;
	struct poll_list *walk = head;
	unsigned long todo = nfds;

	if (timeout >= 0) {
		/* timeout given in ms */
		end_time.tv_sec = timeout / MSEC_PER_SEC;
		end_time.tv_nsec = NSEC_PER_MSEC * (timeout % MSEC_PER_SEC);
		if (!timespec_valid(&end_time))
			return -EINVAL;
		end_time_p = &end_time;
	}
	/* timeout < 0 signifies infinite timeout: end_time_p remains NULL */

	len = min_t(unsigned int, nfds, N_STACK_PPS);
	for (;;) {
		walk->next = NULL;
		walk->len = len;
		if (!len)
			break;

		if (copy_from_user(walk->entries, fds + nfds-todo,
					sizeof(struct pollfd) * walk->len))
			goto out_fds;

		todo -= walk->len;
		if (!todo)
			break;

		len = min(todo, POLLFD_PER_PAGE);
		size = sizeof(struct poll_list) + sizeof(struct pollfd) * len;
		walk = walk->next = kmem_alloc(size);
		if (!walk) {
			err = -ENOMEM;
			goto out_fds;
		}
	}

	poll_initwait(&table);
	fdcount = do_poll(nfds, head, &table, end_time_p);
	poll_freewait(&table);

	for (walk = head; walk; walk = walk->next) {
		struct pollfd *wfds = walk->entries;
		int j;

		for (j = 0; j < walk->len; j++, fds++)
			if (__put_user(wfds[j].revents, &fds->revents))
				goto out_fds;
	}

	err = fdcount;
out_fds:
	walk = head->next;
	while (walk) {
		struct poll_list *pos = walk;
		walk = walk->next;
		kmem_free(pos);
	}

	return err;
}
#endif /* CONFIG_LINUX */


void
kfs_init( void )
{
	kfs_root = kfs_mkdirent(NULL,
				"root",
				&kfs_default_fops,
				0777 | S_IFDIR,
				0,
				0);

	// assign some system calls
	syscall_register( __NR_open, (syscall_ptr_t) sys_open );
	syscall_register( __NR_close, (syscall_ptr_t) sys_close );
	syscall_register( __NR_write, (syscall_ptr_t) sys_write );
	syscall_register( __NR_read, (syscall_ptr_t) sys_read );
	syscall_register( __NR_dup2, (syscall_ptr_t) sys_dup2 );
	syscall_register( __NR_ioctl, (syscall_ptr_t) sys_ioctl );
	syscall_register( __NR_fcntl, (syscall_ptr_t) sys_fcntl );
	syscall_register( __NR_getdents, (syscall_ptr_t) sys_getdents );
	syscall_register( __NR_stat, (syscall_ptr_t) sys_stat );
	syscall_register( __NR_fstat, (syscall_ptr_t) sys_fstat );
#ifdef CONFIG_LINUX
	syscall_register( __NR_poll, (syscall_ptr_t) sys_poll );
#endif /* CONFIG_LINUX */

	syscall_register( __NR_fork, (syscall_ptr_t) sys_fork );

	// Bring up any kfs drivers that we have linked in
	driver_init_by_name( "kfs", "*" );
}
