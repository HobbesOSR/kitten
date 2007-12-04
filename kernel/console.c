#include <lwk/kernel.h>
#include <lwk/console.h>
#include <lwk/spinlock.h>
#include <lwk/params.h>
#include <lwk/driver.h>
#include <lwk/errno.h>
#include <arch/uaccess.h>

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

/**
 * Initializes the console subsystem; called once at boot.
 */
void console_init(void)
{
	char *p, *con;
	
	// console_str contains comma separated list of console
	// driver names.  Try to install a driver for each
	// console name con.
	p = con = console_str;
	while (*p != '\0') {
		if (*p == ',') {
			*p = '\0'; // null terminate con
			if (driver_init_by_name(con))
				printk(KERN_WARNING
				       "failed to install console=%s\n", con);
			con = p + 1;
		}
		++p;
	}

	// Catch the last one
	if (p != console_str)
		driver_init_by_name(con);
}

/**
 * System Call: Writes a string to the local console.
 */
long sys_write_console(const char __user *buf, size_t count)
{
	char   kbuf[512];
	size_t kcount = count;

	/* Protect against overflowing the kernel buffer */
	if (kcount >= sizeof(kbuf))
		kcount = sizeof(kbuf) - 1;

	/* Copy the user-level string to a kernel buffer */
	/* TODO: Fix this to perform access checks */
	if (__copy_from_user(kbuf, buf, kcount))
		return -EFAULT;
	kbuf[kcount] = '\0';

	/* Write the string to the local console */
	printk(KERN_USERMSG
		"(TSK-%u) %s%s\n",
		current->task_id,
		kbuf,
		(kcount != count) ? " <TRUNCATED>" : ""
	);

	/* Return number of characters actually printed */
	return kcount;
}

