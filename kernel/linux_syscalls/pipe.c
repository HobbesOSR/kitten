#include <lwk/kfs.h>
#include <arch-generic/fcntl.h>

static struct file *
pipe_open(int     flags,
          mode_t  mode,
          int *   result_fd)
{
	int fd, fd_temp;
	struct file *file;
	char pathname[128];
	int pid;

	// FIX ME
__lock(&_lock);
	pid = current->id;
	fd_temp = fdTableGetUnused( current->fdTable );
	sprintf(pathname, "/proc/%d/fd/%d", pid, fd_temp);

	if (!kfs_lookup(kfs_root, pathname, 0) && (flags & O_CREAT)) {
		extern struct kfs_fops in_mem_fops;
		extern struct inode_operations in_mem_iops;
		if (kfs_create( pathname, &in_mem_iops, &in_mem_fops, 0777, 0, 0) == NULL) {
			fd = -EFAULT;
			file = NULL;
			goto out;
       		}
    	}

	if ((fd = kfs_open_path(pathname, flags, mode, &file)) < 0) {
		//printk("sys_open failed : %s (%d)\n",pathname,fd);
		file = NULL;
		goto out;
	}
	fd = fd_temp;
	fdTableInstallFd(current->fdTable, fd, file);

        dbg("name=`%s` fd=%d \n",pathname,fd );
out:
__unlock(&_lock);
	*result_fd = fd;
	return file;
}

int
sys_pipe(int fd[2])
{
	int ret = -EFAULT;
	int fd_read, fd_write;

	// TODO - permissions/mode?
	struct file * f1 = pipe_open(O_RDONLY | O_CREAT, 0666, &fd_read);
	struct file * f2 = pipe_open(O_WRONLY | O_CREAT, 0666, &fd_write);
	struct pipe * p;

 	if (f1 != NULL && f2 != NULL) {
		p = kmem_alloc(sizeof(struct pipe));

		if (p == NULL) {
			return ret;
		}

		memset(p, 0x00, sizeof(struct pipe));
		p->buffer = kmem_alloc(sizeof(char)*(PIPE_BUFFER_MAX+1));
		p->amount = 0;
		waitq_init(&p->buffer_wait);
		spin_lock_init(&p->buffer_lock);
		p->ref_count = 2;

		fd[0] = fd_read;
		f1->pipe_end_type = PIPE_END_READ;
		f1->pipe = p;

		fd[1] = fd_write;
		f2->pipe_end_type = PIPE_END_WRITE;
		f2->pipe = p;

		printk("sys_pipe(): created pipe (%d,%p) <- (%d,%p)\n", fd_read, f1, fd_write, f2);

		ret = 0;
	}

	return ret;
}

