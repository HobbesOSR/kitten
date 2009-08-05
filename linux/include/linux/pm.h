/**
 * \file
 * The LWK doesn't have power management, so this is a bunch of NOPs.
 */

#ifndef __LINUX_PM_H
#define __LINUX_PM_H

#include <linux/types.h>

typedef uint32_t pm_message_t;

struct dev_pm_info { };

#endif
