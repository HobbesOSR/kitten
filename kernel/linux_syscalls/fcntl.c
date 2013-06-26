#include <lwk/kfs.h>
#include <lwip/sockets.h>


#ifdef CONFIG_LWIP_SOCKET

/** Return a LWIP connection number from a user fd */
static inline int 
lwip_connection(
    const struct file * file
)
{
    return (int)(uintptr_t) file->private_data;
}


static inline int 
lwip_from_fd(
    int         fd
)
{
    struct file *       file = get_current_file( fd );
    if( !file )
        return -1; 
    //if( file->fops != &kfs_socket_fops )
        //return -1;
    return lwip_connection( file );
}


int
sys_fcntl(int fd, int cmd, long arg)
{
    int ret = -1;

	switch(cmd) {
        case F_GETFL:
            ret = lwip_fcntl(lwip_from_fd(fd), cmd, arg);
            break;
        case F_SETFL:
            /* BJK: Currently, lwip only supports O_NONBLOCK if it is
             * accompanied by no other flags.
            */
            if (arg & O_NONBLOCK) {
                arg &= O_NONBLOCK;
            }
            ret = lwip_fcntl(lwip_from_fd(fd), cmd, arg);
            break;
        default:
            ret = -EINVAL;
            break;
	}

	return ret;
}
#else
#include <arch-generic/fcntl.h>

int
sys_fcntl(int fd, int cmd, long arg)
{
	int ret = -EBADF;
	struct file * const file = get_current_file( fd );

	dbg( "%d %x %lx\n", fd, cmd, arg );

	if(NULL == file)
		goto out;

	switch(cmd) {
	case F_GETFL:
		ret = file->f_flags;
		break;
	case F_SETFL:
		/* TODO: implement; currently we need at least O_NONBLOCK for
		   IB verbs interface */
		file->f_flags = arg;
		ret = 0;
		break;
	case F_GETFD:
	case F_SETFD:
		/* TODO: implement; we do need a place for per-fd flags first,
		   though */
		ret = 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}

out:
	return ret;
	/* not reached */
}
#endif
