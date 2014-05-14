/** 
 * Kitten Block Layer
 * (c) 2014 Jack Lange, <jacklange@cs.pitt.edu>
 */


#ifndef _LWK_BLKDEV_H
#define _LWK_BLKDEV_H

#include <lwk/waitq.h>


typedef struct {
	uintptr_t buf_paddr;    /* Physical Address of DMA buffer */  
	u64       length;       /* Length of DMA buffer */
} blk_dma_desc_t;

typedef void * blkdev_handle_t;

typedef struct {
	struct __attribute__((packed)) {
		u32 async        : 1; 
		u32 write        : 1;
		u32 complete     : 1;
		u32 rsvd         : 29;
	};


	u64 total_len;        /* Total Length of the block operation */
	u64 offset;           /* Block offset of operation */

	u32 desc_cnt;
	blk_dma_desc_t * dma_descs;

	waitq_t user_waitq;
	int status;
} blk_req_t;


typedef struct {
	int (*handle_blkreq)(blk_req_t * blk_req, void * priv_data);
	int (*dump_state)(void * priv_data);
} blkdev_ops_t;


int blk_req_complete(blk_req_t * blk_req, int status);


blkdev_handle_t get_blkdev(char * name);

u64 blkdev_get_capacity(blkdev_handle_t blkdev_handle);
int blkdev_do_request(blkdev_handle_t blkdev_handle, blk_req_t * request);


int 
blkdev_register(char * name, 
		blkdev_ops_t * blkdev_ops, 
		u64 sector_size, u64 num_sectors, 
		u32 max_dma_descs, u32 request_slots,
		void * priv_data);


/**
 * Initializes the block storage subsystem; called once at boot.
 *
 * This will make the calls to initialize the block drivers 
 * specified on the kernel command line, and register each 
 * block device with the a KFS file
*/
extern void blkdev_init(void);

#endif
