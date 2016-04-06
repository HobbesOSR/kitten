#include <lwk/driver.h>
#include <lwk/string.h>


static bool driver_debug;
param(driver_debug, bool);


/**
 * Searches for the specified driver name and calls its init() function.
 *
 * Returns 0 on success, -1 on failure.
 */
int
driver_init_by_name(
	const char *	type,
	const char *	name
)
{
	unsigned int i;
	struct driver_info * drv      = __start___driver_table;
	unsigned int         num_drvs = __stop___driver_table - __start___driver_table;

	int wildcard = strcmp(name, "*") == 0;

	for (i = 0; i < num_drvs; i++, drv++) {
		if (strcmp(type, drv->type) != 0)
			continue;
		if (!wildcard && strcmp(name, drv->name) != 0)
			continue;
		if (drv->init_called)
			return -1;

		if (driver_debug)
			printk(KERN_INFO "%s/%s: Initializing\n", type, drv->name);

		drv->init_called = 1;
		drv->init();

		// If we are not wildcarded, only init the first match.
		if (!wildcard)
			return 0;
	}

	// It is not an error to fall off the list if we are wildcarded
	if (wildcard)
		return 0;

	// No driver found for type/name pair
	if (driver_debug)
		printk(KERN_WARNING "%s/%s: No such driver?\n", type, name);

	return -1;
}


/**
 * Initialize all devices in a coma separated list.
 */
void
driver_init_list(
	const char *	type,
	char *		list
)
{
	char *p;

	for (p = list ; *p ; p++) {
		if (*p != ',')
			continue;
		*p = '\0'; // NULL terminate device

		if (driver_init_by_name(type, list) != 0)
			printk(KERN_WARNING "failed to install device %s/%s\n", type, list);

		list = p + 1;
	}

	// Catch the last one
	if (p != list && driver_init_by_name(type, list) != 0)
		printk(KERN_WARNING "failed to install device %s/%s\n", type, list);
}
