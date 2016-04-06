#include <lwk/kernel.h>
#include "rcr_priv.h"


//
//
// TODO 
//   error messages in getNode et al functions
//
//

//
// RCRblackboard -- RCRdaemon subsection
//
// writer -- allocates its required counters and updates the specified addresses
//           expect an array of address to update will be kept and the lookup will only be
//           part of initialization
// reader -- locates the desired counters and when desired reads the most recent valuses
//           no protection from reader writing at the moment -- be careful
//           expect that meter locations will be saved and lookup will only occur during
//           initialization

//using namespace std;

// not sure how to make a constructor/destructor into C --akp 2/8/16
/*
  // default/empty constructor and destructor routines  -- fill in if needed -- akp
  RCRblackboard_RCRblackboard(RCRblackboard &bb) {
  }
  
  RCRblackboard_RCRblackboard() {
  }
  
  RCRblackboard::~RCRblackboard() {
  }
*/

/*****************************************
 *  private internal routines
 ****************************************/

/*********************************
 * Get MAC address of eth0 or eth1. It first tries eth0. It then tries eth1.
 * Returns null if no eth0 or eth1 are found.
 *********************************/

struct RCRblackboard_state blackboard;

// get address of shared memory region -- used as a base to compute locations using offsets
//   thoughtout the blackboard 

char * RCRblackboard_base(void) {
	return (char*) blackboard.bbMem;
}

size_t RCRblackboard_size(void) {
	return blackboard.bbSize;
}


// allocate next block of shared memory -- track highwater mark

int64_t RCRblackboard_allocateSharedMemory(int64_t size) {
	int64_t ret = blackboard.allocationHighWater;

	if (size == 0) {
		size = 8; // put blank word in if no space required to prevent field overlaps
	}

	//printk(" allocateSharedMemory(): size = %ld allocationHighWater = %ld\n",
	//       (long)size, (long)blackboard.allocationHighWater);

	if (size + blackboard.allocationHighWater > RCR_BLACKBOARD_SIZE) {
		panic("RCR exceeded blackboard size");
	}

	if (ret == 0) { // first time in, allocate some memory for the blackboard
		blackboard.bbSize = RCR_BLACKBOARD_SIZE;
		unsigned long num_pages = (RCR_BLACKBOARD_SIZE / PAGE_SIZE); 
		unsigned long order = ilog2(roundup_pow_of_two(num_pages));
		//printk("num_pages = %lu, order = %lu\n", num_pages, order);
		if ((blackboard.bbMem = kmem_get_pages(order)) == NULL) {
			panic("Failed to allocate memory for RCR blackboard");
		}
	}

	blackboard.allocationHighWater += size;

	return ret;
}


// get current allocation offset -- next allocation starting offset

int64_t RCRblackboard_getCurOffset(void) {
	return blackboard.allocationHighWater;
}

// rountines to aquire the blackboard internal stuctures given location in system tree

RCRNode * RCRblackboard_getNode(int nodeNum) {
        int64_t *nodeOffset = (int64_t *) (RCRblackboard_base() + blackboard.bb->nodeOffset);
	RCRNode * node = (RCRNode*) (RCRblackboard_base() + *(nodeOffset + nodeNum));
	return node;
}

RCRSocket * RCRblackboard_getSocket(int nodeNum, int socketNum) {
        int64_t *socketOffset = (int64_t *) (RCRblackboard_base() + (RCRblackboard_getNode(nodeNum)->socketOffset));
	RCRSocket * socket = (RCRSocket*) (RCRblackboard_base() + *(socketOffset + socketNum));
	return socket;
}

RCRCore * RCRblackboard_getCore(int nodeNum, int socketNum, int coreNum) {
        int64_t *coreOffset = (int64_t *) (RCRblackboard_base() + RCRblackboard_getSocket(nodeNum, socketNum)->coreOffset);
	RCRCore * core = (RCRCore*) (RCRblackboard_base() + *(coreOffset + coreNum));
	return core;
}

RCRThread * RCRblackboard_getThread(int nodeNum, int socketNum, int coreNum, int threadNum) {
        int64_t *threadOffset = (int64_t *) (RCRblackboard_base() + RCRblackboard_getCore(nodeNum, socketNum, coreNum)->threadOffset);
	RCRThread * thread = (RCRThread*) (RCRblackboard_base() + *(threadOffset + threadNum));
	return thread;
}

/*******************************
 * public interface functions
 *******************************/

