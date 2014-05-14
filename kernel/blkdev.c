/** 
 * Kitten Block Layer
 * (c) 2014 Jack Lange, <jacklange@cs.pitt.edu>
 */


#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/params.h>
#include <lwk/driver.h>
#include <lwk/list.h>
#include <lwk/kfs.h>
#include <lwk/fdTable.h>
#include <lwk/blkdev.h>
#include <lwk/pmem.h>
#include <lwk/aspace.h>
#include <lwk/delay.h>




static char blkdev_str[128];
param_string(block, blkdev_str, sizeof(blkdev_str));

static struct list_head blkdev_list;
static struct inode *   blkdev_root;
static spinlock_t       blkdev_lock;

typedef struct {
	char name[32];

	blkdev_ops_t * ops;
	void * priv_data;

	u64 sector_size;    /* Sector Size of the block device (in bytes)*/
	u64 capacity;       /* Capacity of block device (in bytes) */
	u32 max_dma_descs;  /* Maximum number of DMA descriptors per request */
	u32 request_slots;  /* Maximum number of requests that can be issued simultaneously */

	spinlock_t lock;

	struct inode *  dev_inode;

	struct list_head blkdev_node;

} blkdev_t;



static int 
get_needed_desc_cnt(vaddr_t vaddr, size_t size) 
{
	vaddr_t vaddr_iter = vaddr;
	int cnt            = 0;

	while (vaddr_iter <= (vaddr + size)) {
		aspace_mapping_t mapping;
		memset(&mapping, 0, sizeof(aspace_mapping_t));

		
		if (aspace_lookup_mapping(MY_ID, vaddr_iter, &mapping) != 0) {
			printk(KERN_ERR "Invalid user address in blkdev read request\n");
 			return -1;
		}
		
		vaddr_iter = mapping.end;

		cnt++;
	}
	
	return cnt;
}


static ssize_t
__send_blk_req(struct file * filp, vaddr_t ubuf, size_t size, int is_write) 
{
	loff_t offset      = filp->pos;
	blkdev_t  * blkdev = filp->private_data;
	blk_req_t * blkreq = NULL;
	vaddr_t tmp_vaddr  = (vaddr_t)ubuf;
	size_t tmp_size    = size;
	int status         = 0;
	int ret            = 0;
	int i              = 0;

	// We only support block operations at the sector size granularity
	if (((uintptr_t)ubuf % blkdev->sector_size) || 
	    (offset          % blkdev->sector_size) || 
	    (size            % blkdev->sector_size)) 
	{
		return -EINVAL;
	}
	
		
	blkreq = kmem_alloc(sizeof(blk_req_t));
	blkreq->total_len = size;
	blkreq->offset    = offset;
	blkreq->write     = is_write;

	blkreq->desc_cnt  = get_needed_desc_cnt(tmp_vaddr, size);
	blkreq->dma_descs = kmem_alloc(sizeof(blk_dma_desc_t) * blkreq->desc_cnt);

	for (i = 0; i < blkreq->desc_cnt; i++) {
		aspace_mapping_t mapping;
		u32 desc_len = 0;

		memset(&mapping, 0, sizeof(aspace_mapping_t));

		aspace_lookup_mapping(MY_ID, tmp_vaddr, &mapping);
		
		desc_len = (mapping.end - tmp_vaddr) < tmp_size ? 
			   (mapping.end - tmp_vaddr) : tmp_size;

		blkreq->dma_descs[i].buf_paddr = mapping.paddr + (tmp_vaddr - mapping.start);
		blkreq->dma_descs[i].length    = desc_len;

		tmp_vaddr += desc_len;
		tmp_size  -= desc_len;
	}
	
	waitq_init(&(blkreq->user_waitq));

	blkreq->complete = 0;

	ret = blkdev->ops->handle_blkreq(blkreq, blkdev->priv_data);
	
	/* Failed to issue block request 
	 * This might be due to unavailable request slots
	 */
	if (ret != 0) {
		kmem_free(blkreq->dma_descs);
		kmem_free(blkreq);
		return ret;
	}



	/*	while (blkreq->complete == 0) {
		mdelay(100);
		blkdev->ops->dump_state(blkdev->priv_data);
	}
	*/

	wait_event_interruptible(blkreq->user_waitq,
				 (blkreq->complete != 0));


	/** 
	 * At this point blkreq has returned, and the status field records sucess or failure
	 * Success (status = 0), Failure (status = error code) 
	 */
	status = blkreq->status;

	kmem_free(blkreq->dma_descs);
	kmem_free(blkreq);

	if (status == 0) {
	    filp->pos += size;
	    return size;
	}

	return status;	
}


