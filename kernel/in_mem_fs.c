#include <lwk/kfs.h>
#include <lwk/list.h>
#include <lwk/pmem.h>
#include <arch/uaccess.h>

struct in_mem_data_block {
	uintptr_t blk_paddr;
	struct list_head node;
};

struct in_mem_priv_data {
	struct list_head block_list;
	u32 num_blocks;
	struct mutex fop_mutex; /* We probably want this... */
};

#define dbg(fmt,args...)
//#define dbg _KDBG

#define PRIV_DATA(x) ((struct in_mem_priv_data*) x)
#define DATA_BLK_SIZE (PAGE_SIZE)
#define MAX_FILE_SIZE (64 * 1024 * 1024)   /* 64MB for now... */

static inline struct in_mem_data_block *
get_block_from_offset(
	struct in_mem_priv_data * priv, 
	loff_t                    offset)
{
	struct in_mem_data_block * data_blk = NULL;
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


static int in_mem_open(struct inode * inode, struct file * file)
{
	file->pos = 0;
	file->private_data = inode->i_private;
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

	{
		size_t bytes_to_copy = len;
		size_t bytes_copied  = 0;

		while (bytes_to_copy > 0) {
			struct in_mem_data_block * blk = get_block_from_offset(priv, file->pos);
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

	if ((file->pos + len) > (priv->num_blocks * DATA_BLK_SIZE)) {
		/* Expand memory */
		u64 bytes_to_add = (file->pos + len) - (priv->num_blocks * DATA_BLK_SIZE);
		u64 blks_to_add  = (bytes_to_add + (PAGE_SIZE - 1)) / PAGE_SIZE;
		int i;
		
		for (i = 0; i < blks_to_add; i++) {
			struct in_mem_data_block * new_blk = NULL;
			uintptr_t blk_paddr = 0; 

			blk_paddr = alloc_data_page();
			if (blk_paddr == 0) {
				return -ENOMEM;
			}

			new_blk = kmem_alloc(sizeof(struct in_mem_data_block));
			memset(new_blk, 0, sizeof(struct in_mem_data_block));

			new_blk->blk_paddr = blk_paddr;
			list_add_tail(&(new_blk->node), &(priv->block_list));
			priv->num_blocks++;
		}
	}
	
	{
		size_t bytes_to_copy = len;
		size_t bytes_copied  = 0;

		while (bytes_to_copy > 0) {
			struct in_mem_data_block * blk = get_block_from_offset(priv, file->pos);
			size_t bytes_in_block = DATA_BLK_SIZE - (file->pos % DATA_BLK_SIZE);
			
			if (bytes_in_block > bytes_to_copy) {
				bytes_in_block = bytes_to_copy;
			}

			if ( copy_from_user( __va(blk->blk_paddr) + (file->pos % DATA_BLK_SIZE), 
					     buf + bytes_copied, 
					     bytes_in_block) ) {
				return -EFAULT;
			}
			
			file->pos     += bytes_in_block;
			bytes_copied  += bytes_in_block;
			bytes_to_copy -= bytes_in_block;
		}
	}

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

static int create(struct inode *inode, int mode )
{
	dbg("\n");
	inode->i_private = kmem_alloc( sizeof( struct in_mem_priv_data ) );
	INIT_LIST_HEAD(&(PRIV_DATA(inode->i_private)->block_list));
	inode->size = 0;

	return 0;	
}

static int unlink(struct inode *inode )
{
	struct in_mem_priv_data  * file_state = PRIV_DATA(inode->i_private);
	struct in_mem_data_block * tmp        = NULL;
	struct in_mem_data_block * blk        = NULL;

	dbg("\n");
	list_for_each_entry_safe(blk, tmp, &(file_state->block_list), node) 
	{
		list_del( &(blk->node) );
		file_state->num_blocks--;

		free_data_page( blk->blk_paddr );
		kmem_free( blk );
	}

	kmem_free( inode->i_private );
	return 0;	
}

struct inode_operations in_mem_iops = {
	.create = create,	
	.unlink = unlink,
};

struct kfs_fops in_mem_fops = {
	.open = in_mem_open,
	.read = in_mem_read,
	.lseek = in_mem_lseek,
	.write = in_mem_write,
	.ioctl = in_mem_ioctl,
};
