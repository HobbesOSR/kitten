#include <lwk/types.h>
#include <lwk/string.h>
#include <arch/page.h>
#include <lwk/kernel.h>
#include <lwk/print.h>
#include <lwk/spinlock.h>
#include <lwk/interrupt.h>
#include <arch/proto.h>
#include <arch/apic.h>
#include <lwk/waitq.h>

#include <arch/pisces/pisces.h>
#include <arch/pisces/pisces_xbuf.h>


#define XBUF_READY     0x01ULL
#define XBUF_PENDING   0x02ULL
#define XBUF_STAGED    0x04ULL
#define XBUF_ACTIVE    0x08ULL
#define XBUF_COMPLETE  0x10ULL

struct pisces_xbuf {
	union {
		u64 flags;
		struct {
			u64 ready          : 1;   /* Flag set by enclave OS, after channel is init'd      */
			u64 pending        : 1;   /* Set when a message is ready to be received           */
			u64 staged         : 1;   /* Used by the endpoints for staged data transfers      */
			u64 active         : 1;   /* Set when a message has been accepted by the receiver */
			u64 complete       : 1;   /* Set by the receiver when message has been handled    */
			u64 rsvd          : 59;
		} __attribute__((packed));
	} __attribute__((packed));
    
	u32 host_apic;
	u32 host_vector;
	u32 enclave_cpu;
	u32 enclave_vector;
	u32 total_size;

	u32 data_len;

	u8 data[0];
} __attribute__((packed));



static void set_flags(struct pisces_xbuf * xbuf, u64 new_flags) {
	__asm__ __volatile__ ("lock xchgq %1, %0;"
			      : "+m"(xbuf->flags), "+r"(new_flags)
			      : 
			      : "memory");

}

static void raise_flag(struct pisces_xbuf * xbuf, u64 flags) {
	__asm__ __volatile__ ("lock orq %1, %0;"
			      : "+m"(xbuf->flags)
			      : "r"(flags)
			      : "memory");
}

static void lower_flag(struct pisces_xbuf * xbuf, u64 flags) {
	u64 inv_flags = ~flags;

	__asm__ __volatile__ ("lock andq %1, %0;"
			      : "+m"(xbuf->flags)
			      : "r"(inv_flags)
			      : "memory");
}

static u32 
init_stage_data(struct pisces_xbuf * xbuf, u8 * data, u32 data_len) 
{
	u32 xbuf_size  = xbuf->total_size;
	u32 staged_len = (data_len > xbuf_size) ? xbuf_size : data_len;
	
	xbuf->data_len = data_len;

	memcpy(xbuf->data, data, staged_len);
	raise_flag(xbuf, XBUF_STAGED);
	//xbuf->staged = 1;
	mb();
	
	return staged_len;
}

static u32 
send_data(struct pisces_xbuf * xbuf, u8 * data, u32 data_len) 
{
	u32 xbuf_size  = xbuf->total_size;
	u32 bytes_sent = 0;
	u32 bytes_left = data_len;
    
	while (bytes_left > 0) {
		u32 staged_len = (bytes_left > xbuf_size) ? xbuf_size : bytes_left;
	
		while (xbuf->staged == 1) {
			schedule();
			__asm__ __volatile__ ("":::"memory");
		}
	
		memcpy(xbuf->data, data + bytes_sent, staged_len);
		//	xbuf->staged = 1;
		raise_flag(xbuf, XBUF_STAGED);
		mb();


		bytes_sent += staged_len;
		bytes_left -= staged_len;
	}


	return bytes_sent;
}


static u32 
recv_data(struct pisces_xbuf * xbuf, u8 ** data, u32 * data_len)
{
	u32 xbuf_size  = xbuf->total_size;
	u32 bytes_read = 0;
	u32 bytes_left = xbuf->data_len;

	while (xbuf->staged == 0) {
		schedule();
		__asm__ __volatile__ ("":::"memory");
	}
    
	*data_len = xbuf->data_len;
	*data     = kmem_alloc(*data_len);


	//printk("Reading %u bytes\n", bytes_left);
	while (bytes_left > 0) {
		u32 staged_len = (bytes_left > xbuf_size) ? xbuf_size : bytes_left;


		while (xbuf->staged == 0) {
			schedule();
			__asm__ __volatile__ ("":::"memory");
		}

		//printk("Copying data (%d bytes) (bytes_left=%d) (xbuf_size=%d)\n", staged_len, bytes_left, xbuf_size);

		memcpy(*data + bytes_read, xbuf->data, staged_len);

	
		//	xbuf->staged = 0;
		lower_flag(xbuf, XBUF_STAGED);
		mb();

		bytes_read += staged_len;
		bytes_left -= staged_len;
	}

	return bytes_read;

}