// Get number of nodes/sockets/cores/threads in current blackboard system
//assumes # sockets per node all the same
/********************************************************************************/
/*********** ALL OF THESE SIZE FUNCTIONS ASSUME HOMOGENEOUS SYSTEM **************/
/********************************************************************************/

int64_t RCRblackboard_getNumOfNodes(void) {
	return blackboard.bb->numNodes;
}

int64_t RCRblackboard_getNumOfSockets(void) {
	RCRNode * node = RCRblackboard_getNode(0);
	return node->numSockets;
}

int64_t RCRblackboard_getNumOfCores(void) {
	RCRSocket * socket = RCRblackboard_getSocket(0, 0);
	return socket->numCores;
}

int64_t RCRblackboard_getNumOfThreads(void) {
	RCRCore * core = RCRblackboard_getCore(0, 0, 0);
	return core->numThreads;
}

char* RCRblackboard_getNodeName(void) {
	RCRNode * node = RCRblackboard_getNode(0);
	return node->md.nodeName;
}
/*******************************
 * routines to locate a particular meter -- need to know location and desired type
 *   returns ponter to counter/meter that can be used for the life of the program
 ******************************/

// get a specific system meter
volatile int64_t *RCRblackboard_getOSPowerLimit(int socket) {
         return (int64_t *)(RCRblackboard_base() +(sizeof(uint64_t)*socket));
}
void RCRblackboard_setOSPowerLimit(int64_t val) {
         int64_t * ptr = (int64_t *)RCRblackboard_base();

	 printk("base %lx val %ld\n", (unsigned long)ptr, (long)val);

	 *ptr = val;
}

// get a specific system meter
volatile int64_t *RCRblackboard_getSystemMeter(enum rcrMeterType id) {
	int64_t i = 0; // current index being checked

	// location of first meter
	RCRMeterValue *cur = (RCRMeterValue*) (RCRblackboard_base() + blackboard.bb->bbMeterOffset);

	while (i < blackboard.bb->numBBMeters) {  // check each until all meters examined
		if (id == cur->meterID) return &(cur->data.iValue); // return address of counter/meter
		cur++;
		i++;
	}

	return NULL; // ID not found 
}

// get a specific node meter 
volatile int64_t *RCRblackboard_getNodeMeter(enum rcrMeterType id, int64_t nodeId) {
	int64_t i = 0; // current index being checked

	RCRNode * node = RCRblackboard_getNode(nodeId);

	// location of first meter
	RCRMeterValue *cur = (RCRMeterValue*) (RCRblackboard_base() + node->nodeMeterOffset);

	while (i < node->numNodeMeters) {  // check each until all meters examined
		if (id == cur->meterID) return &(cur->data.iValue); // return address of counter/meter
		cur++;
		i++;
	}

	return NULL; // ID not found 
}

// get a specific socket meter 
volatile int64_t *RCRblackboard_getSocketMeter(enum rcrMeterType id, int64_t nodeId, int64_t socketId) {
	int64_t i = 0; // current index being checked

	RCRSocket * socket = RCRblackboard_getSocket(nodeId, socketId);

	// location of first meter
	RCRMeterValue *cur = (RCRMeterValue*) (RCRblackboard_base() + socket->socketMeterOffset);

	while (i < socket->numSocketMeters) {  // check each until all meters examined
		if (id == cur->meterID) return &(cur->data.iValue); // return address of counter/meter
		cur++;
		i++;
	}

	return NULL; // ID not found 
}

// get a specific core meter 
volatile int64_t *RCRblackboard_getCoreMeter(enum rcrMeterType id, int64_t nodeId, int64_t socketId, int64_t coreId) {
	int64_t i = 0; // current index being checked

	RCRCore * core = RCRblackboard_getCore(nodeId, socketId, coreId);

	// location of first meter
	RCRMeterValue *cur = (RCRMeterValue*) (RCRblackboard_base() + core->coreMeterOffset);

	while (i < core->numCoreMeters) {  // check each until all meters examined
		if (id == cur->meterID) return &(cur->data.iValue); // return address of counter/meter
		cur++;
		i++;
	}

	return NULL; // ID not found 
}

