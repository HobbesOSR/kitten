#include <lwk/driver.h>
#include <lwk/string.h>

/**
 * Searches for the specified driver name and calls its init() function.
 *
 * Returns 0 on success, -1 on failure.
 */
int driver_init_by_name(const char *name)
{
	unsigned int i;
	struct driver_info * drvs	=	__start___driver_table;
	unsigned int         num_drvs	=	__stop___driver_table
						  - __start___driver_table;

	for (i = 0; i < num_drvs; i++) {
		if (strcmp(name, drvs[i].name) == 0) {
			if (drvs[i].init_called)
				return -1;
			drvs[i].init_called = 1;
			drvs[i].init();
			return 0;
		}
	}
	return -1;
}

