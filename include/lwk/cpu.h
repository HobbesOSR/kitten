#ifndef _LWK_CPU_H
#define _LWK_CPU_H

/** Maximum number of CPUs supported. */
#define NR_CPUS		CONFIG_NR_CPUS

#ifndef __ASSEMBLY__

void 
shutdown_cpus(void);


#endif /* __!ASSEMBLY__ */

#endif