// get a specific thread meter 
volatile int64_t *RCRblackboard_getThreadMeter(enum rcrMeterType id, int64_t nodeId, int64_t socketId, int64_t coreId, int64_t threadId) {
	int64_t i = 0; // current index being checked

	RCRThread * thread = RCRblackboard_getThread(nodeId, socketId, coreId, threadId);

	// location of first meter
	RCRMeterValue *cur = (RCRMeterValue*) (RCRblackboard_base() + thread->threadMeterOffset);

	while (i < thread->numThreadMeters) {  // check each until all meters examined
		if (id == cur->meterID) return &(cur->data.iValue); // return address of counter/meter
		cur++;
		i++;
	}

	return NULL; // ID not found 
}

/******************************
 * assign a meter to be collected
 *   if counter/meter added returns true
 *   else returns false (all available counter/meters allocated)
 *****************************/
bool RCRblackboard_assignSystemMeter(enum rcrMeterType id) {
	int64_t i = 0; // current index being checked

	RCRMeterValue *cur = (RCRMeterValue *) (RCRblackboard_base() + blackboard.bb->bbMeterOffset);

	while (i < blackboard.bb->numBBMeters) {  // check each until all meters examined

		if (cur->meterID == -1) {
			cur->meterID = id;
			cur->data.iValue = 0;
			return true;
		}
		cur++;
		i++;
	}

	printk("no room to assign System meter\n");
	return false; // no room for assigned meter

}
;

bool RCRblackboard_assignNodeMeter(int64_t nodeId, enum rcrMeterType id) {
	int64_t i = 0; // current index being checked

	RCRNode * node = RCRblackboard_getNode(nodeId);
	RCRMeterValue *cur = (RCRMeterValue *) (RCRblackboard_base() + node->nodeMeterOffset);

	while (i < node->numNodeMeters) {  // check each until all meters examined

		if (cur->meterID == -1) {
			cur->meterID = id;
			cur->data.iValue = 0;
			return true;
		}
		cur++;
		i++;
	}

	printk("no room to assign Node meter on Node %llu\n", nodeId);
	return false; // no room for assigned meter
}
;

bool RCRblackboard_assignSocketMeter(int64_t nodeId, int64_t socketId, enum rcrMeterType id) {

	int64_t i = 0; // current index being checked

	RCRSocket * socket = RCRblackboard_getSocket(nodeId, socketId);
	RCRMeterValue *cur = (RCRMeterValue *) (RCRblackboard_base() + socket->socketMeterOffset);

	while (i < socket->numSocketMeters) {  // check each until all meters examined

		if (cur->meterID == -1) {
			cur->meterID = id;
			cur->data.iValue = 0;
			return true;
		}
		cur++;
		i++;
	}

	printk("no room to assign Socket meter on Node %llu Socket %llu\n", nodeId, socketId);
	return false; // no room for assigned meter
}
;

bool RCRblackboard_assignCoreMeter(int64_t nodeId, int64_t socketId, int64_t coreId, enum rcrMeterType id) {
	int64_t i = 0; // current index being checked

	RCRCore * core = RCRblackboard_getCore(nodeId, socketId, coreId);
	RCRMeterValue *cur = (RCRMeterValue *) (RCRblackboard_base() + core->coreMeterOffset);

	printk("numCoreMeters %llu\n", core->numCoreMeters);
	while (i < core->numCoreMeters) {  // check each until all meters examined

		if (cur->meterID == -1) {
			cur->meterID = id;
			cur->data.iValue = 0;
			return true;
		}
		cur++;
		i++;
	}

	printk("no room to assign Core meter on Node %llu Socket %llu Core %llu\n", nodeId, socketId, coreId);
	return false; // no room for assigned meter
}
;

bool RCRblackboard_assignThreadMeter(int64_t nodeId, int64_t socketId, int64_t coreId, int64_t threadId, enum rcrMeterType id) {
	int64_t i = 0; // current index being checked

	RCRThread *thread = RCRblackboard_getThread(nodeId, socketId, coreId, threadId);
	RCRMeterValue *cur = (RCRMeterValue *) (RCRblackboard_base() + thread->threadMeterOffset);

	while (i < thread->numThreadMeters) {  // check each until all meters examined

		if (cur->meterID == -1) {
			cur->meterID = id;
			cur->data.iValue = 0;
			return true;
		}
		cur++;
		i++;
	}

	printk("no room to assign Core meter on Node %llu Socket %llu Core %llu\n", nodeId, socketId, coreId);
	return false; // no room for assigned meter
}
;

/**********************************
 *  Build pieces of the system during initialization
 *********************************/

