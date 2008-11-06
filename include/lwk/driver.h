#ifndef _LWK_DRIVER_H
#define _LWK_DRIVER_H

#include <lwk/kernel.h>
#include <lwk/types.h>

/*
 * Drivers may set DRIVER_NAME before including this header.
 * Otherwise, the KBUILD name is used.
 */
#ifndef DRIVER_NAME
#define DRIVER_NAME KBUILD_MODNAME
#endif

/*
 * Driver parameter names have the form:
 *
 *      driver_name.parameter_name
 *
 * Example:
 * To set integer parameter foo in driver bar, add this
 * to the kernel boot command line:
 *
 *      bar.foo=1
 */
#define __DRIVER_PARAM_PREFIX	DRIVER_NAME "."

/*
 * For driver parameters.  Parameters can be configured via the
 * kernel boot command line or some other platform-dependent
 * runtime mechanism.
 */
#include <lwk/params.h>

#define driver_param(name, type) \
	__param_named(__DRIVER_PARAM_PREFIX, name, name, type)

#define driver_param_named(name, value, type) \
	__param_named(__DRIVER_PARAM_PREFIX, name, value, type)

#define driver_param_string(name, string, len) \
	__param_string(__DRIVER_PARAM_PREFIX, name, string, len)

#define driver_param_array(name, type, nump) \
	__param_array_named(__DRIVER_PARAM_PREFIX, name, name, type, nump)

#define driver_param_array_named(name, array, type, nump) \
	__param_array_named(__DRIVER_PARAM_PREFIX, name, array, type, nump)

/*
 * Every driver defines one of these structures via the
 * driver_init() macro.  The structure gets placed in the 
 * __driver_table ELF section and the kernel walks this table
 * to find/initialize all drivers in the system.
 */
struct driver_info {
	const char *	name;		// name of the driver
	const char *	type;		// Device type
	void		(*init)(void);	// driver initialization function
	int		init_called;	// set when .init() has been called,
					// used to prevent double inits.
};

/*
 * This adds a function to the __driver_table ELF section.
 */
#define driver_init(type,init_func) 				\
	static char __driver_name[] = DRIVER_NAME;			\
	static char __driver_type[] = type;			\
	static struct driver_info const __driver_info			\
	__attribute_used__						\
	__attribute__ ((unused,__section__ ("__driver_table"),		\
			aligned(sizeof(void *))))			\
	= { __driver_name, __driver_type, init_func };

/*
 * The __driver_table ELF section is surrounded by these symbols,
 * which are defined in the platform's linker script.
 */
extern struct driver_info __start___driver_table[];
extern struct driver_info __stop___driver_table[];

/*
 * This is a placeholder.  Currently drivers are never unloaded once loaded.
 */
#define driver_exit(exit_func)	


/** Initialize a single device.
 *
 * \return 0 for success
 */
extern int
driver_init_by_name(
	const char *		type,
	const char *		name
);


/** Initialize a list of device drivers.
 *
 * Given a comma separated list of device drivers, initialize
 * them each in turn.
 *
 * \note Modifies the string in place to nul terminate the devices!
 */
extern void
driver_init_list(
	const char *		type,
	char *			list
);

#endif
