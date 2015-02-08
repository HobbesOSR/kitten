#ifndef _XPMEM_PRIVATE_H
#define _XPMEM_PRIVATE_H

#include <lwk/interrupt.h>
#include <lwk/spinlock.h>

#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_extended.h>
#include <lwk/xpmem/xpmem_iface.h>


/*
 * Both the xpmem_segid_t and xpmem_apid_t are of type __s64 and designed
 * to be opaque to the user. Both consist of the same underlying fields.
 *
 * The 'uniq' field is designed to give each segid or apid a unique value.
 * Each type is only unique with respect to itself.
 *
 * An ID is never less than or equal to zero.
 */
struct xpmem_id {
    unsigned short uniq;  /* this value makes the ID unique */
    pid_t tgid;           /* thread group that owns ID */
};

#define XPMEM_MAX_UNIQ_ID   ((1 << (sizeof(short) * 8)) - 1)

/*
 * xpmem_sigid_t is of type __64 and designed to be opaque to the user. It consists of 
 * the following underlying fields
 *
 * 'apic_id' is the local apic id where the destination process is running
 * 'vector' is the ipi vector allocated by the destination process
 * 'irq' is the irq allocated by the destination process, which is only used if the
 * destination process is running in a local domain
 */
struct xpmem_signal {
    uint16_t apic_id;
    uint16_t vector;
    uint32_t irq;
};


static inline unsigned short
xpmem_segid_to_uniq(xpmem_segid_t segid)
{
    return ((struct xpmem_id *)&segid)->uniq;
}

static inline unsigned short
xpmem_apid_to_uniq(xpmem_segid_t apid)
{
    return ((struct xpmem_id *)&apid)->uniq;
}


#define XPMEM_ERR(format, args...) \
    printk(KERN_ERR "XPMEM_ERR: "format" [%s:%d]\n", ##args, __FILE__, __LINE__);

#define XPMEM_MAX_LINK  128
#define XPMEM_MIN_DOMID 32
#define XPMEM_MAX_DOMID 128

/* The well-known name server's domid */
#define XPMEM_NS_DOMID    1

struct xpmem_partition_state {
    /* spinlock for state */
    spinlock_t    lock;

    /* link to our own domain */
    xpmem_link_t  local_link;

    /* table mapping link ids to connection structs */
    struct xpmem_link_connection * conn_map[XPMEM_MAX_LINK];

    /* table mapping domids to link ids */
    xpmem_link_t                   link_map[XPMEM_MAX_DOMID];

    xpmem_link_t  uniq_link;

    /* are we running the nameserver */
    int is_nameserver;

    /* this partition's internal state */
    union {
        struct xpmem_ns_state  * ns_state;
        struct xpmem_fwd_state * fwd_state;
    };
};


struct xpmem_partition {
    struct xpmem_partition_state part_state;
    xpmem_link_t		 domain_link;
};


/* found in xpmem_main.c */
extern int do_xpmem_attach_domain(xpmem_apid_t apid, off_t offset,
	size_t size, u64 num_pfns, u64 pfn_pa);

/* found in xpmem_domain.c */
extern xpmem_link_t xpmem_domain_init(void);
extern int xpmem_domain_deinit(xpmem_link_t);

/* foind in xpmem_partition.c */
extern int xpmem_partition_init(int);
extern int xpmem_partition_deinit(void);
extern xpmem_domid_t xpmem_get_domid(void);
extern int xpmem_send_cmd_link(struct xpmem_partition_state *,
	xpmem_link_t, struct xpmem_cmd_ex *);
extern void xpmem_add_domid_link(struct xpmem_partition_state *,
	xpmem_domid_t, xpmem_link_t);
extern xpmem_link_t xpmem_get_domid_link(struct xpmem_partition_state *,
	xpmem_domid_t);
extern void xpmem_remove_domid_link(struct xpmem_partition_state *,
	xpmem_domid_t);
extern xpmem_link_t xpmem_add_local_connection(
		void *, 
		int (*)(struct xpmem_cmd_ex *, void *),
		int (*)(int, void *),
		void (*)(void *));

/* found in xpmem_ns.c */
extern int xpmem_ns_init(struct xpmem_partition_state *);
extern int xpmem_ns_deinit(struct xpmem_partition_state *);
extern int xpmem_ns_deliver_cmd(struct xpmem_partition_state *, xpmem_link_t, struct xpmem_cmd_ex *);
extern void xpmem_ns_kill_domain(struct xpmem_partition_state *, xpmem_domid_t);

/* found in xpmem_fwd.c */
extern int xpmem_fwd_init(struct xpmem_partition_state *);
extern int xpmem_fwd_deinit(struct xpmem_partition_state *);
extern int xpmem_fwd_deliver_cmd(struct xpmem_partition_state *, xpmem_link_t, struct xpmem_cmd_ex *);
extern xpmem_domid_t xpmem_fwd_get_domid(struct xpmem_partition_state *);

/* found in xpmem_irq.c */
extern int xpmem_request_irq(irqreturn_t (*)(int, void *), void *);
extern int xpmem_release_irq(int, void *);
extern int xpmem_local_irq_to_vector(int);
extern void xpmem_send_ipi_to_apic(unsigned int, unsigned int);

static inline char *
xpmem_cmd_to_string(xpmem_op_t op)
{
    switch (op) {
        case XPMEM_MAKE:
            return "XPMEM_MAKE";
        case XPMEM_REMOVE:
            return "XPMEM_REMOVE";
        case XPMEM_GET:
            return "XPMEM_GET";
        case XPMEM_RELEASE:
            return "XPMEM_RELEASE";
        case XPMEM_ATTACH:
            return "XPMEM_ATTACH";
        case XPMEM_DETACH:
            return "XPMEM_DETACH";
        case XPMEM_MAKE_COMPLETE:
            return "XPMEM_MAKE_COMPLETE";
        case XPMEM_REMOVE_COMPLETE:
            return "XPMEM_REMOVE_COMPLETE";
        case XPMEM_GET_COMPLETE:
            return "XPMEM_GET_COMPLETE";
        case XPMEM_RELEASE_COMPLETE:
            return "XPMEM_RELEASE_COMPLETE";
        case XPMEM_ATTACH_COMPLETE:
            return "XPMEM_ATTACH_COMPLETE";
        case XPMEM_DETACH_COMPLETE:
            return "XPMEM_DETACH_COMPLETE";
        case XPMEM_DOMID_REQUEST:
            return "XPMEM_DOMID_REQUEST";
        case XPMEM_DOMID_RESPONSE:
            return "XPMEM_DOMID_RESPONSE";
        case XPMEM_DOMID_RELEASE:
            return "XPMEM_DOMID_RELEASE";
        default:
            return "UNKNOWN";
    }
}

#endif
