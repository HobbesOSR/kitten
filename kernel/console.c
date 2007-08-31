#include <lwk/console.h>
#include <lwk/spinlock.h>

/** List of all registered consoles in the system.
 *
 * Kernel messages output via printk() will be written to
 * all consoles in this list.
 */
static LIST_HEAD(console_list);

/** Serializes access to the console. */
static DEFINE_SPINLOCK(console_lock);


/** Registers a new console. */
void
console_register(
	struct console *con
)
{
	list_add(&con->next, &console_list);
}


/** Writes a string to all registered consoles. */
void
console_write(
	const char *str
)
{
	struct console *con;

	spin_lock(&console_lock);
	list_for_each_entry(con, &console_list, next)
		con->write(con, str);
	spin_unlock(&console_lock);
}

