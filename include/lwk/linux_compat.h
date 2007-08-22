#ifndef _LWK_LINUX_COMPAT_H
#define _LWK_LINUX_COMPAT_H

/* Mostly Linux stuff that is not implemented or supported in LWK */

#define EXPORT_SYMBOL(sym)
#define EXPORT_SYMBOL_GPL(sym)
#define EXPORT_SYMBOL_GPL_FUTURE(sym)
#define EXPORT_PER_CPU_SYMBOL(var) EXPORT_SYMBOL(per_cpu__##var)
#define EXPORT_PER_CPU_SYMBOL_GPL(var) EXPORT_SYMBOL_GPL(per_cpu__##var)

#endif