int64_t RCRblackboard_buildSystem(int64_t numBBMeters, int64_t numNodes) {
	int64_t i;
	int64_t offset = RCRblackboard_allocateSharedMemory(sizeof(RCRBlackboard));

	blackboard.bb = (RCRBlackboard*) (RCRblackboard_base() + offset);
	blackboard.bb->md.bbVersionNumber = RCRBlackBoardVersionNumber;
	blackboard.bb->numBBMeters = numBBMeters;
	blackboard.bb->bbMeterOffset = RCRblackboard_allocateSharedMemory(sizeof(RCRMeterValue) * numBBMeters);

	for (i = 0; i < numBBMeters; i++) {
	        RCROffset *meterOffset = (RCROffset*) (RCRblackboard_base() + blackboard.bb->bbMeterOffset);
		RCRMeterValue *cur = (RCRMeterValue *) (RCRblackboard_base() + meterOffset->offset[i]);
		cur->meterID = -1;   // allocated -- free
	}
	blackboard.bb->numNodes = numNodes;
	blackboard.bb->nodeOffset = RCRblackboard_allocateSharedMemory(sizeof(int64_t) * (numNodes + 1)); // 1 for size field
	// AKP -- is addition for size field correct??
	return offset;
}

int64_t RCRblackboard_buildNode(int64_t nodeId, int64_t numNodeMeters, int64_t numSockets) {
	int i;
	int64_t offset = RCRblackboard_allocateSharedMemory(sizeof(RCRNode));

	RCRNode* node = (RCRNode*) (RCRblackboard_base() + offset);
	node->md.nodeType = 0;
	node->md.nodeNumber = nodeId;
	node->md.nodeTreeSize = 0;
	node->md.nodeName = "nodename";
	node->numNodeMeters = numNodeMeters;
	node->nodeMeterOffset = RCRblackboard_allocateSharedMemory(sizeof(RCRMeterValue) * numNodeMeters);

	for (i = 0; i < numNodeMeters; i++) {
		RCRMeterValue *cur = (RCRMeterValue *) (RCRblackboard_base() + node->nodeMeterOffset + (sizeof(RCRMeterValue) * i)); // use offset to find
		cur->meterID = -1;  // allocated -- free
	}
	node->numSockets = numSockets;
	node->socketOffset = RCRblackboard_allocateSharedMemory(sizeof(RCRMeterValue) * (numSockets + 1));
	// 1 for size field  -- AKP -- is addition for size field correct??
	return offset;
}
;

int64_t RCRblackboard_buildSocket(int64_t socketId, int64_t numSocketMeters, int64_t numCores) {
	int i;
	int64_t offset = RCRblackboard_allocateSharedMemory(sizeof(RCRSocket));

	RCRSocket* socket = (RCRSocket*) (RCRblackboard_base() + offset);
	socket->md.socketType = 0;
	socket->md.socketNumber = socketId;
	socket->md.socketTreeSize = 0;
	socket->numSocketMeters = numSocketMeters;
	socket->socketMeterOffset = RCRblackboard_allocateSharedMemory(sizeof(RCRMeterValue) * numSocketMeters);

	for (i = 0; i < numSocketMeters; i++) {
		RCRMeterValue *cur = (RCRMeterValue *) (RCRblackboard_base() + socket->socketMeterOffset + (sizeof(RCRMeterValue) * i)); // use offset to find
		cur->meterID = -1;     // allocated -- free
	}

	socket->numCores = numCores;
	socket->coreOffset = RCRblackboard_allocateSharedMemory(sizeof(RCRMeterValue) * (numCores + 1));
	// 1 for size field  -- AKP -- is addition for size field correct??
	return offset;
}
;

int64_t RCRblackboard_buildCore(int64_t coreId, int64_t numCoreMeters, int64_t numThreads) {
	int64_t i;
	int64_t offset = RCRblackboard_allocateSharedMemory(sizeof(RCRCore));

	RCRCore* core = (RCRCore*) (RCRblackboard_base() + offset);
	core->md.coreType = 0;
	core->md.coreNumber = coreId;
	core->md.coreTreeSize = 0;
	core->numCoreMeters = numCoreMeters;
	core->coreMeterOffset = RCRblackboard_allocateSharedMemory(sizeof(RCRMeterValue) * numCoreMeters);

	for (i = 0; i < numCoreMeters; i++) {
		RCRMeterValue *cur = (RCRMeterValue *) (RCRblackboard_base() + core->coreMeterOffset + (sizeof(RCRMeterValue) * i)); // use offset to find
		cur->meterID = -1;     // allocated -- free
	}

	core->numThreads = numThreads;
	core->threadOffset = RCRblackboard_allocateSharedMemory(sizeof(RCRMeterValue) * (numThreads + 1));
	// 1 for size field  -- AKP -- is addition for size field correct??
	return offset;
}
;

