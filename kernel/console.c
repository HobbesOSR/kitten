#include <lwk/kernel.h>
#include <lwk/console.h>
#include <lwk/spinlock.h>
#include <lwk/params.h>
#include <lwk/driver.h>
#include <lwk/errno.h>
#include <arch/uaccess.h>
#include <lwk/kfs.h>

/**
 * List of all registered consoles in the system.
 *
 * Kernel messages output via printk() will be written to
 * all consoles in this list.
 */
static LIST_HEAD(console_list);

/**
 * Serializes access to the console.
 */
static DEFINE_SPINLOCK(console_lock);

/**
 * Holds a comma separated list of consoles to configure.
 */
static char console_str[128];
param_string(console, console_str, sizeof(console_str));

/**
 * Registers a new console.
 */
void console_register(struct console *con)
{
	list_add(&con->next, &console_list);
}

/**
 * Writes a string to all registered consoles.
 */
void console_write(const char *str)
{
	struct console *con;
	unsigned long flags;

	spin_lock_irqsave(&console_lock, flags);
	list_for_each_entry(con, &console_list, next)
		con->write(con, str);
	spin_unlock_irqrestore(&console_lock, flags);
}


/** Write a user string to the console */
static ssize_t
console_user_write(
	struct kfs_file *	file,
	uaddr_t			buf,
	size_t			len
)
{
	char			kbuf[ 512 ];
	size_t			klen = len;
	if( klen > sizeof(kbuf)-1 )
		klen = sizeof(kbuf)-1;

	if( copy_from_user( kbuf, (void*) buf, klen ) )
		return -EFAULT;

	kbuf[ klen ] = '\0';

	printk( KERN_USERMSG
		"(%s) %s%s",
		current->name,
		kbuf,
		klen != len ? "<TRUNC>" : ""
	);

	return klen;
}


static struct kfs_fops
kfs_console_fops = {
	.write		= console_user_write
};



/**
 * Initializes the console subsystem; called once at boot.
 */
void
console_init(void)
{
	driver_init_list( "console", console_str );
}


void
console_user_init(void)
{
	kfs_create(
		"/dev/console",
		&kfs_console_fops,
		0777,
		0,
		0
	);
}

driver_init( "kfs", console_user_init );

