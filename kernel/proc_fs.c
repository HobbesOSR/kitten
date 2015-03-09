#include <lwk/kfs.h>
#include <lwk/stat.h>
#include <lwk/list.h>
#include <lwk/htable.h>
#include <lwk/pmem.h>
#include <arch/uaccess.h>

struct proc_data_block {
	uintptr_t blk_paddr;
	struct list_head node;
};


struct proc_ops {
	int (*get_proc_data)(struct file * file, void * priv_data);
	void * priv_data;
};	

struct proc_inode_data {
	struct proc_ops * ops;
};

struct proc_file_data {
	struct list_head block_list;
	u32    num_blocks;    
	size_t data_size;
};


#define dbg(fmt,args...)
//#define dbg _KDBG

#define PRIV_DATA(x) ((struct in_mem_priv_data*) x)
#define DATA_BLK_SIZE (PAGE_SIZE)
#define MAX_FILE_SIZE (64 * 1024 * 1024)   /* 64MB for now... */

static inline struct proc_data_block *
get_block_from_offset(
	struct proc_file_data * priv, 
	loff_t                  offset)
{
	struct proc_data_block * data_blk = NULL;
	u64 target_block = offset / DATA_BLK_SIZE;
	int i = 0;

	if (target_block > priv->num_blocks) {
		return NULL;
	}


	list_for_each_entry(data_blk, &(priv->block_list), node) {
		if (i == target_block) return data_blk;
		i++;
	}

	return NULL;
}

static uintptr_t 
alloc_data_page( void )
{
	struct pmem_region result;
	int status;

	status = pmem_alloc_umem(PAGE_SIZE, 0, &result);
	if (status) return 0;

	status = pmem_zero(&result);
	if (status) return 0;

	return result.start;
}


static void 
free_data_page(uintptr_t page_paddr)
{
	struct pmem_region query;
	struct pmem_region result;
	int status;

	pmem_region_unset_all(&query);

	query.start            = page_paddr;
	query.end              = page_paddr + PAGE_SIZE;
	query.allocated        = true;
	query.allocated_is_set = true;

	status = pmem_query(&query, &result);
	if (status) {
		panic("Freeing page %p failed! query status=%d\n", 
		      (void *)page_paddr, status);
	}

	result.allocated = false;
	status = pmem_update(&result);
	if (status) {
		panic("Failed to free page %p! (status=%d)",
		      (void *)page_paddr, status);
	}
}


static int proc_open(struct inode * inode, struct file * file)
{
	struct proc_inode_data * inode_data = inode->i_private;
	struct proc_file_data  * file_data  = kmem_alloc(sizeof(struct proc_file_data));

	memset(file_data, 0, sizeof(struct proc_file_data));

	INIT_LIST_HEAD(&(file_data->block_list));
	file_data->num_blocks = 0;
	file_data->data_size  = 0;

	file->pos             = 0;
	file->private_data    = file_data;

	inode_data->ops->get_proc_data(file, inode_data->ops->priv_data);


	return 0;
}

static ssize_t
proc_read(
        struct file *	file,
        char *		buf,
        size_t          len,
        loff_t *        off
)
{
	struct proc_file_data* priv = file->private_data;

	len = file->pos + len > priv->data_size ?
			priv->data_size - file->pos : len;

	{
		size_t bytes_to_copy = len;
		size_t bytes_copied  = 0;

		while (bytes_to_copy > 0) {
			struct proc_data_block * blk = get_block_from_offset(priv, file->pos);
			size_t bytes_in_block = DATA_BLK_SIZE - (file->pos % DATA_BLK_SIZE);
			
			if (bytes_in_block > bytes_to_copy) {
				bytes_in_block = bytes_to_copy;
			}

			if ( copy_to_user(  buf + bytes_copied,
					    __va(blk->blk_paddr) + (file->pos % DATA_BLK_SIZE), 
					     bytes_in_block) ) {
				return -EFAULT;
			}
			
			file->pos     += bytes_in_block;
			bytes_copied  += bytes_in_block;
			bytes_to_copy -= bytes_in_block;
		}
	}

	return len;
}

static ssize_t
proc_write(
        struct file *   file,
        const char *    buf,
        size_t          len,
        loff_t *        off
)
{
	return -EINVAL;
}