int64_t RCRblackboard_buildThread(int64_t threadId, int64_t numThreadMeters) {
	int64_t i;
	int64_t offset = RCRblackboard_allocateSharedMemory(sizeof(RCRThread));

	RCRThread * thread = (RCRThread*) (RCRblackboard_base() + offset);
	thread->md.threadType = 0;
	thread->md.threadNumber = threadId;
	thread->md.threadTreeSize = 0;
	thread->numThreadMeters = numThreadMeters;
	thread->threadMeterOffset = RCRblackboard_allocateSharedMemory(sizeof(RCRMeterValue) * numThreadMeters);

	for (i = 0; i < numThreadMeters; i++) {
		RCRMeterValue *cur = (RCRMeterValue *) (RCRblackboard_base() + thread->threadMeterOffset + (sizeof(RCRMeterValue) * i)); // use offset to find
		cur->meterID = -1;     // allocated -- free
	}
	return offset;
}
;

/***********************************
 * functions to set my tree size and offset to children after allocation 
 **********************************/

/* for each System allocation */
void RCRblackboard_setSystemSize(int64_t initOffset) {
	int64_t curOffset = RCRblackboard_getCurOffset();
	RCRBlackboard * bb = (RCRBlackboard*) (RCRblackboard_base() + initOffset);
	bb->md.bbTreeSize = curOffset - initOffset; // assign now that the rest is written
}
;

/* for each Node allocation */
void RCRblackboard_setNodeSize(int64_t initOffset) {
	int64_t curOffset = RCRblackboard_getCurOffset();
	RCRNode * node = (RCRNode*) (RCRblackboard_base() + initOffset);
	node->md.nodeTreeSize = curOffset - initOffset; // assign now that the rest is written
}
;

void RCRblackboard_setNodeOffset(int64_t nodeId) {
	int64_t *nodeOffset = (int64_t *) (RCRblackboard_base() + blackboard.bb->nodeOffset + (sizeof(int64_t) * nodeId));
	*nodeOffset = RCRblackboard_getCurOffset();
}
;

/* for each socket allocation */
void RCRblackboard_setSocketSize(int64_t initOffset) {
	int64_t curOffset = RCRblackboard_getCurOffset();
	RCRSocket * sock = (RCRSocket*) (RCRblackboard_base() + initOffset);
	sock->md.socketTreeSize = curOffset - initOffset; // assign now that the rest is written
}
;

void RCRblackboard_setSocketOffset(int64_t nodeId, int64_t socketId) {
        RCRNode * node = RCRblackboard_getNode(nodeId);
	int64_t *socketOffset = (int64_t *) (RCRblackboard_base() + node->socketOffset + (sizeof(int64_t) * socketId));
	*socketOffset = RCRblackboard_getCurOffset();
}
;

/* for each core allocatoin */
void RCRblackboard_setCoreSize(int64_t initOffset) {
	int64_t curOffset = RCRblackboard_getCurOffset();
	RCRCore * core = (RCRCore*) (RCRblackboard_base() + initOffset);
	core->md.coreTreeSize = curOffset - initOffset; // assign now that the rest is written
}
;

void RCRblackboard_setCoreOffset(int64_t nodeId, int64_t socketId, int64_t coreId) {
	struct _RCRSocket * socket = RCRblackboard_getSocket(nodeId, socketId);
	int64_t *coreOffset = (int64_t *) (RCRblackboard_base() + socket->coreOffset + (sizeof(int64_t) * coreId));
	*coreOffset = RCRblackboard_getCurOffset();
}
;

/* for each thread allocation */
void RCRblackboard_setThreadSize(int64_t initOffset) {
	int64_t curOffset = RCRblackboard_getCurOffset();
	RCRThread * thread = (RCRThread*) (RCRblackboard_base() + initOffset);
	thread->md.threadTreeSize = curOffset - initOffset; // assign now that the rest is written
}
;

