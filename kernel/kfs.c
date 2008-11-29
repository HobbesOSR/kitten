/** \file
 * Kitten kernel VFS.
 *
 */
#include <lwk/driver.h>
#include <lwk/print.h>
#include <lwk/htable.h>
#include <lwk/list.h>
#include <lwk/string.h>


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


struct kfs_fops
{
	struct kfs_file * (*lookup)(
		struct kfs_file *	root,
		const char *		name,
		unsigned		create_mode
	);

	int (*read)(
		struct kfs_file *,
		uaddr_t,
		size_t
	);

	int (*write)(
		struct kfs_file *,
		uaddr_t,
		size_t
	);
};


struct kfs_file
{
	struct htable *		files;
	struct hlist_node	ht_link;
	lwk_id_t		name_ptr;

	struct kfs_file *	parent;
	char			name[ 128 ];
	const struct kfs_fops *	fops;
	unsigned		mode;
	void *			priv;
	size_t			priv_len;
};

static struct kfs_file * kfs_root;


static int
kfs_read(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			len
)
{
	return -1;
}

static int
kfs_write(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			len
)
{
	return -1;
}


struct kfs_fops kfs_default_fops =
{
	.lookup		= 0, // use the kfs_lookup
	.read		= kfs_read,
	.write		= kfs_write,
};


static struct kfs_file *
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

	// Test lookup
	struct kfs_file * test = kfs_lookup( kfs_root, "/sys/kernel/dummy/string", 0 );
	printk( "%s: file %p: %s\n", __func__, test, test ? test->name : "!!!" );
}


driver_init( "late", kfs_init );