static ssize_t 
blkdev_write(struct file * filp, const char __user * ubuf, size_t size, loff_t * off)
{
	return __send_blk_req(filp, (vaddr_t)ubuf, size, 1);
}


static ssize_t 
blkdev_read(struct file * filp, char __user * ubuf, size_t size, loff_t * off) 
{
	return __send_blk_req(filp, (vaddr_t)ubuf, size, 0);
}

/**
 * The only difference from the standard lseek is that we require aligned offsets 
 * that match the blkdev's sector size 
 */
static off_t
blkdev_lseek(struct file * filp,
	     off_t         offset,
	     int           whence) 
{
	blkdev_t * blkdev = filp->private_data;
        loff_t new_offset = 0;
        int rv = 0;
	

	if (offset % blkdev->sector_size) {
		return -EINVAL;
	}

        switch(whence) {
        case 0 /* SEEK_SET */:
                new_offset = offset;
                break;
        case 1 /* SEEK_CUR */:
                new_offset = filp->pos + offset;
                break;
        case 2 /* SEEK_END */:
                if (!filp->inode) {
                        printk(KERN_WARNING "lseek(..., SEEK_END) invoked on "
                               "a file with no inode.\n");
                        rv = -EINVAL;
                } else {
                        new_offset = filp->inode->size + offset;
                }
                break;
        default:
                rv = -EINVAL;
                break;
        }

        if(new_offset < 0)
                rv = -EINVAL;

        /* TODO: more sanity checks; especially wrt. file size */

        if (rv == 0) {
                rv        = filp->pos;
                filp->pos = new_offset;
        }

        return rv;
}

static unsigned int 
blkdev_poll(struct file              * filp, 
	    struct poll_table_struct * table) 
{
	unsigned int mask = 0;

	return mask;
}

static int
blkdev_open(struct inode * inodep, 
	    struct file  * filp) 
{
	blkdev_t * blkdev   = inodep->priv;
	
	filp->private_data = blkdev;

        return 0;
}

static int 
blkdev_close(struct file * filp) 
{
//	blkdev_t * blkdev = filp->private_data;

        return 0;
}

static long 
blkdev_ioctl(struct file  * filp,
	     unsigned int   ioctl, 
	     unsigned long  arg)
{

    return 0;
}

/** 
 * Called when the driver has completed the block request
 * On success: status = 0
 * On failure: status = error code
 */
int 
blk_req_complete(blk_req_t * blkreq, 
		 int         status) 
{

	blkreq->complete = 1;
	blkreq->status   = status;
    
	waitq_wakeup(&(blkreq->user_waitq));

	return 0;
}


static struct kfs_fops blkdev_fops = {
        .open           = blkdev_open, 
        .write          = blkdev_write,
        .read           = blkdev_read,
	.lseek          = blkdev_lseek,
        .poll           = blkdev_poll, 
        .close          = blkdev_close,
        .unlocked_ioctl = blkdev_ioctl,
};


