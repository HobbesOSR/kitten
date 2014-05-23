#ifndef __PISCES_XPMEM_H__
#define __PISCES_XPMEM_H__


#ifdef __KERNEL__
#include <lwk/types.h>
#endif /* __KERNEL__ */

typedef enum {
	LOCAL,
	VM,
	ENCLAVE,
} xpmem_endpoint_t;

struct xpmem_dom {
	/* Name-server routing info */
	struct {
		int id; 
		int fd; 
		xpmem_endpoint_t type;
	} ns; 

	/* Within enclave routing info */
	struct { 
		int id; 
		int fd; 
		xpmem_endpoint_t type;
	} enclave;
};

struct xpmem_cmd_make_ex {
	int64_t segid; /* Input/Output - nameserver must ensure uniqueness */
};

struct xpmem_cmd_remove_ex {
	int64_t segid;
};

struct xpmem_cmd_get_ex {
	int64_t  segid;
	uint32_t flags;
	uint32_t permit_type;
	uint64_t permit_value;
	int64_t  apid; /* Output */
};

struct xpmem_cmd_release_ex {
	int64_t apid;
};

struct xpmem_cmd_attach_ex {
	int64_t    apid;
	uint64_t   off;
	uint64_t   size;
	uint64_t   num_pfns;
	uint64_t * pfns;
};

struct xpmem_cmd_detach_ex {
	uint64_t vaddr;
};

typedef enum {
	XPMEM_MAKE,
	XPMEM_MAKE_COMPLETE,
	XPMEM_REMOVE,
	XPMEM_REMOVE_COMPLETE,
	XPMEM_GET,
	XPMEM_GET_COMPLETE,
	XPMEM_RELEASE,
	XPMEM_RELEASE_COMPLETE,
	XPMEM_ATTACH,
	XPMEM_ATTACH_COMPLETE,
	XPMEM_DETACH,
	XPMEM_DETACH_COMPLETE,
} xpmem_op_t;

struct xpmem_cmd_ex {
	struct xpmem_dom src_dom;
	struct xpmem_dom dst_dom;

	xpmem_op_t type;
	union {
		struct xpmem_cmd_make_ex    make;
		struct xpmem_cmd_remove_ex  remove;
		struct xpmem_cmd_get_ex     get;
		struct xpmem_cmd_release_ex release;
		struct xpmem_cmd_attach_ex  attach;
		struct xpmem_cmd_detach_ex  detach;
	};  
};


#ifdef __KERNEL__


struct xpmem_cmd_iter {
	struct xpmem_cmd_ex * cmd;
	struct list_head      node;
};

struct pisces_xpmem_state {
	int initialized;
	spinlock_t state_lock;

	/* Incoming lcall cmds */
	struct list_head in_cmds;
	spinlock_t       in_lock;

	/* User process waiting */
	waitq_t user_waitq;

	/* Structure to facilitate IPI-based communication */
	struct pisces_xbuf_desc * xbuf_desc;
};



/* 
 * Longcall structures 
 */
struct pisces_xpmem_cmd_lcall {
	union {
		struct pisces_lcall      lcall;
		struct pisces_lcall_resp lcall_resp;
	} __attribute__ ((packed));

	struct xpmem_cmd_ex xpmem_cmd;

	u8 pfn_list[0];
} __attribute__ ((packed));



/* 
 * XPMEM/ctrl command structures 
 */
struct pisces_xpmem_cmd_ctrl {
	struct xpmem_cmd_ex xpmem_cmd;
	u8 pfn_list[0];
} __attribute__ ((packed));



#endif /* __KERNEL__ */

#endif /* __PISCES_XPMEM_H__ */
