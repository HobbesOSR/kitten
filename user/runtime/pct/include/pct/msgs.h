#ifndef PCT_MSGS_H
#define PCT_MSGS_H

#include <rdmaPlus.h>

#define CTRL_MSG_TAG 0xdead
#define ORTE_MSG_TAG 0xf00d
#define MAX_FANOUT 7 
#define SYSCALL_MSG_KEY 0xbeef

using namespace rdmaPlus;
typedef int JobId; 

struct NidRnk {
    Nid  nid;
    uint baseRank;
    uint ranksPer; 
};

static inline int Ranks2Nids( int ranks, int ranksPer )
{
    return ranks / ranksPer + (ranks % ranksPer?1:0);
}

struct JobMsg {

    typedef enum { Load = 1,
                    NidPidMapPart,
                    NidPidMapAll,
                    Kill,
                    ChildStart,
                    ChildExit } Type;

    struct Load {
        MemRegion::Id   nidRnkMapKey;
        MemRegion::Addr nidRnkMapAddr;
        uint        nNids;
        uint        fanout;
        uint        childNum;

        struct App {
            MemRegion::Id   imageKey;
            MemRegion::Addr imageAddr;
            size_t      elf_len;
            size_t      heap_len;
            size_t      stack_len;
            uint        cmdLine_len;
            uint        env_len;
            uint        gid;
            uint        uid;
            uint        nRanks;
        } app;
    };

    struct Kill {
        uint signal;
    };

    struct NidPidMapAll {
        MemRegion::Id   key;
        MemRegion::Addr addr;
    };

    struct NidPidMapPart {
        MemRegion::Id   key;
        MemRegion::Addr addr;
        uint         baseRank;
        uint         numRank;
    };

    struct ChildExit {
        Nid num;
    };

    struct ChildStart { 
        Nid num;
    };

    Type    type;
    JobId   jobId;

    union {
        struct Kill             kill; 
        struct Load             load; 
        struct NidPidMapAll     nidPidMapAll;
        struct NidPidMapPart    nidPidMapPart;
        struct ChildExit        childExit;
        struct ChildStart       childStart;
    } u;
};


struct CtrlMsg {
    typedef enum { Job = 1 } CtrlType;
    CtrlType        type;
    ProcId          src;
    union {
        JobMsg      job;
    } u;
} __attribute((aligned(8)));

struct OrteMsg {
	enum { OOB, Barrier } type;
	union u {
		struct oob {
			unsigned char data[128];
		} oob;
		struct barrier {
			typedef enum { Up, Down } type_t;	
			type_t	type;
		} barrier;
	} u;
} __attribute((aligned(8)));


#endif
