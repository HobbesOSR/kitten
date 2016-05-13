/** \file
 * Kitten kernel VFS.
 *
 */
#include <lwk/driver.h>
#include <lwk/print.h>
#include <lwk/htable.h>
#include <lwk/kfs.h>
#include <lwk/stat.h>
#include <lwk/aspace.h>

struct inode *kfs_root;
spinlock_t _lock;

/* in-kernel dirent struct: should match the libc dirent semantics */
struct dirent {
	long d_ino;                 /* inode number */
	off_t d_off;                /* offset to next dirent */
	unsigned short d_reclen;    /* length of this dirent */
	char d_name [1];            /* filename (var-length, null-terminated) */
	/* d_type follows d_name */
};

struct dirent64 {
	long d_ino;                 /* inode number */
	off_t d_off;                /* offset to next dirent */
	unsigned short d_reclen;    /* length of this dirent */
	char d_type;                /* oops, a subtle difference */
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

int
kfs_stat(struct inode *inode, uaddr_t buf)
{
	struct stat rv;

	dbg("\n");

	memset(&rv, 0x00, sizeof(struct stat));
	rv.st_mode = inode->mode;
	rv.st_rdev = inode->i_rdev;
	rv.st_ino  = (ino_t)inode;
	rv.st_size = inode->size;

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
__lock(&_lock);
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
__unlock(&_lock);
	return offset;
}

static ssize_t
kfs_default_write(struct file *	file,
		  const char *	buf,
		  size_t        len,
		  loff_t *      off)
{
	dbg("\n");
	return -1;
}


static int
kfs_default_close(struct file *	file)
{
	dbg("\n");
	return 0;
}


int
kfs_fill_dirent(uaddr_t buf, unsigned int count,
		struct inode *inode, const char *name, int namelen,
		loff_t offset, unsigned int type)
{
	struct dirent __user *de;
	unsigned short len = ALIGN(offsetof(struct dirent, d_name)
				   + namelen + 2, sizeof(long));
	dbg("name=`%s`\n",name);

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
kfs_fill_dirent64(uaddr_t buf, unsigned int count,
		  struct inode *inode, const char *name, int namelen,
		  loff_t offset, unsigned int type)
{
	struct dirent64 __user *de;
	unsigned short len = ALIGN(offsetof(struct dirent, d_name)
				   + namelen + 2, sizeof(long));
	dbg("name=`%s`\n",name);

	if(count < len)
		return -EINVAL;

	de = (struct dirent64 *)buf;
	if(__put_user(offset, &de->d_off))
		return -EFAULT;
	if(__put_user((long)inode, &de->d_ino)) /* fake inode # */
		return -EFAULT;
	if(__put_user(len, &de->d_reclen))
		return -EFAULT;
	if(__put_user(type, &de->d_type))
		return -EFAULT;
	if(copy_to_user(de->d_name, name, namelen))
		return -EFAULT;
	if(__put_user(0, de->d_name + namelen))
		return -EFAULT;

	return len;
}

int
kfs_readdir(struct file * filp, uaddr_t buf, unsigned int count,
	    dirent_filler f)
{
	struct inode *child;
__lock(&_lock);
	struct htable_iter iter = htable_iter( filp->inode->files );
	int res = 0, rv = 0, i;

	/* for directories, we just increase pos by 1 for every
	   child we successfully fill into the dirent buffer;
	   linux uses 0 and 1 for . and .., then inodes for the
	   rest */
	if(filp->pos == 0) {
		rv = f(buf, count, filp->inode,
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
		rv = f(buf, count, filp->inode,
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
		rv = f(buf, count, child,
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
__unlock(&_lock);
	if(rv < 0 && (rv != -EINVAL || res == 0))
		return rv;
	return res;
}

off_t
kfs_lseek( struct file *filp, off_t offset, int whence )
{
	loff_t new_offset = 0;
	int rv = 0;

	switch(whence) {
	case 0 /* SEEK_SET */:
		new_offset = offset;
		break;
	case 1 /* SEEK_CUR */:
		new_offset = filp->pos + offset;
		break;
	case 2 /* SEEK_END */:
		if(!filp->inode) {
			printk(KERN_WARNING "lseek(..., SEEK_END) invoked on "
			       "a file with no inode.\n");
			rv = -EINVAL;
		}
		else {
			new_offset = filp->inode->size + offset;
		}
		break;
	default:
		rv = -EINVAL;
		break;
	}

	if(new_offset < 0)
		rv = -EINVAL;

	/* TODO: more sanity checks; especially wrt. file size */

	if(rv == 0) {
		rv = filp->pos;
		filp->pos = new_offset;
	}

	return rv;
}

struct kfs_fops kfs_default_fops =
{
	.read		= kfs_default_read,
	.write		= kfs_default_write,
	.close		= kfs_default_close,
	.readdir	= kfs_readdir,
	.lseek          = kfs_lseek,
};

static struct inode *
kfs_create_inode(void)
{
	struct inode *inode = kmem_alloc(sizeof(struct inode));
	memset(inode, 0x00, sizeof(struct inode));
	atomic_set(&inode->i_count, 0);

	return inode;
}

struct inode *
kfs_mkdirent(struct inode *          parent,
	     const char *            name,
	     const struct inode_operations * i_ops,
	     const struct kfs_fops * f_ops,
	     unsigned                mode,
	     void *                  priv,
	     size_t                  priv_len)
{
	char buf[MAX_PATHLEN];
	if ( parent ) {
		get_full_path(parent, buf);
	}
	dbg( "name=`%s/%s`\n", buf, name );

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
	inode->i_fop	= f_ops;
	inode->i_op	= i_ops;
	inode->priv	= priv;
	inode->priv_len	= priv_len;
	inode->mode	= mode;

	if ( i_ops && i_ops->create )  {
		if ( i_ops->create( inode, mode ) ) {
			printk("%s:%d() ????\n",__FUNCTION__,__LINE__);
			return NULL;
		}
	}
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
	//_KDBG("%p %d\n",inode,atomic_read(&inode->i_count));
	if ( atomic_read(&inode->i_count ) == 0 ) {
		if ( inode->files )
			htable_destroy( inode->files );
		kmem_free( inode );
	}
}

struct inode *
kfs_lookup(struct inode *       root,
	   const char *		dirname,
	   unsigned		create_mode)
{
	dbg("name=`%s`\n", dirname );

	// Special case -- use the root if root is null.
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
		if( root->i_op && root->i_op->lookup )
			return (struct inode *)
				root->i_op->lookup( root,
						    (struct dentry*) dirname,
						    (struct nameidata*) (unsigned long) create_mode );

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
						NULL,
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
kfs_create_at(struct inode                  * root_inode,
	      const char                    * full_filename,
	      const struct inode_operations * iops,
	      const struct kfs_fops         * fops,
	      unsigned		              mode,
	      void                          * priv,
	      size_t		              priv_len)
{
	struct inode *dir;
	const char * filename = strrchr( full_filename, '/' );

	BUG_ON(!root_inode);

	if ( full_filename[0] == '/' ) {
		root_inode = kfs_root;
	}

	dbg("name=`%s`\n", full_filename);

	if( !filename )
	{
		//printk( "%s: Non-absolute path name! '%s'\n",
		//	__func__, full_filename );
		//return NULL;
		root_inode = kfs_root;
	}else{
		filename++;
	}

	dir = kfs_lookup( root_inode, full_filename, 0777 );

	if( !dir )
		return NULL;

	// Create the file entry in the directory
	return kfs_mkdirent(dir, filename, iops, fops, mode, priv, priv_len);
}


struct inode *
kfs_create(const char *            full_filename,
	   const struct inode_operations * iops,
	   const struct kfs_fops * fops,
	   unsigned		   mode,
	   void *		   priv,
	   size_t		   priv_len)
{
	return kfs_create_at(kfs_root, full_filename, iops, fops, mode, priv, priv_len);
}

struct inode *
kfs_mkdir(char *   name,
	  unsigned mode)
{
	dbg("name=`%s`\n",name);
	return kfs_create(name,
			  NULL,
			  &kfs_default_fops,
			  (mode & ~S_IFMT) | S_IFDIR,
			  0,
			  0);
}

static char*
get_full_path2(struct inode *inode, char *buf, int flag )
{
	if( inode->parent )
		get_full_path2(inode->parent, buf, 1);

	strcat(buf, inode->name);

	if ( flag )
		strcat(buf, "/");

	return buf;
}

char *
get_full_path(struct inode *inode, char *buf)
{
	buf[0] = 0;
	if ( ! inode ) return "no inode";
	return  get_full_path2( inode, buf, 0 );
}

struct inode *
kfs_link(struct inode *target, struct inode *parent, const char *name)
{
	struct inode *link;

	dbg("name=`%s`\n",name);

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
		link->mode |= 0111;

	link->link_target = target;

	link->parent = parent;
	htable_add(parent->files, link);

	return link;
}

static struct file *
kfs_open(struct inode *inode, int flags, mode_t mode)
{
	struct file *file = kmem_alloc(sizeof(struct file));

	if(NULL == file)
		return NULL;

	memset(file, 0x00, sizeof(struct file));

	file->f_flags = flags;
	file->f_mode = mode;
	file->f_op = inode->i_fop;
	file->inode = inode;
	atomic_set(&file->f_count,1);
	atomic_inc(&inode->i_count);

	return file;
}

void
kfs_close(struct file *file)
{
	// if this file was allocated with alloc_file it will not have
	// an inode
	if ( file->inode ) {
		char __attribute__((unused)) buff[MAX_PATHLEN];
        	dbg("name=`%s`\n", get_full_path( file->inode, buff) );
		atomic_dec(&file->inode->i_count);
	}
	kmem_free(file);
}


int
kfs_open_path_at(struct inode * root_inode,
		 const char   * pathname, 
		 int            flags, 
		 mode_t         mode, 
		 struct file **rv)
{
	struct inode * inode = NULL;
	
	BUG_ON(!root_inode);

	/* If pathname is absolute, then use kfs root inode */
	if (pathname[0] == '/') {
		root_inode = kfs_root;
	}
	
	inode = kfs_lookup( root_inode, pathname, 0 );
	if( !inode )
		return -ENOENT;

	dbg("name=`%s`\n",pathname);
	struct file *file = kfs_open(inode, flags, mode);
	if(NULL == file)
		return -ENOMEM;

	if( file->f_op->open && file->f_op->open( file->inode , file ) < 0 ) {
		kfs_close(file);
		return -EACCES;
	}

	*rv = file;

	return 0;
}

int
kfs_open_path(const char *pathname, int flags, mode_t mode, struct file **rv)
{
	return kfs_open_path_at(kfs_root, pathname, flags, mode, rv);
}


int
kfs_open_anon(const struct kfs_fops * fops, void * priv_data)
{
    struct file * file = kmem_alloc(sizeof(struct file));
    int fd = 0;

    if (NULL == file)
	return -ENOMEM;

    memset(file, 0x00, sizeof(struct file));

    file->f_op = fops;
    file->private_data = priv_data;
    atomic_set(&file->f_count, 1);

    __lock(&_lock);
    {
	fd = fdTableGetUnused( current->fdTable );
	fdTableInstallFd( current->fdTable, fd, file );
    }
    __unlock(&_lock);

    return fd;
}


void kfs_init_stdio(struct task_struct *task)
{
	struct fdTable *tbl = task->fdTable;
	/* TODO: should really do a kfs_open() once for each of the
	   std fds ... and use appropriate flags and mode for each */

	printk("kfs_init_stdio(): task_id=%d, user_id=%d\n", task->id, task->uid);

	dbg("\n");
	struct file * console;
        if(kfs_open_path("/dev/console", 0, 0, &console ))
                panic( "Unable to open /dev/console?" );
        tbl->files[ 0 ] = console;
        if(kfs_open_path("/dev/console", 0, 0, &console ))
                panic( "Unable to open /dev/console?" );
        tbl->files[ 1 ] = console;
        if(kfs_open_path("/dev/console", 0, 0, &console ))
                panic( "Unable to open /dev/console?" );
        tbl->files[ 2 ] = console;

	/* the process's pipes are stored here */
	// TODO - the permissions are probably wrong
	struct inode *inode;
	char buffer[64];
	sprintf(buffer, "/proc/%d", task->id);
	inode = kfs_mkdir("/proc", 0666);
	if (!inode)
		panic("Failed to 'mkdir /proc'.");
	inode = kfs_mkdir(buffer, 0666);
	if (!inode) {
		sprintf(buffer, "Failed to 'mkdir /proc/%d'.", task->id);
		panic(buffer);
	}
	sprintf(buffer, "/proc/%d/fd", task->id);
	inode = kfs_mkdir(buffer, 0666);
	if (!inode) {
		sprintf(buffer, "Failed to 'mkdir /proc/%d/fd'.", task->id);
		panic(buffer);
	}
}

extern struct inode * sysfs_root;

void
kfs_init( void )
{
	spin_lock_init(&_lock);
	kfs_root = kfs_mkdirent(NULL,
				"",
				NULL,
				&kfs_default_fops,
				0777 | S_IFDIR,
				0,
				0);

	if (!kfs_mkdir("/tmp", 0777))
		panic("Failed to 'mkdir /tmp'.");

	sysfs_root = kfs_mkdir("/sys", 0777);
	if (!sysfs_root) 
		panic("Failed to 'mkdir /sys'.");

	// Bring up any kfs drivers that we have linked in
	driver_init_by_name( "kfs", "*" );
}
