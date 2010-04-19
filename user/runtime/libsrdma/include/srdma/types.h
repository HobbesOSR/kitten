#ifndef srdma_types_h
#define srdma_types_h

#include <sys/types.h>

namespace srdma {

typedef u_int32_t Tag;
typedef size_t Size;

typedef u_int32_t Nid;
typedef u_int32_t Pid;
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

typedef enum { Read, Write } MemOp;

struct Event {
	MemOp op;
	void* addr;
	Size length;
};

} // namespace srdma

#endif
