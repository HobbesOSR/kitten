#ifndef _XPMEM_PRIVATE_H
#define _XPMEM_PRIVATE_H

#include <lwk/interrupt.h>
#include <lwk/list.h>
#include <lwk/spinlock_types.h>
#include <lwk/mutex.h>
#include <lwk/kfs.h>
#include <lwk/aspace.h>

#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_extended.h>
#include <lwk/xpmem/xpmem_iface.h>


#define PTR_ERR(ptr) (long)ptr
#define ERR_PTR(err) (void *)err

#define MAX_ERRNO       4095
#define IS_ERR_VALUE(x) ((x) >= (unsigned long)-MAX_ERRNO)
#define IS_ERR(ptr)     IS_ERR_VALUE((unsigned long)ptr)


#define XPMEM_CURRENT_VERSION        0x00030000
#define XPMEM_CURRENT_VERSION_STRING "3.0" 


#define XPMEM_ERR(format, args...) \
    printk(KERN_ERR "XPMEM_ERR: "format" [%s:%d]\n", ##args, __FILE__, __LINE__);


#define offset_in_page(p)       ((unsigned long)(p) & ~PAGE_MASK)


struct xpmem_thread_group {
    /* thread group ids */
    pid_t                   tgid;

    /* list of segments */
    struct list_head        seg_list;
    rwlock_t                seg_list_lock;

    /* hashtable of access permits */
    struct xpmem_hashlist * ap_hashtable;

    /* hashtable of attachments */
    struct xpmem_hashlist * att_hashtable;

    /* other misc */
    struct aspace         * aspace;
    atomic_t                uniq_apid;

    volatile int            flags;
    atomic_t                refcnt;

    /* synchronization */
    spinlock_t              lock;

    /* list embeddings */
    struct list_head        tg_hashnode;
};


struct xpmem_segment {
    /* list of access permits */
    struct list_head            ap_list;

    /* this segment's exported region */
    xpmem_segid_t               segid;
    vaddr_t                     vaddr;
    size_t                      size;
    int                         permit_type;
    void                      * permit_value;
    xpmem_domid_t               domid;
    xpmem_apid_t                remote_apid;

    /* optional signalling stuff */
    struct xpmem_signal         sig;
    atomic_t                    irq_count;
    waitq_t                     signalled_wq;
    struct file               * kfs_file;

    /* other misc */
    volatile int                flags;
    atomic_t                    refcnt;
    struct xpmem_thread_group * tg;

    /* synchronization */
    struct mutex                mutex;
    spinlock_t                  lock;

    /* list embeddings */
    struct list_head            seg_node;
};

struct xpmem_access_permit {
    /* list of attachments */
    struct list_head            att_list;

    /* this access permit's attached region */
    xpmem_apid_t                apid;
    int                         mode;

    struct xpmem_segment      * seg;
    struct xpmem_thread_group * tg;

    /* other misc */
    volatile int                flags;
    atomic_t                    refcnt;

    /* synchronization */
    spinlock_t                  lock;

    /* list embeddings */
    struct list_head            ap_node;
    struct list_head            ap_hashnode;
};

struct xpmem_attachment {
    /* the source attached region */
    vaddr_t                      vaddr;
    struct xpmem_access_permit * ap;

    /* the local thread's attached region */
    vaddr_t                      at_vaddr;
    size_t                       at_size;

    /* other misc */
    volatile int                 flags;
    atomic_t                     refcnt;

    /* synchronization */
    struct mutex                 mutex;

    /* list embeddings */
    struct list_head             att_node;
    struct list_head             att_hashnode;
};



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
    gid_t tgid;           /* thread group that owns ID */
};

#define XPMEM_MAX_UNIQ_ID   ((1 << (sizeof(short) * 8)) - 1)

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
    struct xpmem_hashlist      * tg_hashtable;

    /* wk segid->gid mapping */
    gid_t                        wk_segid_to_tgid[XPMEM_MAX_WK_SEGID + 1];
    rwlock_t                     wk_segid_to_tgid_lock;

    /* per-partition state */
    struct xpmem_partition_state part_state;

    /* local domain xpmem link */
    xpmem_link_t		 domain_link;

    /* pisces link */
    xpmem_link_t                 pisces_link;
};


/* found in xpmem_make.c */
extern int xpmem_make_segment(vaddr_t, size_t, int, void *, int, struct xpmem_thread_group *, xpmem_segid_t, xpmem_apid_t, xpmem_domid_t, xpmem_sigid_t, int *);
extern int xpmem_make(vaddr_t, size_t, int, void *, int, xpmem_segid_t,  xpmem_segid_t *, int *);
extern void xpmem_remove_segs_of_tg(struct xpmem_thread_group *);
extern int xpmem_remove(xpmem_segid_t);
extern int xpmem_remove_seg(struct xpmem_thread_group *, struct xpmem_segment *);

