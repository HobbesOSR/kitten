#ifndef _XPMEM_PRIVATE_H
#define _XPMEM_PRIVATE_H

#include <lwk/interrupt.h>
#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_iface.h>

#include <xpmem_partition.h>

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

/* found in xpmem_ns.c */
extern int xpmem_ns_init(struct xpmem_partition_state *);
extern int xpmem_ns_deinit(struct xpmem_partition_state *);
extern int xpmem_ns_deliver_cmd(struct xpmem_partition_state *, xpmem_link_t, struct xpmem_cmd_ex *);
extern void xpmem_ns_kill_domain(struct xpmem_partition_state *, xpmem_domid_t);

/* found in xpmem_fwd.c */
extern int xpmem_fwd_init(struct xpmem_partition_state *);
extern int xpmem_fwd_deinit(struct xpmem_partition_state *);
extern int xpmem_fwd_deliver_cmd(struct xpmem_partition_state *, xpmem_link_t, struct xpmem_cmd_ex *);

/* found in xpmem_irq.c */
extern int xpmem_request_irq(irqreturn_t (*)(int, void *), void *);
extern int xpmem_release_irq(int, void *);
extern int xpmem_local_irq_to_vector(int);
extern void xpmem_send_ipi_to_apic(unsigned int, unsigned int);

#endif
