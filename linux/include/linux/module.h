/**
 * \file
 * The LWK doesn't have modules, so this is a bunch of NOPs.
 */

#ifndef __LINUX_MODULE_H
#define __LINUX_MODULE_H

#include <lwk/linux_compat.h>
#include <lwk/driver.h>
#include <lwk/stat.h>

#define THIS_MODULE	((void*) __FILE__)

#define MODULE_AUTHOR(author)
#define MODULE_DESCRIPTION(desc)
#define MODULE_LICENSE(license)

#define module_init(init_func) DRIVER_INIT("LINUX", (init_func))
#define module_exit(exit_func) DRIVER_EXIT((exit_func))

#endif
