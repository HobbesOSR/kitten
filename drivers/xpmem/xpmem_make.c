/*
 * Cross Partition Memory (XPMEM) make segment support.
 */

#include <lwk/kfs.h>
#include <arch-generic/fcntl.h>

#include <xpmem.h>
#include <xpmem_extended.h>
#include <xpmem_private.h>


/*
 * Create a new and unique segid.
 */
static xpmem_segid_t
xpmem_make_segid(struct xpmem_thread_group *seg_tg, xpmem_segid_t request)
{
    struct xpmem_id segid;
    xpmem_segid_t *segid_p = (xpmem_segid_t *)&segid;
    int ret;

    BUG_ON(sizeof(struct xpmem_id) != sizeof(xpmem_segid_t));

    *segid_p = 0;

    /* If there's no explicit request, the gid is encoded directly in the segid */
    if (request == 0) {
        segid.tgid = seg_tg->tgid;
    }

    /* Allocate a segid from the nameserver */
    ret = xpmem_make_remote(xpmem_my_part->domain_link, request, segid_p);
    if (ret != 0)
        return ret;

    return *segid_p;
}

int
xpmem_make_segment(vaddr_t                     vaddr,
                   size_t                      size, 
                   int                         permit_type,
                   void                      * permit_value,
                   int                         flags,
                   struct xpmem_thread_group * seg_tg,
                   xpmem_segid_t               segid,
                   xpmem_apid_t                remote_apid,
                   xpmem_domid_t               domid,
                   xpmem_sigid_t               sigid,
                   int                       * fd_p)
{
    struct xpmem_segment *seg;
    int status;
    unsigned long irq_flags;

    /* create a new struct xpmem_segment structure with a unique segid */
    seg = kmem_alloc(sizeof(struct xpmem_segment));
    if (seg == NULL)
        return -ENOMEM;

    spin_lock_init(&(seg->lock));
    mutex_init(&seg->mutex);
    seg->segid = segid;
    seg->remote_apid = remote_apid;
    seg->vaddr = vaddr;
    seg->size = size;
    seg->permit_type = permit_type;
    seg->permit_value = permit_value;
    seg->domid = domid;
    seg->tg = seg_tg;
    seg->flags = flags;
    INIT_LIST_HEAD(&seg->ap_list);
    INIT_LIST_HEAD(&seg->seg_node);
    atomic_set(&(seg->irq_count), 0);

    if (seg->flags & XPMEM_FLAG_SIGNALLABLE) {
        if (seg->flags & XPMEM_FLAG_SHADOW) {
            /* We are making a shadow segment that is signallable. We simply need
             * to record the sigid that will be used to IPI the segment
             */
            struct xpmem_signal * signal = (struct xpmem_signal *)&sigid;

            seg->sig.irq     = signal->irq;
            seg->sig.vector  = signal->vector;
            seg->sig.apic_id = signal->apic_id;
        } else {
            /* Else, create the signal structures locally */
            int fd;

            /* Allocate signal */
            status = xpmem_alloc_seg_signal(seg);
            if (status != 0) {
                kmem_free(seg);
                return status;
            }

            seg->domid = xpmem_get_domid();
            waitq_init(&seg->signalled_wq);

	    /* Create anonymous kfs file */
	    fd = kfs_open_anon(&xpmem_signal_fops, (void *)seg->segid);
	    if (fd < 0) {
		XPMEM_ERR("Unable to create anon fd for xpmem signalling");
		xpmem_free_seg_signal(seg);
		kmem_free(seg);
		return -ENFILE;
	    }

	    /* Save kfs ptr */
	    seg->kfs_file = get_current_file(fd);
	    seg->kfs_fd   = fd;

            *fd_p = fd;
        }
    }

    xpmem_seg_not_destroyable(seg);

    /* add seg to its tg's list of segs */
    write_lock_irqsave(&seg_tg->seg_list_lock, irq_flags);
    list_add_tail(&seg->seg_node, &seg_tg->seg_list);
    write_unlock_irqrestore(&seg_tg->seg_list_lock, irq_flags);

    /* add seg to global hash list of well-known segids, if necessary. Note
     * that shadow segments are not added to the wk list, because multiple 
     * shadow segments from different tgs can have the same segid
     */
    if ((segid <= XPMEM_MAX_WK_SEGID) &&
        (!(seg->flags & XPMEM_FLAG_SHADOW))) {
        write_lock_irqsave(&xpmem_my_part->wk_segid_to_tgid_lock, irq_flags);
        xpmem_my_part->wk_segid_to_tgid[segid] = seg_tg->tgid;
        write_unlock_irqrestore(&xpmem_my_part->wk_segid_to_tgid_lock, irq_flags);
    }

    return 0;
}

