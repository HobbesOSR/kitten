#include <lwk/kfs.h>
#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

/** Open system call
 *
 * \todo Implement flags and mode tests.
 */
int
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

	//printk("sys_open(%s): %08x %03x\n", pathname, flags, mode);

	dbg( "name='%s' flags %x mode %x\n", pathname, flags, mode);
	//_KDBG( "name='%s' flags %x mode %x\n", pathname, flags, mode);

	// FIX ME
__lock(&_lock);
	if ( ! kfs_lookup( kfs_root, pathname, 0 ) && ( flags & O_CREAT ) )  {
		extern struct kfs_fops in_mem_fops;
		extern struct inode_operations in_mem_iops;
		if ( kfs_create( pathname, &in_mem_iops, &in_mem_fops, 0777, 0, 0 )
		     == NULL ) {
			fd = -EFAULT;
			goto out;
       		}
    	}

	if((fd = kfs_open_path(pathname, flags, mode, &file)) < 0) {
		//printk("sys_open failed : %s (%d)\n",pathname,fd);
		goto out;
	}
	fd = fdTableGetUnused( current->fdTable );
	fdTableInstallFd( current->fdTable, fd, file );

	// TODO XXX - check to see if the file's path identifies it as a pipe, and set the pipe stuff

        dbg("name=`%s` fd=%d \n",pathname,fd );
out:
__unlock(&_lock);
	return fd;
}

