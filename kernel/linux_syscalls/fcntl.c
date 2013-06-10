#include <lwk/kfs.h>
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
