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

static uint64_t
kfs_hash_filename(
	lwk_id_t		name,
	unsigned		bits
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
kfs_equal_filename(
	lwk_id_t		name_in_search,
	lwk_id_t		name_in_dir
)
{
	const char * search = (void*) name_in_search;
	const char * dir = (void*) name_in_dir;

	//printk( "%s: Comparing '%s' to '%s'\n", __func__, search, dir );

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
kfs_read(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			len
)
{
	return -1;
}

static ssize_t
kfs_write(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			len
)
{
	return -1;
}


static int
kfs_close(
	struct kfs_file *	file
)
{
	return 0;
}



struct kfs_fops kfs_default_fops =
{
	.lookup		= 0, // use the kfs_lookup
	.read		= kfs_read,
	.write		= kfs_write,
	.close		= kfs_close,
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
	printk( "%s: Creating '%s' (parent %s)\n",
		__func__,
		name,
		parent ? parent->name : "NONE"
	);

	struct kfs_file * file = kmem_alloc( sizeof(*file) );
	if( !file )
		return NULL;

	file->files = htable_create(
		7,
		offsetof( struct kfs_file, name_ptr ),
		offsetof( struct kfs_file, ht_link ),
		kfs_hash_filename,
		kfs_equal_filename
	);

	file->parent	= parent;
	file->fops	= fops;
	file->priv	= priv;
	file->priv_len	= priv_len;
	file->mode	= mode;
	file->name_ptr	= (lwk_id_t) file->name;
	file->refs	= 0;

	if( name )
	{
		// Copy up to the first / or nul, stopping before we over run
		int offset = 0;
		while( *name && *name != '/' && offset < sizeof(file->name)-1 )
			file->name[offset++] = *name++;
		file->name[offset] = '\0';
	}

	if( parent )
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
			(lwk_id_t) dirname
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




// For testing /sys/dummy
static char dummy_string[] = "Thank you for playing";


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

	printk( "%s: Creating '%s' (%s)\n", __func__, full_filename, filename );

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

	printk( "%s: Openning '%s' flags %x mode %x\n",
		__func__, 
		pathname,
		flags,
		mode
	);

	struct kfs_file * file = kfs_lookup( kfs_root, pathname, 0 );
	if( !file )
		return -ENOENT;

	printk( "%s: Found file %p: '%s'\n",
		__func__,
		file,
		file->name
	);

	// Find the first free fd
	int fd;
	for( fd=0 ; fd<MAX_FILES ; fd++ )
	{
		if( !current->files[fd] )
			break;
	}

	if( fd == MAX_FILES )
		return -EMFILE;

	if( file->fops->open
	&&  file->fops->open( file ) < 0 )
		return -EACCES;

	current->files[fd] = file;
	file->refs++;

	return fd;
}



static ssize_t
console_write(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			len
)
{
	char			kbuf[ 512 ];
	size_t			klen = len;
	if( klen > sizeof(kbuf)-1 )
		klen = sizeof(kbuf)-1;

	if( copy_from_user( kbuf, (void*) buf, klen ) )
		return -EFAULT;

	kbuf[ klen ] = '\0';

	printk( KERN_USERMSG
		"(%s) %s%s",
		current->name,
		kbuf,
		klen != len ? "<TRUNC>" : ""
	);

	return klen;
}


struct kfs_fops kfs_console_fops = {
	.write		= console_write
};






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
		return file->fops->write( file, buf, len );

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
		return file->fops->read( file, buf, len );

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

	if( file->fops->close )
		return file->fops->close( file );

	printk( KERN_WARNING "%s: fd %d has no close operation\n", __func__, fd );
	return -EBADF;
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

	//kfs_mkdir( "/sys", 0777 );
	//kfs_mkdir( "/sys/dummy", 0777 );

	kfs_create(
		"/sys/kernel/dummy/string",
		&kfs_default_fops,
		0777,
		dummy_string,
		sizeof( dummy_string )
	);

	kfs_create(
		"/dev/console",
		&kfs_console_fops,
		0777,
		0,
		0
	);

	// Test lookup
	struct kfs_file * test = kfs_lookup( kfs_root, "/sys/kernel/dummy/string", 0 );
	printk( "%s: file %p: %s\n", __func__, test, test ? test->name : "!!!" );

	// Assign some system calls
	syscall_register( __NR_open, (syscall_ptr_t) sys_open );
	syscall_register( __NR_close, (syscall_ptr_t) sys_close );
	syscall_register( __NR_write, (syscall_ptr_t) sys_write );
	syscall_register( __NR_read, (syscall_ptr_t) sys_read );
}


driver_init( "late", kfs_init );