int 
blkdev_do_request(blkdev_handle_t   blkdev_handle, 
		  blk_req_t       * request)
{
    
        blkdev_t * blkdev = (blkdev_t *)blkdev_handle;
	u32 total_len     = 0;
	int ret           = 0;
	int i             = 0;

	/* Sanity Check Request */
	// We only support block operations at the sector size granularity
	if ((request->total_len % blkdev->sector_size) || 
	    (request->offset    % blkdev->sector_size)) 
	{
		printk(KERN_ERR "BLKDEV: Request Alignment Error\n");
		printk(KERN_ERR "BLKDEV:\ttotal_len=%llu, offset=%llu, sector_size=%llu\n", 
		       request->total_len, request->offset, blkdev->sector_size);
		return -EINVAL;
	}


	/* Check that the iov list matches total requested length */
	for (i = 0; i < request->desc_cnt; i++) {
		total_len += request->dma_descs[i].length;
	}

	if (total_len != request->total_len) {
		printk(KERN_ERR "BLKDEV: Invalid DMA Descriptor List.\n");
		printk(KERN_ERR "BLKDEV:\tDMA descriptor Length=%u, Request Length=%llu\n", 
		       total_len, request->total_len);
		return -EINVAL;
	}

	/* Check that request does not overrun device */
	if ((request->offset + request->total_len) > blkdev->capacity) {
		printk(KERN_ERR "BLKDEV: Device Overrun Detected\n");
		printk(KERN_ERR "BLKDEV:\tDev Capacity=%llu, Request End=%llu\n", 
		       blkdev->capacity, (request->offset + request->total_len));
		       
		return -EINVAL;
	}

	/* Check that the iov list is not larger than max supported size */
	if (request->desc_cnt > blkdev->max_dma_descs) {
		printk(KERN_ERR "BLKDEV: DMA Descriptor List is too long\n");
		printk(KERN_ERR "BLKDEV:\tMAX descriptors=%u, Descriptor Count=%u\n", 
		       blkdev->max_dma_descs, request->desc_cnt);
		return -EINVAL;
	}
	

	waitq_init(&(request->user_waitq));

	request->complete = 0;

	ret = blkdev->ops->handle_blkreq(request, blkdev->priv_data);
	
	/* Failed to issue block request 
	 * This might be due to unavailable request slots
	 */
	if (ret != 0) {
		printk(KERN_ERR "BLKDEV: Error handling block request in block driver\n");
		return ret;
	}

	/*
	while (request->complete == 0) {
		mdelay(100);
		blkdev->ops->dump_state(blkdev->priv_data);
	}
	*/

	wait_event_interruptible(request->user_waitq,
				 (request->complete != 0));

	return request->status;
}

u64
blkdev_get_capacity(blkdev_handle_t blkdev_handle) 
{
	blkdev_t * blkdev = (blkdev_t *)blkdev_handle;
	
	return blkdev->capacity;
}

blkdev_handle_t 
get_blkdev(char * name)
{
	blkdev_t      * blkdev      = NULL;
	blkdev_t      * blkdev_iter = NULL;
	unsigned long   irqstate;


	spin_lock_irqsave(&(blkdev_lock), irqstate);
	{
		list_for_each_entry(blkdev_iter, &blkdev_list, blkdev_node) {
			if (strncmp(name, blkdev_iter->name, 32) == 0) {
				blkdev = blkdev_iter;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&(blkdev_lock), irqstate);

	return (blkdev_handle_t)blkdev;
}


int 
blkdev_register(char         * name, 
		blkdev_ops_t * blkdev_ops, 
		u64            sector_size, 
		u64            num_sectors, 
		u32            max_dma_descs,
		u32            request_slots,
		void         * priv_data) 
{
    
	blkdev_t      * blkdev = kmem_alloc(sizeof(blkdev_t));
	unsigned long   irqstate;

	if (blkdev == NULL) {
		printk(KERN_ERR "Failed to allocate blkdev '%s'\n", name);
		return -1;
	}
	

	blkdev->ops       = blkdev_ops;
	blkdev->priv_data = priv_data;
	strncpy(blkdev->name, name, 32);


	blkdev->sector_size   = sector_size;
	blkdev->capacity      = sector_size * num_sectors;
	blkdev->max_dma_descs = max_dma_descs;
	blkdev->request_slots = request_slots;

	spin_lock_init(&(blkdev->lock));

	printk("Registering Block Device (%s) [Sect. Size=%llu, capacity=%lluGB]\n", 
	       blkdev->name, blkdev->sector_size, blkdev->capacity / (1024 * 1024 * 1024));

	spin_lock_irqsave(&(blkdev_lock), irqstate);
	{
	    list_add((&blkdev->blkdev_node), &(blkdev_list));
	}
	spin_unlock_irqrestore(&(blkdev_lock), irqstate);

	/* Create KFS file */
	blkdev->dev_inode = kfs_mkdirent(blkdev_root,
					 blkdev->name, 
					 NULL,
					 &blkdev_fops, 
					 0777, 
					 blkdev, 0);


	return 0;
}




/**
 * Initializes the block storage subsystem; called once at boot
 */
void 
blkdev_init(void)
{
	printk( KERN_INFO "%s: Registering Block devices: '%s'\n", 
		__func__, blkdev_str);


	INIT_LIST_HEAD(&(blkdev_list));
	spin_lock_init(&(blkdev_lock));
       
	blkdev_root = kfs_mkdir("/dev/block", 0777);

	driver_init_list("block", blkdev_str);

	return;
}

