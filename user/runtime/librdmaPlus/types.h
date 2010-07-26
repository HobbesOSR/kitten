#ifndef _rdma_types_h
#define _rdma_types_h

#include <stdint.h>
#include <sys/types.h>

namespace rdmaPlus {

typedef uint32_t Tag;
typedef size_t Size;

typedef uint32_t Nid;
typedef uint32_t Pid;
struct ProcId {
	Nid nid;
	Pid pid;
};
typedef Pid Port;

struct Key {
	ProcId id;
	Tag tag;
	Size count;
};

struct Status {
	Key key;
};

typedef enum { Read, Write } memOp_t;

struct Event {
	memOp_t op;
	void* addr;
	Size length;
};

struct RemoteEntry {
    uint64_t    addr;
    uint32_t    key;
    uint32_t    length;
};

} // namespace rdmaPlus

#endif
