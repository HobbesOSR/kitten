#include <lwk/kfs.h>

ssize_t
sys_close(int fd)
{
	int ret = 0;
	struct file * const file = get_current_file( fd );

	if( !file ) {
		ret = -EBADF;
		goto out;
	}

	// if we're closing an end of a pipe...
	if (file->pipe != NULL) {
		file->pipe->ref_count--;
		if (file->pipe->ref_count <= 0) {
			kmem_free(file->pipe->buffer);
			kmem_free(file->pipe);
		} else if (file->pipe_end_type == PIPE_END_WRITE) {
			// if the write end was closed, that sends an EOF into the pipe
			file->pipe->eof = 1;
		}
	}

	char __attribute__((unused)) buff[MAX_PATHLEN];
//        dbg("name=`%s` fd=%d\n", get_full_path(file->inode,buff), fd );

	if( file->f_op && file->f_op->close )
		ret = file->f_op->close( file );

	if(ret == 0) {
		kfs_close(file);
		fdTableInstallFd(current->fdTable, fd, 0); // remove the fd from the table
	}

out:
	return ret;
}
