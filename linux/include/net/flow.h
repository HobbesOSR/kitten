#ifndef _NET_FLOW_H
#define _NET_FLOW_H

#include <linux/types.h>

struct flowi {
	union {
		struct {
			__be32          daddr;
			__be32          saddr;
			__u8            tos;
			__u8            scope;
		} ip4_u;
	} nl_u;
};

#endif