void RCRblackboard_setThreadOffset(int64_t nodeId, int64_t socketId, int64_t coreId, int64_t threadId) {
	RCRCore * core = RCRblackboard_getCore(nodeId, socketId, coreId);
	int64_t *threadOffset = (int64_t *) (RCRblackboard_base() + core->threadOffset + (sizeof(int64_t) * threadId));
	*threadOffset = RCRblackboard_getCurOffset();  // set thread offset inside core
}
;

/******************************
 * Build blackboard
 *****************************/

/*********  currently build homogeneous system  ****************/

bool RCRblackboard_buildSharedMemoryBlackBoard(int64_t systemMeters, int64_t numNode, int64_t nodeMeters, int64_t numSocket, int64_t socketMeters,
		int64_t numCore, int64_t coreMeters, int64_t numThread, int64_t threadMeters) {

	bool ret = true;
	blackboard.allocationHighWater = 0; // overwrites previous blackboard -- should be elsewhere?

	int64_t systemOffset = RCRblackboard_buildSystem(systemMeters, numNode);

	int n = 0;
	for (n = 0; n < numNode; n++) {
		RCRblackboard_setNodeOffset(n);
		int64_t nodeOffset = RCRblackboard_buildNode(n, nodeMeters, numSocket);
		int s = 0;
		for (s = 0; s < numSocket; s++) {
			RCRblackboard_setSocketOffset(n, s);
			int64_t sockOffset = RCRblackboard_buildSocket(s, socketMeters, numCore);
			int c = 0;
			for (c = 0; c < numCore; c++) {
				RCRblackboard_setCoreOffset(n, s, c);
				int64_t coreOffset = RCRblackboard_buildCore(c, coreMeters, numThread);
				int t = 0;
				for (t = 0; t < numThread; t++) {
					RCRblackboard_setThreadOffset(n, s, c, t);
					int64_t threadOffset = RCRblackboard_buildThread(t, threadMeters);
					RCRblackboard_setThreadSize(threadOffset);
				}
				RCRblackboard_setCoreSize(coreOffset);
			}
			RCRblackboard_setSocketSize(sockOffset);
		}
		RCRblackboard_setNodeSize(nodeOffset);
	}
	RCRblackboard_setSystemSize(systemOffset);

	printk("final offset %llu\n", RCRblackboard_getCurOffset());
	return ret;
}


/******************************
 *  reads current blackboard file and sets it up for access 
 *****************************/

bool RCRblackboard_initBlackBoard(void) {
#ifdef KEVIN
        const char * smName = RCRFILE_NAME;
	bool ret = true;
#endif

	//	printk("file name = %s\n", smName.c_str());

#ifdef KEVIN
	int fd = shm_open(smName, O_RDWR, 0);
#else
	int fd = 2000;
#endif

	if (fd == -1) {
		printk("Could not init blackboard\n");
		return false;
	}

#ifdef KEVIN
	ftruncate(fd, MAX_RCRFILE_SIZE);
	//printk("original bbMem = %lx errno %d\n", bbMem,errno);
	blackboard.bbMem = mmap(NULL, MAX_RCRFILE_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
	//printk("bbMem = %lx errno %d\n", bbMem,errno);
	blackboard.bb = (struct _RCRBlackboard*) blackboard.bbMem;
#endif

	return true;
}


/*********  build page for homogenoues Linux RCR file  ****************/

bool RCRblackboard_buildLinuxBlackBoard(const char * osName, int64_t powerLimit) 
{

        bool ret = true;

	blackboard.allocationHighWater = 8; // overwrites previous blackboard -- should be elsewhere?
	//  RCRblackboard_initBlackBoard(osName);
	RCRblackboard_initBlackBoard();
	
	//int64_t offset = RCRblackboard_allocateSharedMemory(sizeof(int64_t));
	RCRblackboard_allocateSharedMemory(sizeof(int64_t));
	
	RCRblackboard_setOSPowerLimit(powerLimit);
	
	printk("build Linux Page %llu\n", RCRblackboard_getCurOffset());
	return ret;

}


/******************************
 *  
 *****************************/

uint64_t * RCRblackboard_getLinuxPowerSched(int socket) {

        printk("getLinuxPowerSched Enter -- getNumOfSockets %ld socket %d\n", (long)RCRblackboard_getNumOfSockets(), socket);
	uint64_t *ret = (uint64_t *) (RCRblackboard_base() + (sizeof(uint64_t)*socket));
	printk("getLinuxPowerSched return -- ret %p socket %d\n", ret, socket);
	return ret;
}
