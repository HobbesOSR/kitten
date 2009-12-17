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
#include <arch/uaccess.h>
#include <arch/vsyscall.h>


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




struct kfs_file * kfs_root;


static ssize_t
kfs_default_read(
	struct kfs_file *	dir,
	char *		buf,
	size_t			len,
	loff_t *                off
)
{
	size_t offset = 0;
	struct kfs_file * file;
	struct htable_iter iter = htable_iter( dir->files );
	while( (file = htable_next( &iter )) )
	{
		int rc = snprintf(
			(char*)buf + offset,
			len - offset,
			"%s\n",
			file->name
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
	struct kfs_file *	file,
	const char *			buf,
	size_t			len,
	loff_t * 		off
)
{
	return -1;
}


static int
kfs_default_close(
	struct kfs_file *	file
)
{
	return 0;
}



struct kfs_fops kfs_default_fops =
{
	.lookup		= 0, // use the kfs_lookup
	.read		= kfs_default_read,
	.write		= kfs_default_write,
	.close		= kfs_default_close,
};


static ssize_t
kfs_int_read(
	struct kfs_file *	file,
	char *			u_buf,
	size_t			len,
	loff_t * 		off
)
{
	char buf[ len > 32 ? 32 : len ];
	ssize_t rc;

	if( file->priv_len == 0 )
		rc = snprintf( buf, sizeof(buf), "%ld", *(long*) file->priv );
	else
		rc = snprintf( buf, sizeof(buf), "%lx", *(long*) file->priv );
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
	struct kfs_file *	file,
	char *			u_buf,
	size_t			len,
	loff_t *		off
)
{
	if( len > file->priv_len )
		len = file->priv_len;

	if( copy_to_user( (void*) u_buf, file->priv, len ) )
		return -EFAULT;

	return len;
}


struct kfs_fops kfs_string_fops = 
{
	.read		= kfs_string_read,
	.close		= kfs_default_close,
};


struct kfs_file *
kfs_mkdirent(
	struct kfs_file *	parent,
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

	struct kfs_file * file = 0;
	int new_entry = 0;

	// Try a lookup in the parent first, then allocate a new file
	if( parent && name )
		file = htable_lookup( parent->files, name );
	if( !file ) {
		file = kmem_alloc( sizeof(*file) );
		if (!file)
			return NULL;
		new_entry = 1;
	} else
		printk( KERN_WARNING "%s: '%s' already exists\n", __func__, name );

	// If this is a new allocation, create the directory table
	// \todo Do this only when a sub-file is created
	if( !file->files )
		file->files = htable_create(
			7,
			offsetof( struct kfs_file, name ),
			offsetof( struct kfs_file, ht_link ),
			kfs_hash_filename,
			kfs_compare_filename
		);

	// \todo This will overwrite any existing file; should it warn?
	file->parent	= parent;
	file->fops	= fops;
	file->priv	= priv;
	file->priv_len	= priv_len;
	file->mode	= mode;
	file->refs	= 0;

	if( name )
	{
		// Copy up to the first / or nul, stopping before we over run
		int offset = 0;
		while( *name && *name != '/' && offset < sizeof(file->name)-1 )
			file->name[offset++] = *name++;
		file->name[offset] = '\0';
	}

	if( parent && new_entry )
		htable_add( parent->files, file );

	return file;
}


void
kfs_destroy(
	struct kfs_file *	file
)
{
	// Should we check ref counts?
	htable_destroy( file->files );
	kmem_free( file );
}


struct kfs_file *
kfs_lookup(
	struct kfs_file *	root,
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

		// Search for the next / char
		struct kfs_file * child = htable_lookup(
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
				create_mode,
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


struct kfs_file *
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

	struct kfs_file * dir = kfs_lookup( kfs_root, full_filename, 0777 );

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

struct kfs_file *
kfs_mkdir(
	char *			name,
	unsigned		mode
)
{
	return kfs_create(
		name,
		&kfs_default_fops,
		mode,
		0,
		0
	);
}

int get_unused_fd(void) 
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

void put_unused_fd(unsigned int fd)
{
	current->files[fd] = NULL;
}

struct kfs_file *fget(unsigned int fd)
{
	return current->files[fd];
}

void fput(struct kfs_file *file)
{
	/* sync an release everything tied to the file */
}

void fd_install(unsigned int fd, struct kfs_file *file)
{
        current->files[fd] = file;
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
	char pathname[ MAX_PATHLEN ];
	if( strncpy_from_user( pathname, (void*) u_pathname, sizeof(pathname) ) < 0 )
		return -EFAULT;

	if(1)
	printk( "%s: Openning '%s' flags %x mode %x\n",
		__func__, 
		pathname,
		flags,
		mode
	);

	struct kfs_file * file = kfs_lookup( kfs_root, pathname, 0 );
	if( !file )
		return -ENOENT;

	int fd = get_unused_fd();

	if( file->fops->open
	&&  file->fops->open( NULL, file ) < 0 )
		return -EACCES;

	current->files[fd] = file;
	file->refs++;

	return fd;
}



static ssize_t
sys_write(
	int			fd,
	uaddr_t			buf,
	size_t			len
)
{
	struct kfs_file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if( file->fops->write )
		return file->fops->write( file, (const char __user *)buf, len , (loff_t *) NULL );

	printk( KERN_WARNING "%s: fd %d has no write operation\n", __func__, fd );
	return -EBADF;
}


static ssize_t
sys_read(
	int			fd,
	uaddr_t			buf,
	size_t			len
)
{
	struct kfs_file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if( file->fops->read )
		return file->fops->read( file, (char *)buf, len, NULL );

	printk( KERN_WARNING "%s: fd %d has no read operation\n", __func__, fd );
	return -EBADF;
}


static ssize_t
sys_close(
	int			fd
)
{
	struct kfs_file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	file->refs--;
	current->files[ fd ] = 0;

	if( file->fops->close )
		return file->fops->close( file );

	return 0;
}


static int
sys_dup2(
	int			oldfd,
	int			newfd
)
{
	struct kfs_file * const oldfile = get_current_file( oldfd );
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
	struct kfs_file * const file = get_current_file( fd );
	if( !file )
		return -EBADF;

	if( file->fops->ioctl )
		return file->fops->ioctl( file, request, arg );

	return -ENOTTY;
}


static pid_t
sys_fork( void )
{
	printk( "%s: Attempting to fork!\n", __func__ );
	return -ENOSYS;
}


void
kfs_init( void )
{
	kfs_root = kfs_mkdirent(
		NULL,
		"root",
		&kfs_default_fops,
		0777,
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

	syscall_register( __NR_fork, (syscall_ptr_t) sys_fork );

	// Bring up any kfs drivers that we have linked in
	driver_init_by_name( "kfs", "*" );
}
