#ifndef _NET_DST_H
#define _NET_DST_H

#include <net/neighbour.h>

struct dst_entry {
	struct neighbour *neighbour;
};

#endif
