#include <lwk/kernel.h>
#include <lwk/console.h>
#include <lwk/spinlock.h>
#include <lwk/params.h>
#include <lwk/driver.h>

/** List of all registered consoles in the system.
 *
 * Kernel messages output via printk() will be written to
 * all consoles in this list.
 */
static LIST_HEAD(console_list);

/** Serializes access to the console. */
static DEFINE_SPINLOCK(console_lock);

/** Holds a comma separated list of consoles to configure. */
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

	spin_lock(&console_lock);
	list_for_each_entry(con, &console_list, next)
		con->write(con, str);
	spin_unlock(&console_lock);
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

