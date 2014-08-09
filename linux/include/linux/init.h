#ifndef _LINUX_INIT_H
#define _LINUX_INIT_H

#include <lwk/init.h>

/* Used for HOTPLUG... LWK doesn't support HOTPLUG so stub these out */
#define __devinit
#define __devinitdata
#define __devinitconst
#define __devexit
#define __devexitdata
#define __devexitconst
#define __devexit_p(x) (x)

/* These avoid "defined but not used" warnings */
#define fs_initcall(name)	int fs_initcall_##name(void) { return name(); }
#define subsys_initcall(name)	int subsys_initcall_##name(void) { return name(); }
#define core_initcall(name)	int core_initcall_##name(void) { return name(); }        
#define postcore_initcall(name)	int postcore_initcall_##name(void) { return name(); }
#define device_initcall(name)	int device_initcall_##name(void) { return name(); }
#define late_initcall(name)	int late_initcall_##name(void) { return name(); }
#define early_param(str,name)	int early_param_##name(void) { return name(""); }

/* Linux has compile-time checks for non-init code referencing init code, we don't */
#define __ref
#define __refdata
#define __refconst

#endif
