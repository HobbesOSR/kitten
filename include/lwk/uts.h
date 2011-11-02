#ifndef _LWK_UTS_H
#define _LWK_UTS_H

#include <lwk/compile.h>  /* for UTS_MACHINE and UTS_VERSION */

/*
 * Defines for what uname() should return 
 * We trick user-level into thinking we are Linux for compatibility purposes.
 */
#ifndef UTS_LINUX_SYSNAME
#define UTS_LINUX_SYSNAME "Linux"
#endif

#ifndef UTS_LINUX_RELEASE
#define UTS_LINUX_RELEASE "2.6.35"
#endif

#ifndef UTS_NODENAME
#define UTS_NODENAME "(none)"	/* set by sethostname() */
#endif

#ifndef UTS_DOMAINNAME
#define UTS_DOMAINNAME "(none)"	/* set by setdomainname() */
#endif

#endif
