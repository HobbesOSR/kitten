#include <lwk/spinlock.h>
#include <arch/proto.h>
#include <arch/uaccess.h>
#include <lwk/waitq.h>
#include <lwk/fdTable.h>
#include <lwk/poll.h>
#include <lwk/kfs.h>
#include <lwk/driver.h>

#include <arch/pisces/pisces_lcall.h>
#include <arch/pisces/pisces_boot_params.h>
#include <arch/pisces/pisces_xbuf.h>


extern struct pisces_boot_params * pisces_boot_params;
static struct pisces_xbuf_desc * xbuf_desc = NULL;

int 
pisces_lcall_exec(struct pisces_lcall * lcall, struct pisces_lcall_resp ** resp) {
    u32 resp_len = 0;

    if (xbuf_desc == NULL) {
	xbuf_desc = pisces_xbuf_client_init((uintptr_t)__va(pisces_boot_params->longcall_buf_addr), 0, 0);
    }

    //printk("xbuf_desc=%p\n", xbuf_desc);
    //printk("resp=%p\n", resp);

    pisces_xbuf_sync_send(xbuf_desc, 
    			  (u8 *)lcall, sizeof(struct pisces_lcall) + lcall->data_len, 
    			  (u8 **)resp, &resp_len);

    //printk("LCALL xmit complete, resp_len=%d\n", resp_len);
    //printk("*resp=%p\n", *resp);


    if (resp_len != sizeof(struct pisces_lcall_resp) + (*resp)->data_len) {
	printk(KERN_ERR "LCALL response data is invalid (length mismatch).... \n");
	printk(KERN_DEBUG "Response says length is %lu, but actual length is %u\n", 
	       sizeof(struct pisces_lcall_resp) + (*resp)->data_len, resp_len); 
	return -1;
    }

    return 0;
}

