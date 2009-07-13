/** \file
 * Device driver initialization table management.
 *
 * Device drivers initialization functions are stored in a special
 * ELF segment so that the kernel can walk the dynamically generated
 * list without a lengthy set of \#ifdef CONFIG_FOO statements.
 *
 * Devices have a type associated with them, as well as a name.
 * The type is used to control when (and if) the driver is initialized
 * at kernel startup.
 *
 * Currently supported device types:
 * - "console": Output console devices
 * - "net": Network devices
 * - "late": Devices that are to be brought up late, if they are linked in
 *
 * 
 */
#ifndef _LWK_DRIVER_H
#define _LWK_DRIVER_H

#include <lwk/kernel.h>
#include <lwk/types.h>

/**
 * Name of the driver.
 *
 * Drivers may set DRIVER_NAME before including this header.
 * Otherwise, the KBUILD name is used.
 */
#ifndef DRIVER_NAME
#define DRIVER_NAME KBUILD_MODNAME
#endif

/**
 * Driver parameter names have the form:
 *
 *      driver_name.parameter_name
 *
 * Example:
 *
 * To set integer parameter foo in driver bar, add this
 * to the kernel boot command line:
 *
 *      bar.foo=1
 */
#define __DRIVER_PARAM_PREFIX	DRIVER_NAME "."

/**
 * \name Driver parameters.
 *
 * Parameters can be configured via the kernel boot command line
 * or some other platform-dependent runtime mechanism.
 * @{
 */

#include <lwk/params.h>

#define DRIVER_PARAM(name, type) \
	__param_named(__DRIVER_PARAM_PREFIX, name, name, type)

#define DRIVER_PARAM_NAMED(name, value, type) \
	__param_named(__DRIVER_PARAM_PREFIX, name, value, type)

#define DRIVER_PARAM_STRING(name, string, len) \
	__param_string(__DRIVER_PARAM_PREFIX, name, string, len)

#define DRIVER_PARAM_ARRAY(name, type, nump) \
	__param_array_named(__DRIVER_PARAM_PREFIX, name, name, type, nump)

#define DRIVER_PARAM_ARRAY_NAMED(name, array, type, nump) \
	__param_array_named(__DRIVER_PARAM_PREFIX, name, array, type, nump)

/** @} */

/** Driver initialization structure.
 *
 * Every driver defines one of these structures via the
 * DRIVER_INIT() macro.  The structure gets placed in the 
 * __driver_table ELF section and the kernel walks this table
 * to find/initialize all drivers in the system.
 *
 * \note A typical driver file will not directly instantiate this structure.
 */
struct driver_info {
	const char *	name;		//!< name of the driver
	const char *	type;		//!< Device type
	int		(*init)(void);	//!< driver initialization function
	int		init_called;	//!< set when .init() has been called,
					//!< used to prevent double inits.
};

/** Create a driver_info structure.
 *
 * This adds a function to the __driver_table ELF section.
 * \param type should be one of the known types.
 * \param init_func should take be a thunk that will bring up the driver.
 */
#define DRIVER_INIT(type,init_func) 					\
	static char __driver_name[] = DRIVER_NAME;			\
	static char __driver_type[] = type;				\
	static struct driver_info const __driver_info			\
	__attribute_used__						\
	__attribute__ ((unused,__section__ ("__driver_table"),		\
			aligned(sizeof(void *))))			\
	= { __driver_name, __driver_type, init_func };

/**
 * Driver initialization table markers.
 *
 * The __driver_table ELF section is surrounded by these symbols,
 * which are defined in the platform's linker script.
 * @{
 */
extern struct driver_info __start___driver_table[];
extern struct driver_info __stop___driver_table[];

/** @} */


/**
 * Register an exit function for the driver.
 *
 * \note This is a placeholder.
 * Currently drivers are never unloaded once loaded.
 */
#define DRIVER_EXIT(exit_func)	


/** Initialize a single device.
 *
 * Device must be an exact match for the device type and the device name.
 * As a special case '*' may be passed in for the name to initialize
 * all devices of the listed type.
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