int 
pisces_xbuf_sync_send(struct pisces_xbuf_desc * desc, 
		      u8                      * data, 
		      u32                       data_len,
		      u8                     ** resp_data, 
		      u32                     * resp_len) 
{
	struct pisces_xbuf * xbuf  = desc->xbuf;
	unsigned int         flags = 0;
	int acquired = 0;

	if (xbuf->ready == 0) {
		printk(KERN_ERR "Attempted longcall to unready host OS\n");
		return -1;
	}

	//printk("Sending XBUF request (idx=%llu)\n", xbuf_op_idx++);

	while (acquired == 0) {

		spin_lock_irqsave(&(desc->xbuf_lock), flags);
		{
			if (xbuf->pending == 0) {
				// clear all flags and signal that message is pending */
				//	    xbuf->flags = XBUF_READY | XBUF_PENDING;
				set_flags(xbuf, XBUF_READY | XBUF_PENDING);
				acquired = 1;
			}
		}
		spin_unlock_irqrestore(&(desc->xbuf_lock), flags);

		if (!acquired) {
			wait_event_interruptible(desc->xbuf_waitq, (xbuf->pending == 0));
		}
	}


    
	if ((data != NULL) && (data_len > 0)) {
		u32 bytes_staged = 0;

		bytes_staged = init_stage_data(xbuf, data, data_len);
	
		//printk("Staged %u bytes of data\n", bytes_staged);

		data_len -= bytes_staged;
		data     += bytes_staged;
	}


	//printk("Sending IPI %d to cpu %d\n", xbuf->host_vector, xbuf->host_apic);
	lapic_send_ipi_to_apic(xbuf->host_apic, xbuf->host_vector);
	//printk("IPI completed\n");

	send_data(xbuf, data, data_len);

	//printk("XBUF has been sent\n");

	/* Wait for complete flag to be 1 */
	while (xbuf->complete == 0) {
		schedule();
		__asm__ __volatile__ ("":::"memory");
	}


	//printk("XBUF is complete\n");

	if ((resp_data) && (xbuf->staged == 1)) {
		// Response exists and we actually want to retrieve it
		recv_data(xbuf, resp_data, resp_len);
	}


	mb();
	//printk("LCALL XBUF is now ready again\n\n");
	//    xbuf->flags = XBUF_READY;
	set_flags(xbuf, XBUF_READY);
	mb();

	waitq_wakeup(&(desc->xbuf_waitq));

	return 0;
}


int 
pisces_xbuf_send(struct pisces_xbuf_desc * desc, u8 * data, u32 data_len) 
{
	return pisces_xbuf_sync_send(desc, data, data_len, NULL, NULL);
}




int 
pisces_xbuf_complete(struct pisces_xbuf_desc * desc, 
		     u8                      * data, 
		     u32                       data_len) 
{
	struct pisces_xbuf * xbuf = desc->xbuf;

	//printk("Completing Xbuf xfer (data_len = %u) (data=%p)\n", data_len, data);
	
	if (xbuf->active == 0) {
		printk(KERN_ERR "Error: Attempting to complete an inactive xbuf\n");
		return -1;
	}

	//    xbuf->active = 0;
	lower_flag(xbuf, XBUF_ACTIVE);

	__asm__ __volatile__ ("":::"memory");


	if ((data_len > 0) && (data != NULL)) {
		u32 bytes_staged = 0;
		//printk("initing staged data\n");

		bytes_staged = init_stage_data(xbuf, data, data_len);
	
		data_len -= bytes_staged;
		data     += bytes_staged;
	}

	__asm__ __volatile__ ("":::"memory");

	raise_flag(xbuf, XBUF_COMPLETE);
	//    xbuf->complete = 1;    

	__asm__ __volatile__ ("":::"memory");

	//printk("Xbuf IS now complete\n");

    
	send_data(xbuf, data, data_len);

	//printk("XBUF response is fully sent\n");

	return 0;
}







