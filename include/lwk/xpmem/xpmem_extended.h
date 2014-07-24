#ifndef _XPMEM_EXTENDED_H
#define _XPMEM_EXTENDED_H

#include <lwk/types.h>
#include <lwk/waitq.h>

#include <lwk/xpmem/xpmem.h>

typedef int64_t xpmem_domid_t;
typedef int64_t xpmem_link_t;

typedef enum {
    XPMEM_CONN_LOCAL,
    XPMEM_CONN_REMOTE,
} xpmem_connection_t;


struct xpmem_cmd_ex;

struct xpmem_link_connection {
    xpmem_connection_t   conn_type;
    void               * priv_data;
    int (*in_cmd_fn)(struct xpmem_cmd_ex * cmd, void * priv_data);
};


struct xpmem_cmd_make_ex {
    xpmem_segid_t segid;
};

struct xpmem_cmd_remove_ex {
    xpmem_segid_t segid;
};

struct xpmem_cmd_get_ex {
    xpmem_segid_t segid;
    uint32_t flags;
    uint32_t permit_type;
    uint64_t permit_value;
    xpmem_apid_t apid;
};

struct xpmem_cmd_release_ex {
    xpmem_apid_t apid;
};

struct xpmem_cmd_attach_ex {
    xpmem_apid_t apid;
    uint64_t off;
    uint64_t size;
    uint64_t num_pfns;
    uint64_t * pfns;
};

struct xpmem_cmd_detach_ex {
    uint64_t vaddr;
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

    /* Request a domid */
    XPMEM_DOMID_REQUEST,
    XPMEM_DOMID_RESPONSE,

} xpmem_op_t;


struct xpmem_cmd_ex {
    xpmem_domid_t src_dom;
    xpmem_domid_t dst_dom;
    xpmem_op_t type;

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


struct xpmem_partition_state;


/* Package XPMEM command into xpmem_cmd_ex structure and pass to forwarding/name
 * service layer
 */
int
xpmem_make_remote(struct xpmem_partition_state * part,
                  xpmem_segid_t                * segid);

int
xpmem_remove_remote(struct xpmem_partition_state * part,
                    xpmem_segid_t                  segid);

int
xpmem_get_remote(struct xpmem_partition_state * part,
                 xpmem_segid_t                  segid,
		 int                            flags,
		 int                            permit_type,
		 u64                            permit_value,
		 xpmem_apid_t                 * apid);

int
xpmem_release_remote(struct xpmem_partition_state * part,
                     xpmem_apid_t                   apid);

int
xpmem_attach_remote(struct xpmem_partition_state * part,
                    xpmem_apid_t                   apid,
		    off_t                          offset,
		    size_t                         size,
		    u64                          * vaddr);

int
xpmem_detach_remote(struct xpmem_partition_state * part,
                    u64                            vaddr);

#endif /* _XPMEM_EXTENDED_H */