/*
 * Make a segid and segment for the specified address segment.
 */
int
xpmem_make(vaddr_t         vaddr, 
           size_t          size, 
	   int             permit_type, 
	   void          * permit_value, 
	   int             flags,
	   xpmem_segid_t   request, 
	   xpmem_segid_t * segid_p, 
	   int           * fd_p)
{
    xpmem_segid_t segid;
    xpmem_domid_t domid;
    struct xpmem_thread_group *seg_tg;
    int status, seg_flags = 0;

    /* BJK: sanity check permit mode and value */
    switch (permit_type) {
	case XPMEM_PERMIT_MODE:
	    if ((u64)permit_value & ~00777)
		return -EINVAL;
	
	    break;

	 case XPMEM_GLOBAL_MODE:
	     if (permit_value != NULL)
		 return -EINVAL;

	    break;

	 default:
	     return -EINVAL;
    }

    /* BJK: sanity check xpmem_make_hobbes() stuff.
       MEM_MODE: size must be greater than 0 
       SIG_MODE AND NOT MEM_MODE: size must be 0
       REQUEST_MODE: check segid in valid range
     */
    if (flags & XPMEM_MEM_MODE) {
        if (size <= 0) {
            return -EINVAL;
        }
    } else if (!(flags & XPMEM_SIG_MODE)) {
        return -EINVAL;
    } else if (size != 0) {
        return -EINVAL;
    }

    /* BJK: check if this is a well known segid request */
    if (flags & XPMEM_REQUEST_MODE) {
        if (request <= 0 || request > XPMEM_MAX_WK_SEGID) {
            return -EINVAL;
        }
    } else {
        request = 0;
    }

    /* Signallable segid */
    if (flags & XPMEM_SIG_MODE)
	seg_flags |= XPMEM_FLAG_SIGNALLABLE;

    seg_tg = xpmem_tg_ref_by_tgid(current->aspace->id);
    if (IS_ERR(seg_tg)) {
        BUG_ON(PTR_ERR(seg_tg) != -ENOENT);
        return -XPMEM_ERRNO_NOPROC;
    }

    /*
     * The start of the segment must be page aligned and it must be a
     * multiple of pages in size.
     */
    if (offset_in_page(vaddr) != 0 || offset_in_page(size) != 0) {
	xpmem_tg_deref(seg_tg);
	return -EINVAL;
    }

    segid = xpmem_make_segid(seg_tg, request);
    if (segid <= 0) {
        xpmem_tg_deref(seg_tg);
        return segid;
    }

    domid = xpmem_get_domid();
    BUG_ON(domid <= 0);

    status = xpmem_make_segment(vaddr, size, permit_type, permit_value, seg_flags, seg_tg, segid, 0, domid, 0, fd_p);
    if (status == 0)
        *segid_p = segid;

    xpmem_tg_deref(seg_tg);

    return status;
}

/*
 * Remove a segment from the system.
 */
int
xpmem_remove_seg(struct xpmem_thread_group * seg_tg, 
                 struct xpmem_segment      * seg)
{
    unsigned long flags;

    BUG_ON(atomic_read(&seg->refcnt) <= 0);

    /* see if the requesting thread is the segment's owner */
    if ( (current->aspace->id != seg_tg->tgid) &&
	!(seg->flags & XPMEM_FLAG_SHADOW))
        return -EACCES;

    spin_lock_irqsave(&seg->lock, flags);

