#ifndef _XPMEM_EXTENDED_H
#define _XPMEM_EXTENDED_H

#include <lwk/types.h>
#include <lwk/waitq.h>
#include <xpmem.h>

extern u32 extend_enabled;
extern struct xpmem_extended_ops * xpmem_extended_ops;
extern struct xpmem_extended_ops pisces_ops;

struct xpmem_extended_ops {
    int (*make)(struct xpmem_partition *, xpmem_segid_t *);
    int (*remove)(struct xpmem_partition *, xpmem_segid_t);
    int (*get)(struct xpmem_partition *, xpmem_segid_t, int, int, u64, xpmem_apid_t *);
    int (*release)(struct xpmem_partition *, xpmem_apid_t);
    int (*attach)(struct xpmem_partition *, xpmem_apid_t, off_t, size_t, u64 *);
    int (*detach)(struct xpmem_partition *, u64);
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

typedef enum {
    LOCAL,
    VM,
    ENCLAVE,
} xpmem_endpoint_t;

struct xpmem_dom {
    /* Name server routing info */
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

struct xpmem_cmd_ex {
    struct xpmem_dom src_dom;
    struct xpmem_dom dst_dom;
    xpmem_op_t type;

    union {
        struct xpmem_cmd_make_ex make;
        struct xpmem_cmd_remove_ex remove;
        struct xpmem_cmd_get_ex get;
        struct xpmem_cmd_release_ex release;
        struct xpmem_cmd_attach_ex attach;
        struct xpmem_cmd_detach_ex detach;
    };
};

struct pisces_xpmem_state {
    int initialized;

    /* kfs inodes */
    struct inode * local_inodep;
    struct inode * remote_inodep;

    /* pending/in progress cmd */
    struct xpmem_cmd_ex cmd;
    int requested;
    int processed;
    int complete;

    /* issued remote cmd */
    struct xpmem_cmd_ex remote_cmd;
    int remote_requested;

    /* protect cmd access */
    spinlock_t lock;
 
    /* serialize client access to cmd/PCT */
    struct mutex mutex;

    /* waitq for clients */
    waitq_t client_wq;

    /* waitq for server/PCT */
    waitq_t pisces_wq;
};

int xpmem_get_remote(struct xpmem_cmd_get_ex * get_ex);
int xpmem_release_remote(struct xpmem_cmd_release_ex * release_ex);
int xpmem_attach_remote(struct xpmem_cmd_attach_ex * attach_ex);
int xpmem_detach_remote(struct xpmem_cmd_detach_ex * detach_ex);

unsigned long xpmem_map_pfn_range(u64 * pfns, u64 num_pfns);
void xpmem_detach_vaddr(u64 vaddr);

#endif /* _XPMEM_EXTENDED_H */
