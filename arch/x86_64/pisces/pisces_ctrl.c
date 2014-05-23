
#include <lwk/types.h>
#include <arch/page.h>
#include <arch/ptrace.h>
#include <arch/proto.h>
#include <arch/uaccess.h>
#include <lwk/waitq.h>
#include <lwk/fdTable.h>
#include <lwk/poll.h>
#include <lwk/kfs.h>
#include <lwk/sched.h>
#include <lwk/driver.h>
#include <lwk/kthread.h>
#include <lwk/print.h>

/* For wake_up_process */
#include <linux/sched.h>


#include <arch/pisces/pisces.h>
#include <arch/pisces/pisces_boot_params.h>
#include <arch/pisces/pisces_xbuf.h>

extern struct pisces_boot_params * pisces_boot_params;
static struct pisces_xbuf_desc   * xbuf_desc = NULL;

static waitq_t   user_waitq;
static u8      * cmd_data   = NULL;
static u32       cmd_len    = 0;

// This has to be done in a single operation
static ssize_t 
cmd_write(struct file       * filp,
	  const char __user * ubuf, 
	  size_t              size, 
	  loff_t            * off) 
{
	u8 * resp = NULL;

	cmd_data  = NULL;
	cmd_len   = 0;

	resp = kmem_alloc(size);

	if (!resp) {
		printk(KERN_ERR "Unable to allocate buffer for command response\n");
		return -ENOMEM;
	}


	// write response to buffer
	if (copy_from_user(resp, ubuf, size)) {
		return -EINVAL;
	}


	pisces_xbuf_complete(xbuf_desc, resp, size);

	kmem_free(resp);
	return size;
}


/* Note: the offset (off) is not implemented, 
   so reads always start from the begining of the command. 

   This means that reading a command with data requires 2(!!) read operations
*/
static ssize_t 
cmd_read(struct file * filp, 
	 char __user * ubuf, 
	 size_t        size, 
	 loff_t      * off) 
{
	//	u32 cmd_len = 0;
	u32 read_len = 0;


	while (cmd_data == NULL) {
		wait_event_interruptible(user_waitq,
					 (cmd_data != NULL));
	}

 
	/*
	  if ((sizeof(struct pisces_cmd) + cmd->data_len) <= *off) {
	  return 0;
	  }
	  
	  read_len = cmd_len - *off;
	*/

	read_len = (cmd_len > size) ? size : cmd_len;
	
	//printk("Copying CMD to userspace (read_len=%d)\n", read_len);
	//printk("size=%lu, cmd_len=%d\n", size, cmd_len);

	if (copy_to_user((void *)ubuf, cmd_data, read_len)) {
		return -EFAULT;
	}


	//	*off += read_len;

	// If we read to the end, then reset offset to beginning
	/*
	  if (*off >= cmd_len) {
	  *off = 0;
	  }
	*/

	return read_len;
}

static unsigned int 
cmd_poll(struct file              * filp,
	 struct poll_table_struct * table) 
{
	unsigned int mask = 0;

	poll_wait(filp, &(user_waitq), table);
	
	if (cmd_data != NULL) {
		mask |= (POLLIN | POLLOUT);
		mask |= (POLLRDNORM | POLLWRNORM);
	}

	return mask;
}



static int 
cmd_open(struct inode * inodep, 
	 struct file  * filp) 
{
	return 0;
}



static int 
cmd_close(struct file * filp) 
{
	return 0;
}

static long 
cmd_ioctl(struct file  * filp,
	  unsigned int   ioctl, 
	  unsigned long  arg)
{
	return 0;
}

static struct kfs_fops pisces_ctrl_fops = {
	.open           = cmd_open, 
	.write          = cmd_write,
	.read           = cmd_read,
	.poll           = cmd_poll, 
	.close          = cmd_close,
	.unlocked_ioctl = cmd_ioctl,
};


static void 
cmd_handler(u8    * data, 
	    u32     data_len, 
	    void  * priv_data)
{	
	cmd_data = data;
	cmd_len  = data_len;

	__asm__ __volatile__("":::"memory");

	waitq_wakeup(&(user_waitq));

	return;
}

static int 
pisces_ctrl_init(void)
{



	waitq_init(&(user_waitq));

	kfs_create("/pisces-cmd",
		   NULL,
		   &pisces_ctrl_fops, 
		   0777, 
		   NULL, 0);

	xbuf_desc = pisces_xbuf_server_init((uintptr_t)__va(pisces_boot_params->control_buf_addr), 
					    pisces_boot_params->control_buf_size, 
					    cmd_handler, NULL, -1, 0);		 

	if (xbuf_desc == NULL) {
		printk(KERN_ERR "Could not initialize cmd/ctrl xbuf channel\n");
		return -1;
	}

	return 0;
}


DRIVER_INIT("kfs", pisces_ctrl_init);
