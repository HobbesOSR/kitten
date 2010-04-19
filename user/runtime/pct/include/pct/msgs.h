#ifndef PCT_MSGS_H
#define PCT_MSGS_H

#include <srdma/dm.h>

#define CTRL_MSG_TAG 0xdead
#define MAX_FANOUT 7 
#define SYSCALL_MSG_KEY 0xbeef

using namespace srdma;
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
        MemRegion::Id   imageKey;
        MemRegion::Addr imageAddr;
        MemRegion::Id   nidRnkMapKey;
        MemRegion::Addr nidRnkMapAddr;
        size_t      elf_len;
        size_t      heap_len;
        size_t      stack_len;
        uint        cmdLine_len;
        uint        env_len;
        uint        gid;
        uint        uid;
        uint        nRanks;
        uint        nNids;
        uint        fanout;
        uint        childNum;
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

#endif