static irqreturn_t
ipi_handler(int    irq, 
	    void * dev_id)
{	

	struct pisces_xbuf_desc * desc = dev_id;
	struct pisces_xbuf      * xbuf = NULL;
	u8 * data      = NULL;
	u32  data_len  = 0;
	//printk("IPI Received\n");

	//printk("desc=%p\n", desc);

	if (desc == NULL) {
		printk("IPI Handled for unknown XBUF\n");
		return IRQ_NONE;
	}

	xbuf = desc->xbuf;

	if (xbuf->active == 1) {
		printk(KERN_ERR "Error IPI for active xbuf, this should be impossible\n");
		return IRQ_NONE;
	}


	//printk("Receiving Data\n");
	recv_data(xbuf, &data, &data_len);
	//printk("Data_len=%d\n", data_len);
    
	//xbuf->active = 1;
	raise_flag(xbuf, XBUF_ACTIVE);
	mb();
	__asm__ __volatile__ ("":::"memory");

	if (desc->recv_handler) {
		//printk("Calling Receive handler for IPI\n");
		desc->recv_handler(data, data_len, desc->private_data);
	} else {
		printk("IPI Arrived for XBUF without a handler\n");
		//	xbuf->complete = 1;
		raise_flag(xbuf, XBUF_COMPLETE);
	}

	return IRQ_HANDLED;
}



struct pisces_xbuf_desc * 
pisces_xbuf_server_init(uintptr_t   xbuf_va, 
			u32         xbuf_total_bytes, 
			void      (*recv_handler)(u8 * data, u32 data_len, void * priv_data), 
			void      * private_data, 
                        u32         ipi_vector,
			u32         target_cpu) 
{
	struct pisces_xbuf_desc * desc = NULL;
	struct pisces_xbuf      * xbuf = (struct pisces_xbuf *)xbuf_va;
	

	
	desc = kmem_alloc(sizeof(struct pisces_xbuf_desc));

	if (desc == NULL) {
		printk(KERN_ERR "Could not allocate xbuf state\n");
		return NULL;
	}

	if (ipi_vector == -1) {
	   ipi_vector = irq_request_free_vector(ipi_handler, 0, "XBUF_IPI", desc);
	   
	   if (ipi_vector == -1) {
	       printk(KERN_ERR "Could not allocate free IRQ vector for XBUF IPI\n");

	       kmem_free(desc);
	       return NULL;
	   }
	} else {
	    if (irq_request(ipi_vector, ipi_handler, 0, "XBUF_IPI", desc) == -1) {
		printk(KERN_ERR "Could not register handler on IRQ %d for XBUF IPI\n", ipi_vector);
		kmem_free(desc);
		return NULL;
	    }
	}

	memset(desc, 0, sizeof(struct pisces_xbuf_desc));

	xbuf->enclave_cpu    = target_cpu;
	xbuf->enclave_vector = ipi_vector;
	xbuf->total_size     = xbuf_total_bytes - sizeof(struct pisces_xbuf);

	
	desc->xbuf         = xbuf;
	desc->private_data = private_data;
	desc->recv_handler = recv_handler;
	
	spin_lock_init(&(desc->xbuf_lock));
	waitq_init(&(desc->xbuf_waitq));

	//    xbuf->ready = 1;
	set_flags(xbuf, XBUF_READY);
	
	return desc;
}


struct pisces_xbuf_desc * 
pisces_xbuf_client_init(uintptr_t xbuf_va,
			u32       ipi_vector, 
			u32       target_cpu)
{
	struct pisces_xbuf      * xbuf = (struct pisces_xbuf *)xbuf_va;
	struct pisces_xbuf_desc * desc = kmem_alloc(sizeof(struct pisces_xbuf_desc));

	printk("XBUF client init At VA=%p\n", (void *)xbuf_va);

	if ((desc == NULL) || (xbuf == NULL)) {
		printk("Error initializing xbuf\n");
		return NULL;
	}

    	printk("LCALL IPI %d to cpu %d\n", xbuf->host_vector, xbuf->host_apic);

	memset(desc, 0, sizeof(struct pisces_xbuf_desc));

	xbuf->enclave_cpu    = target_cpu;
	xbuf->enclave_vector = ipi_vector;

	desc->xbuf = xbuf;
	spin_lock_init(&(desc->xbuf_lock));
	waitq_init(&(desc->xbuf_waitq));

	return desc;
}
