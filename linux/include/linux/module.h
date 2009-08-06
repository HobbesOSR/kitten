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
#define MODULE_VERSION(version)
#define MODULE_PARM_DESC(parm,desc)
#define MODULE_DEVICE_TABLE(type,name)

#define module_param(name,type,perm) \
	DRIVER_PARAM(name,type)
#define module_param_named(name,value,type,perm) \
	DRIVER_PARAM_NAMED(name,value,type)
#define module_param_string(name,string,len,perm) \
	DRIVER_PARAM_STRING(name,string,len)
#define module_param_array(name,type,nump,perm) \
	DRIVER_PARAM_ARRAY(name,type,nump)
#define module_param_array_named(name,array,type,nump,perm) \
	DRIVER_PARAM_ARRAY_NAMED(name,array,type,nump)

#define module_init(init_func) DRIVER_INIT("LINUX", (init_func))
#define module_exit(exit_func) DRIVER_EXIT((exit_func))

#endif
