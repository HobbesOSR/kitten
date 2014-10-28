
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
#include <lwk/aspace.h>

#include <arch/pisces/pisces.h>
#include <arch/pisces/pisces_boot_params.h>
#include <arch/pisces/pisces_xbuf.h>
#include <arch/pisces/pisces_file.h>

extern struct pisces_boot_params * pisces_boot_params;
static struct pisces_xbuf_desc   * xbuf_desc = NULL;

static waitq_t   user_waitq;
static u8      * cmd_data   = NULL;
static u32       cmd_len    = 0;



/*** Local Kitten Requests for Pisces ***/

#define PISCES_STAT_FILE   500
#define PISCES_LOAD_FILE   501
#define PISCES_WRITE_FILE  502


struct pisces_user_file_info {
    unsigned long long user_addr;
    unsigned int       path_len;
    char               path[0];
} __attribute__((packed)); 






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
    char __user * uptr = (char __user *)arg;
    int ret = 0;

    switch (ioctl) {
	case PISCES_STAT_FILE: {
	    struct pisces_user_file_info file_info;
	    loff_t file_size   = 0;
	    u64    file_handle = 0;
	    
	    memset(&file_info, 0, sizeof(struct pisces_user_file_info));
	    
	    if (copy_from_user(&file_info, uptr, sizeof(struct pisces_user_file_info))) {
		printk(KERN_ERR "Unable to copy to file info from user space\n");
		return -1;
	    }
	    
	    if (copy_from_user(file_info.path, uptr + sizeof(struct pisces_user_file_info), file_info.path_len)) {
		printk(KERN_ERR "Unable to copy file path from user space\n");
		return -1;
	    }
	    
	    file_handle = pisces_file_open(file_info.path, O_RDONLY);
    
	    if (file_handle == 0) {
		printk(KERN_ERR "Could not find file (%s)\n", file_info.path);
		return -1;
	    }

	    file_size =  pisces_file_size(file_handle);

	    pisces_file_close(file_handle);
	    
	    return file_size;

	    break;
	}
	case PISCES_LOAD_FILE: {

	    struct pisces_user_file_info file_info;
	    loff_t  file_size   = 0;
	    u64     file_handle = 0;
	    paddr_t addr_pa     = 0;
	    ssize_t bytes_read  = 0;
	    
	    memset(&file_info, 0, sizeof(struct pisces_user_file_info));
	    
	    if (copy_from_user(&file_info, uptr, sizeof(struct pisces_user_file_info))) {
		printk(KERN_ERR "Unable to copy to file info from user space\n");
		return -1;
	    }
	    
	    if (copy_from_user(file_info.path, uptr + sizeof(struct pisces_user_file_info), file_info.path_len)) {
		printk(KERN_ERR "Unable to copy file path from user space\n");
		return -1;
	    }

	    if (aspace_virt_to_phys(current->aspace->id, file_info.user_addr, &addr_pa) < 0) {
		printk(KERN_ERR "Invalid user address used for loading file\n");
		return -1;
	    }
	    
	    file_handle = pisces_file_open(file_info.path, O_RDONLY);
    
	    if (file_handle == 0) {
		printk(KERN_ERR "Could not find file (%s)\n", file_info.path);
		return -1;
	    }
	    	    
	    file_size =  pisces_file_size(file_handle);
	    bytes_read = pisces_file_read(file_handle, __va(addr_pa), file_size, 0);

	    pisces_file_close(file_handle);

	    if (bytes_read != file_size) {
		printk(KERN_ERR "Could not load file (%s) [only read %ld bytes]\n", 
		       file_info.path, bytes_read);
		return -1;
	    }
	    
	    return 0;

	    break;
	}
	case PISCES_WRITE_FILE:

	default:
	    printk(KERN_ERR "Invalid Pisces IOCTL (%d)\n", ioctl);
	    return -1;
    }

    return ret;
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
