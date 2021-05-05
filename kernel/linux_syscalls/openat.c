#include <lwk/kfs.h>
#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>


/** Openat system call
 *
 * \todo Implement flags and mode tests.
 */
int
sys_openat(int     dirfd,
	   uaddr_t u_pathname,
	   int     flags,
	   mode_t  mode)
{
	int fd;
	char pathname[ MAX_PATHLEN ];
	struct file *file;
	struct inode * root_inode = NULL;

	printk("openat\n");

	if( strncpy_from_user( pathname, (void*) u_pathname, sizeof(pathname) ) < 0 ) {
		printk("bad copy from user\n");

		return -EFAULT;
	}

	//printk("sys_openat(%s): %d %08x %03x\n", pathname, dirfd, flags, mode);

	printk( "name='%s' dirfd=%d flags %x mode %x\n", pathname, dirfd, flags, mode);
	//_KDBG( "name='%s' flags %x mode %x\n", pathname, flags, mode);

	if ((pathname[0] == '/') ||
	    (dirfd       == AT_FDCWD)) {
	    root_inode = kfs_root;
	} else {
	    struct file * root_file = get_current_file(dirfd);
	    
	    if (!root_file) {
		return -EBADF;
	    }

	    root_inode = root_file->inode;
	}


	// FIX ME
__lock(&_lock);
	if ( ! kfs_lookup( root_inode, pathname, 0 ) && ( flags & O_CREAT ) )  {
		extern struct kfs_fops in_mem_fops;
		extern struct inode_operations in_mem_iops;
		if ( kfs_create_at( root_inode, pathname, &in_mem_iops, &in_mem_fops, 0777, 0, 0 )
		     == NULL ) {
			fd = -EFAULT;
			goto out;
       		}
    	}

	if((fd = kfs_open_path_at(root_inode, pathname, flags, mode, &file)) < 0) {
		//printk("sys_openat failed : %s (%d)\n",pathname,fd);
		goto out;
	}
	fd = fdTableGetUnused( current->fdTable );
	fdTableInstallFd( current->fdTable, fd, file );

	// TODO XXX - check to see if the file's path identifies it as a pipe, and set the pipe stuff

        printk("name=`%s` fd=%d \n", pathname, fd );
out:
__unlock(&_lock);
	return fd;
}

