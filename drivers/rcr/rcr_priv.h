#ifndef _RCR_PRIV_H
#define _RCR_PRIV_H

#include <lwk/rcr/rcr.h>
#include <lwk/rcr/blackboard.h>

struct RCRblackboard_state {
	struct _RCRBlackboard *bb;   // pointer to root of the blackboard
	int64_t allocationHighWater; // current offset into the blackboard
	void * bbMem;                // pointer to blackboard memory
	size_t bbSize;               // size of the blackboard
};

char * RCRblackboard_base(void);
size_t RCRblackboard_size(void);

bool   RCRblackboard_buildSharedMemoryBlackBoard(
               int64_t systemMeters,
               int64_t numNode,
               int64_t nodeMeters,
               int64_t numSocket,
               int64_t socketMeters,
               int64_t numCore,
               int64_t coreMeters,
               int64_t numThread,
               int64_t threadMeters
       );

int64_t RCRblackboard_getNumOfNodes(void);
int64_t RCRblackboard_getNumOfSockets(void);
int64_t RCRblackboard_getNumOfCores(void);
int64_t RCRblackboard_getNumOfThreads(void);
char*   RCRblackboard_getNodeName(void);

bool RCRblackboard_assignSocketMeter(int64_t nodeId, int64_t socketId, enum rcrMeterType id);
volatile int64_t *RCRblackboard_getSocketMeter(enum rcrMeterType id, int64_t nodeId, int64_t socketId);

#endif
