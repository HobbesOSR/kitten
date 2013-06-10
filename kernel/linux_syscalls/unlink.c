#include <lwk/kfs.h>
#include <lwk/htable.h>
#include <lwk/stat.h>
#include <arch/uaccess.h>

int
sys_unlink(uaddr_t u_pathname)
{
        // Note that this function is hack
	// jaKa: noted ;)
	char pathname[ MAX_PATHLEN ];
	if( strncpy_from_user( pathname, (void*) u_pathname,
                               sizeof(pathname) ) < 0 )
	return -EFAULT;

	dbg( "name='%s'\n", pathname);

	int ret = 0;

__lock(&_lock);
	struct inode * inode = kfs_lookup( kfs_root, pathname, 0 );
	if( !inode ) {
		ret = ENOENT;
		goto out;
	}

	if(S_ISDIR(inode->mode)) {
		ret = -EISDIR;
		goto out;
	}

	if ( inode->i_op && inode->i_op->unlink )  {
		if ( inode->i_op->unlink( inode ) ) {
			printk("%s:%d() ????\n",__FUNCTION__,__LINE__);
		}
	}

	htable_del( inode->parent->files, inode );

	kfs_destroy( inode );

out:
__unlock(&_lock);
	return ret;
}