/* found in xpmem_get.c */
extern int xpmem_check_permit_mode(int, struct xpmem_segment *);
extern xpmem_apid_t xpmem_make_apid(struct xpmem_thread_group *);
extern int xpmem_get_segment(int, int, void *, xpmem_apid_t, struct xpmem_segment *, struct xpmem_thread_group *, struct xpmem_thread_group *);
extern int xpmem_get(xpmem_segid_t, int, int, void *, xpmem_apid_t *);
extern void xpmem_release_aps_of_tg(struct xpmem_thread_group *);
extern void xpmem_release_ap(struct xpmem_thread_group *, struct xpmem_access_permit *);
extern int xpmem_release(xpmem_apid_t);
extern int xpmem_signal(xpmem_apid_t);

/* found in xpmem_attach.c */
extern struct vm_operations_struct xpmem_vm_ops;
extern int xpmem_attach(xpmem_apid_t, off_t, size_t, int, vaddr_t *);
extern int xpmem_detach(vaddr_t);
extern void xpmem_detach_att(struct xpmem_access_permit *,
                 struct xpmem_attachment *);


/* found in xpmem_main.c */
extern struct xpmem_partition * xpmem_my_part;
extern int do_xpmem_attach_domain(xpmem_apid_t apid, off_t offset,
	size_t size, u64 num_pfns, paddr_t pfn_pa);

/* found in xpmem_misc.c */
extern struct xpmem_thread_group * xpmem_tg_ref_by_tgid(gid_t);
extern struct xpmem_thread_group * xpmem_tg_ref_by_segid(xpmem_segid_t);
extern struct xpmem_thread_group * xpmem_tg_ref_by_apid(xpmem_apid_t);
extern void xpmem_tg_deref(struct xpmem_thread_group *);
extern struct xpmem_segment * xpmem_seg_ref_by_segid(struct xpmem_thread_group *, xpmem_segid_t);
extern void xpmem_seg_deref(struct xpmem_segment *);
extern struct xpmem_access_permit * xpmem_ap_ref_by_apid(struct xpmem_thread_group *, xpmem_apid_t);
extern void xpmem_ap_deref(struct xpmem_access_permit *);
extern struct xpmem_attachment * xpmem_att_ref_by_vaddr(struct xpmem_thread_group *, vaddr_t);
extern void xpmem_att_deref(struct xpmem_attachment *);

extern gid_t xpmem_segid_to_tgid(xpmem_segid_t);
extern unsigned short xpmem_segid_to_uniq(xpmem_segid_t);
extern gid_t xpmem_apid_to_tgid(xpmem_apid_t);
extern unsigned short xpmem_apid_to_uniq(xpmem_apid_t);

extern int xpmem_validate_access(struct xpmem_thread_group *, struct xpmem_access_permit *, 
		off_t, size_t, int, vaddr_t *);

/* found in xpmem_domain.c */
extern xpmem_link_t xpmem_domain_init(void);
extern int xpmem_domain_deinit(xpmem_link_t);

extern int xpmem_make_remote(xpmem_link_t, xpmem_segid_t, xpmem_segid_t *); 
extern int xpmem_remove_remote(xpmem_link_t, xpmem_segid_t);
extern int xpmem_get_remote(xpmem_link_t, xpmem_segid_t, int, int, u64,
        xpmem_apid_t *, u64 *, xpmem_domid_t *, xpmem_sigid_t *); 
extern int xpmem_release_remote(xpmem_link_t, xpmem_segid_t, xpmem_apid_t);
extern int xpmem_attach_remote(xpmem_link_t, xpmem_segid_t, xpmem_apid_t,
        off_t, size_t, u64);
extern int xpmem_detach_remote(xpmem_link_t, xpmem_segid_t, xpmem_apid_t, u64);

/* foind in xpmem_partition.c */
extern int xpmem_partition_init(int);
extern int xpmem_partition_deinit(void);
extern xpmem_domid_t xpmem_get_domid(void);
extern int xpmem_ensure_valid_domid(void);
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
		int (*)(xpmem_segid_t, xpmem_sigid_t, xpmem_domid_t, void *),
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
extern xpmem_domid_t xpmem_fwd_get_domid(struct xpmem_partition_state *, xpmem_link_t);
extern int xpmem_fwd_ensure_valid_domid(struct xpmem_partition_state *);

/* found in xpmem_irq.c */
extern void xpmem_send_ipi_to_apic(unsigned int, unsigned int);
extern int xpmem_request_host_vector(int);
extern void xpmem_release_host_vector(int);
extern int xpmem_get_host_apic_id(int);

/* found in xpmem_signal.c */
extern int xpmem_alloc_seg_signal(struct xpmem_segment *); 
extern void xpmem_free_seg_signal(struct xpmem_segment *); 
extern void xpmem_signal_seg(struct xpmem_segment *); 
extern int xpmem_segid_signal(xpmem_segid_t);
extern int xpmem_signal(xpmem_apid_t);
extern struct kfs_fops xpmem_signal_fops;


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



/*
 * Attribute and state flags for various xpmem structures. Some values
 * are defined in xpmem.h, so we reserved space here via XPMEM_DONT_USE_X
 * to prevent overlap.
 */
#define XPMEM_FLAG_DESTROYING      0x00010 /* being destroyed */

