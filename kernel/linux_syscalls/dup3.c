#include <lwk/kfs.h>

/* From the man page:
 *      dup3() is the same as dup2(), except that:
 *
 *      *  The  caller  can  force  the close-on-exec flag to be set for the new file 
 *         descriptor by specifying O_CLOEXEC in flags.  See the description of the same flag in open(2) for
 *         reasons why this may be useful.
 *
 *      *  If oldfd equals newfd, then dup3() fails with the error EINVAL.
 *
 */

int
sys_dup3(int oldfd,
	 int newfd,
	 int flags)
{

	if (oldfd == newfd) {
		return -EINVAL;
	}

	if (flags != 0) {
		printk("dup3 flags are not currently supported\n");
		return -EINVAL;
	}

	return sys_dup2(oldfd, newfd);
}


