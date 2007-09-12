#include <lwk/console.h>
#include <lwk/spinlock.h>
#include <lwk/params.h>


/** List of all registered consoles in the system.
 *
 * Kernel messages output via printk() will be written to
 * all consoles in this list.
 */
static LIST_HEAD(console_list);


/** Serializes access to the console. */
static DEFINE_SPINLOCK(console_lock);


/** Holds list of consoles to configure; parsed from kernel boot cmdline. */
static char consoles[128];
KERNEL_PARAM_STRING(console, consoles, sizeof(consoles),
	"List of consoles to configure. Example: console=vga,serial");


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


/** Finds the console driver corresponding to the specified name. */
static int
install_console_driver(char * name)
{
	struct console_driver *drvs = __start___console_driver_table;
	unsigned int num_drvs =
		__stop___console_driver_table - __start___console_driver_table;
	unsigned int i;

	for (i = 0; i < num_drvs; i++) {
		if (strcmp(name, drvs[i].name) == 0) {
			drvs[i].init();
			return 0;
		}
	}
	return -1;
}


/** Initializes the console subsystem; called once at boot. */
void
init_console( void )
{
	install_console_driver("vga");
	install_console_driver("serial");
}