/* Shadow segments are created by processes that attach to remote segids. Shadow
 * attachments are created when processes attach to shadow access segments. Memory mapped
 * for these attachments comes from remote domains 
 */
#define XPMEM_FLAG_SHADOW           0x00100 

/* Remote attachments are created when a remote process attaches to a local segment.
 * Memory mapped for these attachments comes from the local domain
 */
#define XPMEM_FLAG_REMOTE           0x00200 

#define XPMEM_FLAG_SIGNALLABLE      0x00400 /* Segment that is signallable */

#define XPMEM_DONT_USE_1        0x10000
#define XPMEM_DONT_USE_2        0x20000
#define XPMEM_DONT_USE_3        0x40000 /* reserved for xpmem.h */
#define XPMEM_DONT_USE_4        0x80000 /* reserved for xpmem.h */





/*
 * Inlines that mark an internal driver structure as being destroyable or not.
 * The idea is to set the refcnt to 1 at structure creation time and then
 * drop that reference at the time the structure is to be destroyed.
 */
static inline void
xpmem_tg_not_destroyable(struct xpmem_thread_group *tg)
{
    atomic_set(&tg->refcnt, 1);
}

static inline void
xpmem_tg_destroyable(struct xpmem_thread_group *tg)
{
    xpmem_tg_deref(tg);
}

static inline void
xpmem_seg_not_destroyable(struct xpmem_segment *seg)
{
    atomic_set(&seg->refcnt, 1);
}

static inline void
xpmem_seg_destroyable(struct xpmem_segment *seg)
{
    xpmem_seg_deref(seg);
}

static inline void
xpmem_ap_not_destroyable(struct xpmem_access_permit *ap)
{
    atomic_set(&ap->refcnt, 1);
}

static inline void
xpmem_ap_destroyable(struct xpmem_access_permit *ap)
{
    xpmem_ap_deref(ap);
}

static inline void
xpmem_att_not_destroyable(struct xpmem_attachment *att)
{
    atomic_set(&att->refcnt, 1);
}

static inline void
xpmem_att_destroyable(struct xpmem_attachment *att)
{
    xpmem_att_deref(att);
}

/*
 * Inlines that increment the refcnt for the specified structure.
 */
static inline void
xpmem_tg_ref(struct xpmem_thread_group *tg)
{
    BUG_ON(atomic_read(&tg->refcnt) <= 0);
    atomic_inc(&tg->refcnt);
}

static inline void
xpmem_seg_ref(struct xpmem_segment *seg)
{
    BUG_ON(atomic_read(&seg->refcnt) <= 0);
    atomic_inc(&seg->refcnt);
}

static inline void
xpmem_ap_ref(struct xpmem_access_permit *ap)
{
    BUG_ON(atomic_read(&ap->refcnt) <= 0);
    atomic_inc(&ap->refcnt);
}

static inline void
xpmem_att_ref(struct xpmem_attachment *att)
{
    BUG_ON(atomic_read(&att->refcnt) <= 0);
    atomic_inc(&att->refcnt);
}

static inline void
xpmem_seg_down(struct xpmem_segment * seg)
{
    mutex_lock(&seg->mutex);
}

static inline void
xpmem_seg_up(struct xpmem_segment * seg)
{
    mutex_unlock(&seg->mutex);
}



/*
 * Hash Tables
 *
 * XPMEM utilizes hash tables to enable faster lookups of list entries.
 * These hash tables are implemented as arrays. A simple modulus of the hash
 * key yields the appropriate array index. A hash table's array element (i.e.,
 * hash table bucket) consists of a hash list and the lock that protects it.
 *
 * XPMEM has the following two hash tables:
 *
 * table        bucket                  key
 * part->tg_hashtable   list of struct xpmem_thread_group   gid
 * tg->ap_hashtable list of struct xpmem_access_permit  apid.uniq
 */

struct xpmem_hashlist {
    rwlock_t lock;      /* lock for hash list */
    struct list_head list;  /* hash list */
} ____cacheline_aligned;

#define XPMEM_TG_HASHTABLE_SIZE  8
#define XPMEM_AP_HASHTABLE_SIZE  8
#define XPMEM_ATT_HASHTABLE_SIZE 8

static inline int 
xpmem_tg_hashtable_index(pid_t gid)
{
    return ((unsigned int)gid % XPMEM_TG_HASHTABLE_SIZE);
}

static inline int 
xpmem_ap_hashtable_index(xpmem_apid_t apid)
{
    BUG_ON(apid <= 0); 
    return xpmem_apid_to_uniq(apid) % XPMEM_AP_HASHTABLE_SIZE;
}

static inline int 
xpmem_att_hashtable_index(vaddr_t vaddr)
{
    int vpfn;

    /* vaddrs are usually going to be page aligned, so mod'ing by the hashtable size
     * (which evenly divides PAGE_SIZE) is a horrible idea
     *
     * Instead, let's mod the virtual page number by the hashtable size
     */
    vpfn = (vaddr & PAGE_MASK) >> PAGE_SHIFT;

    return ((unsigned int)vpfn % XPMEM_ATT_HASHTABLE_SIZE);
}

#endif