    /* Cancel removal if
     * (1) it's already being removed
     * (2) the segment is a shadow segment and has more than 2 references
     *		(one at creation, plus the one about to be dropped by the
     *		invoker of this function)
     */
    if ( (seg->flags & XPMEM_FLAG_DESTROYING) ||
	   ((seg->flags & XPMEM_FLAG_SHADOW) &&
	    (atomic_read(&seg->refcnt) > 2)
	   )
	)
    {
	spin_unlock_irqrestore(&seg->lock, flags);
	return 0;
    }

    seg->flags |= XPMEM_FLAG_DESTROYING;
    spin_unlock_irqrestore(&seg->lock, flags);

    xpmem_seg_down(seg);

    if (seg->flags & XPMEM_FLAG_SHADOW) {
        /* Release the remote apid for shadow segments */
        BUG_ON(seg->remote_apid <= 0);
        xpmem_release_remote(xpmem_my_part->domain_link, seg->segid, seg->remote_apid);
    } else {
	/* Remove signal and free from name server if this is a real segment */
        if (xpmem_free_seg_signal(seg) == 0) {
	    kfs_close(seg->kfs_file);
	    fdTableInstallFd(current->fdTable, seg->kfs_fd, NULL);
	}

        xpmem_remove_remote(xpmem_my_part->domain_link, seg->segid);
    }

    /* Remove segment structure from its tg's list of segs */
    write_lock_irqsave(&seg_tg->seg_list_lock, flags);
    list_del_init(&seg->seg_node);
    write_unlock_irqrestore(&seg_tg->seg_list_lock, flags);

    /* Remove segment structure from global list of well-known segids */
    if (seg->segid <= XPMEM_MAX_WK_SEGID) {
        write_lock_irqsave(&xpmem_my_part->wk_segid_to_tgid_lock, flags);
        xpmem_my_part->wk_segid_to_tgid[seg->segid] = 0;
        write_unlock_irqrestore(&xpmem_my_part->wk_segid_to_tgid_lock, flags);
    }

    xpmem_seg_up(seg);
    xpmem_seg_destroyable(seg);

    return 0;
}

/*
 * Remove all segments belonging to the specified thread group.
 */
void
xpmem_remove_segs_of_tg(struct xpmem_thread_group *seg_tg)
{
    struct xpmem_segment *seg;
    unsigned long flags;

    BUG_ON(current->aspace->id != seg_tg->tgid);

    read_lock_irqsave(&seg_tg->seg_list_lock, flags);

    while (!list_empty(&seg_tg->seg_list)) {
        seg = list_entry((&seg_tg->seg_list)->next,
                 struct xpmem_segment, seg_node);
	if (!(seg->flags & XPMEM_FLAG_DESTROYING)) {

	    xpmem_seg_ref(seg);
	    read_unlock_irqrestore(&seg_tg->seg_list_lock, flags);

	    (void)xpmem_remove_seg(seg_tg, seg);

	    xpmem_seg_deref(seg);
	    read_lock_irqsave(&seg_tg->seg_list_lock, flags);
	}
    }
    read_unlock_irqrestore(&seg_tg->seg_list_lock, flags);
}

/*
 * Remove a segment from the system.
 */
int
xpmem_remove(xpmem_segid_t segid)
{
    struct xpmem_thread_group *seg_tg;
    struct xpmem_segment *seg;
    int ret;

    if (segid <= 0)
        return -EINVAL;

    seg_tg = xpmem_tg_ref_by_segid(segid);
    if (IS_ERR(seg_tg))
        return PTR_ERR(seg_tg);

    if (current->aspace->id != seg_tg->tgid) {
        xpmem_tg_deref(seg_tg);
        return -EACCES;
    }

    seg = xpmem_seg_ref_by_segid(seg_tg, segid);
    if (IS_ERR(seg)) {
        xpmem_tg_deref(seg_tg);
        return PTR_ERR(seg);
    }
    BUG_ON(seg->tg != seg_tg);

    ret = xpmem_remove_seg(seg_tg, seg);
    xpmem_seg_deref(seg);
    xpmem_tg_deref(seg_tg);

    return ret;
}
