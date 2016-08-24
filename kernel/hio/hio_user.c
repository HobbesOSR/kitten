#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>
#include <lwk/waitq.h>
#include <lwk/spinlock.h>
#include <lwk/poll.h>

#include <arch/vsyscall.h>
#include <arch/atomic.h>


static waitq_t user_waitq;

static int
hio_open_fop(struct inode * inodep,
    	     struct file  * filp)
{
	filp->private_data = inodep->priv;
	return 0;
}

static int
hio_close_fop(struct file * filp)
{
	return 0;
}

static int
hio_release_fop(struct inode * inodep,
		struct file  * filp)
{
	return 0;
}

static ssize_t
hio_read_fop(struct file * filp,
	     char __user * buffer,
	     size_t        length,
	     loff_t      * offset)
{
	hio_syscall_t * k_syscall;
	int status;

	if (length < sizeof(hio_syscall_t))
		return -EINVAL;

retry:
	status = wait_event_interruptible(
		user_waitq,
		(hio_get_num_pending_syscalls() > 0)
	);
	if (status)
		return status;

	status = hio_get_pending_syscall(&k_syscall);
	if (status != 0) {
		if (status == -ENOENT)
			goto retry;

		return status;
	}

	if (copy_to_user(buffer, k_syscall, sizeof(hio_syscall_t))) {
		/* Cancel it, then re-issue */
		hio_cancel_syscall(k_syscall);
		hio_issue_syscall(k_syscall);
		return -EFAULT;
	}

	return sizeof(hio_syscall_t);
}

static ssize_t
hio_write_fop(struct file	* filp,
	      const char __user * buffer,
	      size_t		  length,
	      loff_t            * offset)
{
	hio_syscall_t syscall;

	if (length < sizeof(hio_syscall_t))
		return -EINVAL;

	if (copy_from_user(&syscall, buffer, sizeof(hio_syscall_t)))
		return -EFAULT;

	hio_return_syscall(&syscall);
	return sizeof(hio_syscall_t);
}


static unsigned int
hio_poll_fop(struct file	      * filp,
	     struct poll_table_struct * poll)
{
	unsigned int mask = POLLOUT | POLLWRNORM;

	poll_wait(filp, &user_waitq, poll);

	if (hio_get_num_pending_syscalls() > 0)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static struct kfs_fops
hio_fops = {
	.open    = hio_open_fop,
	.close   = hio_close_fop,
	.release = hio_release_fop,
	.read    = hio_read_fop,
	.write   = hio_write_fop,
	.poll    = hio_poll_fop
};



static int
init(void)
{
	waitq_init(&user_waitq);

	return (kfs_create("/dev/hio",
		NULL,
		&hio_fops,
		0777,
		NULL, 0) == NULL) ? -1 : 0;
}

static int
notify_new_syscall(void)
{
	/* Wake up poller */
	mb();
	waitq_wakeup(&user_waitq);
	
	return 0;
}


struct hio_implementation 
hio_impl = {
	.init	            = init,
	.notify_new_syscall = notify_new_syscall,
};