static ssize_t
proc_lseek(
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
proc_ioctl(
        struct file *   file,
	int		request,
	uaddr_t		addr	
)
{
	return -EINVAL;
}


static int
proc_release(struct inode * inode, struct file * file)
{
	
	struct proc_file_data  * file_state = file->private_data;
	struct proc_data_block * tmp        = NULL;
	struct proc_data_block * blk        = NULL;

	list_for_each_entry_safe(blk, tmp, &(file_state->block_list), node) 
	{
		list_del( &(blk->node) );
		file_state->num_blocks--;

		free_data_page( blk->blk_paddr );
		kmem_free( blk );
	}
	
	kmem_free(file_state);

	return 0;
}

static int create(struct inode *inode, int mode )
{
	struct proc_inode_data * inode_data = kmem_alloc( sizeof( struct proc_inode_data ) );

	dbg("\n");

	inode_data->ops  = inode->priv;
	inode->i_private = inode_data;
	inode->size      = 0;

	return 0;	
}

static int unlink(struct inode *inode )
{
	struct proc_inode_data * inode_data = inode->i_private;

	dbg("\n");
	
	kmem_free( inode_data->ops );
	kmem_free( inode_data );
	return 0;	
}

struct inode_operations proc_iops = {
	.create = create,	
	.unlink = unlink,
};

struct kfs_fops proc_fops = {
	.open    = proc_open,
	.read    = proc_read,
	.lseek   = proc_lseek,
	.write   = proc_write,
	.ioctl   = proc_ioctl,
	.release = proc_release
};



int 
create_proc_file(char * path, 
		 int (*get_proc_data)(struct file * file, void * priv_data),
		 void * priv_data)
{
        struct proc_ops        * ops        = kmem_alloc(sizeof(struct proc_ops));

	memset(ops, 0, sizeof(struct proc_ops));

	ops->get_proc_data = get_proc_data;
	ops->priv_data     = priv_data;

	if (kfs_create(path, &proc_iops, &proc_fops, 0444, ops, sizeof(struct proc_ops)) == NULL) {
		return -1;
	}

	return 0;
}

int 
remove_proc_file(char * path)
{
	struct inode * inode = NULL;
	int ret = 0;
__lock(&_lock);
	inode = kfs_lookup(NULL, path, 0);
	if (!inode) {
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
	kfs_destroy(inode);
out:
__unlock(&_lock);
	return ret;
}

int
proc_mkdir(char * dir_name) 
{
__lock(&_lock);
	struct inode* i = kfs_mkdir( dir_name, 0666 );
__unlock(&_lock);
        if ( ! i ) return ENOENT;
	return 0;
}


int 
proc_rmdir(char * dir_name)
{
	int ret = 0;
__lock(&_lock);
        struct inode * inode = kfs_lookup(kfs_root, dir_name, 0);
	if( !inode ) {
		ret = ENOENT;
		goto out;
	}
	
	if(! S_ISDIR(inode->mode)) {
		ret = -ENOTDIR;
		goto out;
	}
	
	if ( ! htable_empty( inode->files ) ) {
		ret = -ENOTEMPTY;
		goto out;
	}
	
	htable_del( inode->parent->files, inode );
	
	kfs_destroy( inode );
 out:
__unlock(&_lock);
        return ret;
}



int
proc_sprintf(struct file * file, const char * fmt, ...) 
{
	struct proc_file_data * priv = file->private_data;
	unsigned char * buf = NULL;
	size_t          len = 0;
	int             ret = 0;

	{
		va_list ap;
		
		va_start(ap, fmt);
		buf = kvasprintf(0, fmt, ap);
		va_end(ap);
	}

	len = strlen(buf);

	if ( priv->data_size + len > MAX_FILE_SIZE ) {
		ret = -EFBIG;
		goto out;
	}

	if ((priv->data_size + len) > (priv->num_blocks * DATA_BLK_SIZE)) {
		/* Expand memory */
		u64 bytes_to_add = (priv->data_size + len) - (priv->num_blocks * DATA_BLK_SIZE);
		u64 blks_to_add  = (bytes_to_add + (PAGE_SIZE - 1)) / PAGE_SIZE;
		int i;
		
		for (i = 0; i < blks_to_add; i++) {
			struct proc_data_block * new_blk = NULL;
			uintptr_t blk_paddr = 0; 

			blk_paddr = alloc_data_page();
			if (blk_paddr == 0) {
				ret = -ENOMEM;
				goto out;
			}

			new_blk = kmem_alloc(sizeof(struct proc_data_block));
			memset(new_blk, 0, sizeof(struct proc_data_block));

			new_blk->blk_paddr = blk_paddr;
			list_add_tail(&(new_blk->node), &(priv->block_list));
			priv->num_blocks++;
		}
	}
	
	{
		size_t bytes_to_copy = len;
		size_t bytes_copied  = 0;

		while (bytes_to_copy > 0) {
			struct proc_data_block * blk = get_block_from_offset(priv, priv->data_size);
			size_t bytes_in_block = DATA_BLK_SIZE - (priv->data_size % DATA_BLK_SIZE);
			
			if (bytes_in_block > bytes_to_copy) {
				bytes_in_block = bytes_to_copy;
			}

			memcpy( __va(blk->blk_paddr) + (priv->data_size % DATA_BLK_SIZE), 
				buf + bytes_copied, 
				bytes_in_block);
		
			priv->data_size += bytes_in_block;
			bytes_copied    += bytes_in_block;
			bytes_to_copy   -= bytes_in_block;
		}
	}

	ret = len;


 out:
	kmem_free(buf);
	return ret;
}
