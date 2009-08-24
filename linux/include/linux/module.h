/**
 * \file
 * The LWK doesn't have modules, so this is a bunch of NOPs.
 */

#ifndef __LINUX_MODULE_H
#define __LINUX_MODULE_H

#include <lwk/driver.h>
#include <lwk/stat.h>

#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/gfp.h>

#define THIS_MODULE	((void*) __FILE__)

#define EXPORT_SYMBOL(sym)
#define EXPORT_SYMBOL_GPL(sym)
#define EXPORT_SYMBOL_GPL_FUTURE(sym)
#define EXPORT_PER_CPU_SYMBOL(var) EXPORT_SYMBOL(per_cpu__##var)
#define EXPORT_PER_CPU_SYMBOL_GPL(var) EXPORT_SYMBOL_GPL(per_cpu__##var)

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

struct module;

static inline int try_module_get(struct module *module) { return 1; }
static inline void module_put(struct module *module) { }

#endif
