#ifndef _XPMEM_EXTENDED_H
#define _XPMEM_EXTENDED_H

#ifdef __KERNEL__ 

#include <lwk/types.h>
#include <lwk/waitq.h>

#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_iface.h>


struct xpmem_cmd_make_ex {
    xpmem_segid_t request;
    xpmem_segid_t segid;
};

struct xpmem_cmd_remove_ex {
    xpmem_segid_t segid;
};

struct xpmem_cmd_get_ex {
    xpmem_segid_t segid;
    uint32_t      flags;
    uint32_t      permit_type;
    uint64_t      permit_value;
    xpmem_apid_t  apid;
    uint64_t      size;
    xpmem_domid_t domid;
    xpmem_sigid_t sigid;
    uint32_t      seg_flags;
};

struct xpmem_cmd_release_ex {
    xpmem_segid_t segid; /* needed for routing */
    xpmem_apid_t  apid;
};

struct xpmem_cmd_attach_ex {
    xpmem_segid_t segid; /* needed for routing */
    xpmem_apid_t  apid;
    uint64_t      off;
    uint64_t      size;
    uint64_t      num_pfns;
    uint64_t      pfn_pa;
};

struct xpmem_cmd_detach_ex {
    xpmem_segid_t segid; /* needed for routing */
    xpmem_apid_t  apid;
    uint64_t      vaddr;
};

struct xpmem_cmd_domid_req_ex {
    xpmem_domid_t domid;
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

    /* Name service commands */
    XPMEM_PING_NS,
    XPMEM_PONG_NS,

    /* Request/Release a domid */
    XPMEM_DOMID_REQUEST,
    XPMEM_DOMID_RESPONSE,
    XPMEM_DOMID_RELEASE,

} xpmem_op_t;


struct xpmem_cmd_ex {
    uint32_t      reqid;   /* The local enclave identifier for the command */
    xpmem_domid_t req_dom; /* The domain invoking the original XPMEM operation */
    xpmem_domid_t src_dom; /* The domain that created the most recent request / response */
    xpmem_domid_t dst_dom; /* The domain targeted with the command / response */
    xpmem_op_t    type;

    union {
        struct xpmem_cmd_make_ex      make;
        struct xpmem_cmd_remove_ex    remove;
        struct xpmem_cmd_get_ex       get;
        struct xpmem_cmd_release_ex   release;
        struct xpmem_cmd_attach_ex    attach;
        struct xpmem_cmd_detach_ex    detach;
        struct xpmem_cmd_domid_req_ex domid_req;
    };
};

static inline char *
xpmem_op_to_str(xpmem_op_t op)
{
    switch (op) {
	case XPMEM_MAKE:		    return "XPMEM_MAKE";
	case XPMEM_REMOVE:		    return "XPMEM_REMOVE";
	case XPMEM_GET:			    return "XPMEM_GET";
	case XPMEM_RELEASE:		    return "XPMEM_RELEASE";
	case XPMEM_ATTACH:		    return "XPMEM_ATTACH";
	case XPMEM_DETACH:		    return "XPMEM_DETACH";
	case XPMEM_MAKE_COMPLETE:	    return "XPMEM_MAKE_COMPLETE";
	case XPMEM_REMOVE_COMPLETE:	    return "XPMEM_REMOVE_COMPLETE";
	case XPMEM_GET_COMPLETE:	    return "XPMEM_GET_COMPLETE";
	case XPMEM_RELEASE_COMPLETE:	    return "XPMEM_RELEASE_COMPLETE";
	case XPMEM_ATTACH_COMPLETE:	    return "XPMEM_ATTACH_COMPLETE";
	case XPMEM_DETACH_COMPLETE:	    return "XPMEM_DETACH_COMPLETE";
	case XPMEM_PING_NS:		    return "XPMEM_PING_NS";
	case XPMEM_PONG_NS:		    return "XPMEM_PONG_NS";
	case XPMEM_DOMID_REQUEST:	    return "XPMEM_DOMID_REQUEST";
	case XPMEM_DOMID_RESPONSE:	    return "XPMEM_DOMID_RESPONSE";
	case XPMEM_DOMID_RELEASE:	    return "XPMEM_DOMID_RELEASE";
	default:			    return "XPMEM_INVALID_OP";
    }
}

#endif /* __KERNEL__ */

#endif /* _XPMEM_EXTENDED_H */
