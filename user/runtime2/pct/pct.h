#ifndef PCT_H
#define PCT_H

// The well-known portals user ID of the PCT
#define PCT_PTL_UID         0

// The well-known portals process ID of the PCT
#define PCT_PTL_PID         1

// The well-known portals table index for PMI messages
#define PCT_PMI_PT_INDEX    1

// The well-known maximum size of PMI message, in bytes
#define PCT_MAX_PMI_MSG     1024

#ifdef USING_PIAPI
/* The well-known powerinsight agent saddr */
#define PIAPI_AGNT_SADDR    0x0a361500

/* The well-known powerinsight agent port */
#define PIAPI_AGNT_PORT     20201
#endif

#endif
