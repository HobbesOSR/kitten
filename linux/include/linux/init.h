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

#endif
