#include <lwk/kfs.h>
#include <arch/uaccess.h>

struct in_mem_priv_data {
	char* buf;
};

#define PRIV_DATA(x) ((struct in_mem_priv_data*) x)
#define MAX_FILE_SIZE	512

static int in_mem_open(struct inode * inode, struct file * file)
{
	if ( ! inode->priv ) {
		inode->priv = kmem_alloc( sizeof( struct in_mem_priv_data ) );
		PRIV_DATA(inode->priv)->buf = kmem_alloc(MAX_FILE_SIZE);
		inode->size = 0;
	}

	file->pos = 0;
	file->private_data = inode->priv;
	return 0;
}

static ssize_t
in_mem_read(
        struct file *	file,
        char *		buf,
        size_t          len,
        loff_t *        off
)
{
	struct in_mem_priv_data* priv = file->private_data;

	len = file->pos + len > file->inode->size ?
			file->inode->size - file->pos : len;

	if ( copy_to_user( buf, priv->buf + file->pos, len ) ) {
		return -EFAULT;
	}

	file->pos += len;
	return len;
}

static ssize_t
in_mem_write(
        struct file *   file,
        const char *    buf,
        size_t          len,
        loff_t *        off
)
{
	struct in_mem_priv_data* priv = file->private_data;

	if ( file->pos + len > MAX_FILE_SIZE ) 
		return -EFBIG;

	if ( copy_from_user( priv->buf + file->pos, buf, len ) ) {
		return -EFAULT;
	}

	file->pos += len;

	if ( file->pos > file->inode->size ) {
		file->inode->size = file->pos;	
	}
	return len;
}

static ssize_t
in_mem_lseek(
        struct file *   file,
	off_t		offset,
	int		whence
)
{
	switch ( whence ) {
	    case 0: /* SEEK_SET */
		if ( offset > MAX_FILE_SIZE ) 
			return -EINVAL; 
		file->pos = offset;
		break;

	     case 1: /*  SEEK_CUR */
		if ( file->pos + offset > MAX_FILE_SIZE ) 
			return -EINVAL; 
		file->pos += offset;
		break;

	     case 2: /* SEEK_END */
		file->pos = file->inode->size;
		break;

	     default:
		return -EINVAL;
	}
	return file->pos;
}

static int 
in_mem_ioctl(
        struct file *   file,
	int		request,
	uaddr_t		addr	
)
{
	return -EINVAL;
}

struct kfs_fops in_mem_fops = {
	.open = in_mem_open,
	.read = in_mem_read,
	.lseek = in_mem_lseek,
	.write = in_mem_write,
	.ioctl = in_mem_ioctl,
};
