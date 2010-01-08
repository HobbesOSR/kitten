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
#include <lwk/stat.h>
#include <arch/uaccess.h>
#include <arch/vsyscall.h>
#include <arch/atomic.h>

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
kfs_hash_filename(
	const void *		name,
	size_t			bits
)
{
	//printk( "%s: hashing '%s'\n", __func__, (char*) name );
	return 0;
}

/** Look for a filename up to the / character in the search term.
 *
 * \returns 0 if name_in_search == name_in_dir, -1 if lt and +1 if gt.
 */
static int
kfs_compare_filename(
	const void *		name_in_search,
	const void *		name_in_dir
)
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

struct inode *kfs_root;

static int
kfs_stat(struct inode *inode, uaddr_t buf)
{
	struct stat rv;

	printk("Stat on inode %p (%s)\n", inode, inode?inode->name:"null");

	memset(&rv, 0x00, sizeof(struct stat));
	rv.st_mode = inode->mode;
	/* TODO: plenty of stuff to report back via stat, but we just
	   don't need it atm ... */
	
	if(copy_to_user((void *)buf, &rv, sizeof(struct stat)))
	  return -EFAULT;

	return 0;
}

static ssize_t
kfs_default_read(
	struct file *dir,
	char *       buf,
	size_t       len,
	loff_t *     off
)
{
	/* TODO: honour offset */
	size_t offset = 0;
	struct file * file;
	struct htable_iter iter = htable_iter( dir->inode->files );
	while( (file = htable_next( &iter )) )
	{
		int rc = snprintf(
			(char*)buf + offset,
			len - offset,
			"%s\n",
			file->inode->name
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
kfs_default_write(
	struct file *	file,
	const char *		buf,
	size_t			len,
	loff_t * 		off
)
{
	return -1;
}


static int
kfs_default_close(
	struct file *	file
)
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
	printk("file %p, inode %p, children %p, pos %d\n",
	       filp, filp->inode, (filp->inode?filp->inode->files:0), (int)filp->pos);

	struct inode *child;
	struct htable_iter iter = htable_iter( filp->inode->files );
	int res = 0, rv = 0, i;

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
	.readdir        = kfs_readdir,
};


static ssize_t
kfs_int_read(
	struct file *	file,
	char *		u_buf,
	size_t		len,
	loff_t * 	off
)
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
kfs_string_read(
	struct file *	file,
	char *		u_buf,
	size_t		len,
	loff_t *	off
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

static struct inode *
kfs_create_inode(void)
{
	struct inode *inode = kmem_alloc(sizeof(struct inode));
	atomic_set(&inode->refs, 0);
	return inode;
}

struct inode *
kfs_mkdirent(
	struct inode *	parent,
	const char *		name,
	const struct kfs_fops *	fops,
	unsigned		mode,
	void *			priv,
	size_t			priv_len
)
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

	if(parent && !S_ISDIR(parent->mode)) {
	  printk(KERN_WARNING "%s is not a directory.\n", name);
	  return NULL;
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
		printk( KERN_WARNING "%s: '%s' already exists\n", __func__, name );

	// If this is a new allocation, create the directory table
	// \todo Do this only when a sub-file is created
	if( !inode->files && S_ISDIR(mode) )
		inode->files = htable_create(
			7,
			offsetof( struct inode, name ),
			offsetof( struct inode, ht_link ),
			kfs_hash_filename,
			kfs_compare_filename
		);

	// \todo This will overwrite any existing file; should it warn?
	inode->parent	= parent;
	inode->fops	= fops;
	inode->priv	= priv;
	inode->priv_len	= priv_len;
	inode->mode	= mode;

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
kfs_destroy(
	struct inode *inode
)
{
	// Should we check ref counts?
	/* yes, we should. but not right now. */
	htable_destroy( inode->files );
	kmem_free( inode );
}


struct inode *
kfs_lookup(
	struct inode *          root,
	const char *		dirname,
	unsigned		create_mode
)
{
	// Special case -- use the root if root is null
	if( !root )
		root = kfs_root;

	while(1)
	{
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

		//printk( "%s: Looking for %s in %s\n", __func__, dirname, root->name );
		// If we have a mount point descend into its lookup routine
		if( root->fops->lookup )
			return root->fops->lookup( root, dirname, create_mode );

		/* current entry is not a directory, so we can't search any
		   further */
		if( !S_ISDIR(root->mode) )
			return NULL;

		// Search for the next / char
		struct inode * child = htable_lookup(
			root->files,
			dirname
		);

		// If it does not exist and we're not auto-creating
		// then return no match
		if( !child && !create_mode )
			return NULL;

		// If it does not exist, but we are auto-creating,
		// create a subdirectory entry for this position with
		// the default operations.
		if( !child )
			child = kfs_mkdirent(
				root,
				dirname,
				&kfs_default_fops,
				(create_mode & ~S_IFMT) | S_IFDIR,
				0,
				0
			);

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
kfs_create(
	const char *		full_filename,
	const struct kfs_fops *	fops,
	unsigned		mode,
	void *			priv,
	size_t			priv_len
)
{
	const char *		filename = strrchr( full_filename, '/' );
	if( !filename )
	{
		printk( "%s: Non-absolute path name! '%s'\n", __func__, full_filename );
		return NULL;
	}

	filename++;

#if 0
	printk( "%s: Creating '%s' (%s)\n", __func__, full_filename, filename );
#endif

	struct inode * dir = kfs_lookup( kfs_root, full_filename, 0777 );

	if( !dir )
		return NULL;

	// Create the file entry in the directory
	return kfs_mkdirent(
		dir,
		filename,
		fops,
		mode,
		priv,
		priv_len
	);
}

struct inode *
kfs_mkdir(
	char *			name,
	unsigned		mode
)
{
	return kfs_create(
		name,
		&kfs_default_fops,
		(mode & ~S_IFMT) | S_IFDIR,
		0,
		0
	);
}

int get_unused_fd(void)
{
	/* TODO: O(N) complexity ... */
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

void put_unused_fd(unsigned int fd)
{
	current->files[fd] = NULL;
}

struct file *fget(unsigned int fd)
{
	return current->files[fd];
}

void fput(struct file *file)
{
	/* sync an release everything tied to the file */
}

void fd_install(unsigned int fd, struct file *file)
{
	current->files[fd] = file;
}

static struct file *
kfs_open_file(struct inode *inode)
{
	struct file *file = kmem_alloc(sizeof(struct file));
	if(NULL == file)
		return NULL;
	file->inode = inode;
	atomic_inc(&inode->refs);
	return file;
}

static void
kfs_close_file(struct file *file)
{
	atomic_dec(&file->inode->refs);
	kmem_free(file);
}

int
kfs_open(const char *pathname, int flags, mode_t mode, struct file **rv)
{
	struct inode * inode = kfs_lookup( kfs_root, pathname, 0 );
	if( !inode )
		return -ENOENT;

	struct file *file = kfs_open_file(inode);
	if(NULL == file)
		return -ENOMEM;

	if( inode->fops->open
	    &&  inode->fops->open( file->inode , file ) < 0 ) {
		kfs_close_file(file);
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
sys_open(
	uaddr_t			u_pathname,
	int			flags,
	mode_t			mode
)
{
	int fd;
	char pathname[ MAX_PATHLEN ];
	struct file *file;
	if( strncpy_from_user( pathname, (void*) u_pathname, sizeof(pathname) ) < 0 )
		return -EFAULT;

	if(1)
	printk( "%s: Openning '%s' flags %x mode %x\n",
		__func__,
		pathname,
		flags,
		mode
	);

	if((fd = kfs_open(pathname, flags, mode, &file)) < 0)
		return fd;
	fd = get_unused_fd();
	current->files[fd] = file;

	printk("open fd %d, file %p, inode %p\n", fd, file, file->inode);

	return fd;
}



static ssize_t
sys_write(
	int			fd,
	uaddr_t			buf,
	size_t			len
)
{
	struct file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if( file->inode->fops->write )
		return file->inode->fops->write( file,
						 (const char __user *)buf,
						 len , (loff_t *) NULL );

	printk( KERN_WARNING "%s: fd %d has no write operation\n",
		__func__, fd );
	return -EBADF;
}


static ssize_t
sys_read(
	int			fd,
	uaddr_t			buf,
	size_t			len
)
{
	struct file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if( file->inode->fops->read )
		return file->inode->fops->read( file, (char *)buf, len, NULL );

	printk( KERN_WARNING "%s: fd %d has no read operation\n",
		__func__, fd );
	return -EBADF;
}


static ssize_t
sys_close(
	int			fd
)
{
	int rv = 0;
	struct file * const file = get_current_file( fd );

	if( !file )
		return -EBADF;

	if( file->inode->fops->close )
		rv = file->inode->fops->close( file );

	if(rv == 0) {
		kfs_close_file(file);
		current->files[ fd ] = 0;
	}

	return rv;
}


static int
sys_dup2(
	int			oldfd,
	int			newfd
)
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
sys_ioctl(
	int			fd,
	int			request,
	uaddr_t			arg
)
{
	struct file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if( file->inode->fops->ioctl )
		return file->inode->fops->ioctl( file, request, arg );

	return -ENOTTY;
}

static int 
sys_fcntl(int fd, int cmd, long arg) 
{
	printk( "%s: %d %x %lx\n",__func__, fd, cmd, arg );
	return 0;
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
	res = file->inode->fops->readdir(file, dirp, count);

	printk( "%s: %d %p %d %d\n", __func__, fd, (void *)dirp, count, res );
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
sys_fstat( int fd, uaddr_t buf)
{
	struct file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	printk("fstat fd %d, file %p, inode %p\n", fd, file, file->inode);
	if(file->inode)
	  return kfs_stat(file->inode, buf);

	printk(KERN_WARNING
	       "Attempting fstat() on fd %d with no backing inode.\n", fd);
	return 0;
}

void
kfs_init( void )
{
	kfs_root = kfs_mkdirent(
		NULL,
		"root",
		&kfs_default_fops,
		0777 | S_IFDIR,
		0,
		0
	);

	// Assign some system calls
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

	syscall_register( __NR_fork, (syscall_ptr_t) sys_fork );

	// Bring up any kfs drivers that we have linked in
	driver_init_by_name( "kfs", "*" );
}
