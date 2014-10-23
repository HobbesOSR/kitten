#ifndef __PISCES_DATA_H__
#define __PISCES_DATA_H__

#include <lwk/types.h>

struct pisces_xbuf;

struct pisces_xbuf_desc {
    struct pisces_xbuf * xbuf;

    spinlock_t xbuf_lock;
    waitq_t    xbuf_waitq;
    void     * private_data;

    void (*recv_handler)(u8 * data, u32 data_len, void * priv_data);

};


struct pisces_xbuf_desc * 
pisces_xbuf_server_init(uintptr_t   xbuf_va, 
			u32         xbuf_total_bytes,  
			void      (*recv_handler)(u8 * data, u32 data_len, void * priv_data),
			void      * priv_data,
			u32         ipi_vector,
			u32         target_cpu);

int
pisces_xbuf_server_deinit(struct pisces_xbuf_desc * xbuf_desc);


struct pisces_xbuf_desc *  
pisces_xbuf_client_init(uintptr_t xbuf_va, 
			u32       ipi_vector, 
			u32       target_cpu);



int 
pisces_xbuf_sync_send(struct pisces_xbuf_desc * desc,
		      u8                      * data, 
		      u32                       data_len, 
		      u8                     ** resp_data, 
		      u32                     * resp_len);


int
pisces_xbuf_send(struct pisces_xbuf_desc * desc, 
		 u8                      * data, 
		 u32                       data_len);


int 
pisces_xbuf_complete(struct pisces_xbuf_desc * desc, 
		     u8                      * data, 
		     u32                       data_len);

#endif
