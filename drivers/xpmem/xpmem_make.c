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
    if (request == 0)
        segid.gid = seg_tg->gid;

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

    /* create a new struct xpmem_segment structure with a unique segid */
    seg = kmem_alloc(sizeof(struct xpmem_segment));
    if (seg == NULL)
        return -ENOMEM;

    spin_lock_init(&(seg->lock));
    mutex_init(&seg->mutex);
    atomic_set(&(seg->irq_count), 0);
    seg->segid = segid;
    seg->remote_apid = remote_apid;
    seg->vaddr = vaddr;
    seg->size = size;
    seg->permit_type = permit_type;
    seg->permit_value = permit_value;
    seg->domid = domid;
    seg->tg = seg_tg;
    INIT_LIST_HEAD(&seg->ap_list);
    INIT_LIST_HEAD(&seg->seg_node);

    if (flags & XPMEM_FLAG_SHADOW)
        seg->flags = XPMEM_FLAG_SHADOW;

    if (flags & XPMEM_SIG_MODE) {
        if (flags & XPMEM_FLAG_SHADOW) {
            /* We are making a shadow segment that is signallable. We simply need
             * to record the sigid that will be used to IPI the segment
             */
            struct xpmem_signal * signal = (struct xpmem_signal *)&sigid;

            seg->sig.irq     = signal->irq;
            seg->sig.vector  = signal->vector;
            seg->sig.apic_id = signal->apic_id;

            seg->flags |= XPMEM_FLAG_SIGNALLABLE;
        } else {
            /* Else, create the signal structures locally */
	    struct inode * inodep = NULL;
            char name[16];
            int fd;

            /* Allocate signal */
            status = xpmem_alloc_seg_signal(seg);
            if (status != 0) {
                kmem_free(seg);
                return status;
            }

            seg->domid = xpmem_get_domid();
            waitq_init(&seg->signalled_wq);
            seg->flags |= XPMEM_FLAG_SIGNALLABLE;

            memset(name, 0, 16);
            snprintf(name, 16, XPMEM_DEV_PATH "%d", seg->sig.irq);

	    /* Create kfs file */
	    inodep = kfs_create(name, NULL, &xpmem_signal_fops, 0444, (void *)seg->segid, sizeof(xpmem_segid_t));
	    if (inodep == NULL) {
		XPMEM_ERR("Unable to create kfs file %s for xpmem signalling", name);
		xpmem_free_seg_signal(seg);
		kmem_free(seg);
		return -ENFILE;
	    }

	    /* Open now */
	    status = kfs_open_path(name, 0, O_RDONLY, &(seg->kfs_file));
	    if (status != 0) {
		XPMEM_ERR("Unable to open kfs file %s for xpmem signalling (%d)", name, status);
		kfs_destroy(inodep);
		xpmem_free_seg_signal(seg);
		kmem_free(seg);
		return status;
	    }

	    /* Get fd */
	    fd = fdTableGetUnused(current->fdTable);

	    if (fd < 0) {
		XPMEM_ERR("Unable to get fd for kfs file %s for xpmem signalling (%d)", name, fd);
		kfs_close(seg->kfs_file);
		kfs_destroy(inodep);
		xpmem_free_seg_signal(seg);
		kmem_free(seg);
		return fd;
	    }

	    /* Install fd */
	    fdTableInstallFd(current->fdTable, fd, seg->kfs_file);

            *fd_p = fd;
        }
    }

    xpmem_seg_not_destroyable(seg);

    /* add seg to its tg's list of segs */
    write_lock(&seg_tg->seg_list_lock);
    list_add_tail(&seg->seg_node, &seg_tg->seg_list);
    write_unlock(&seg_tg->seg_list_lock);

    /* add seg to global hash list of well-known segids, if necessary */
    if (segid <= XPMEM_MAX_WK_SEGID) {
        write_lock(&xpmem_my_part->wk_segid_to_gid_lock);
        xpmem_my_part->wk_segid_to_gid[segid] = seg_tg->gid;
        write_unlock(&xpmem_my_part->wk_segid_to_gid_lock);
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
    int status;

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

    seg_tg = xpmem_tg_ref_by_gid(current->aspace->id);
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

    status = xpmem_make_segment(vaddr, size, permit_type, permit_value, flags, seg_tg, segid, 0, domid, 0, fd_p);
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
    BUG_ON(atomic_read(&seg->refcnt) <= 0);

    /* see if the requesting thread is the segment's owner */
    if (current->aspace->id != seg_tg->gid)
        return -EACCES;

    spin_lock(&seg->lock);
    if (seg->flags & XPMEM_FLAG_DESTROYING) {
	spin_unlock(&seg->lock);
	return 0;
    }
    seg->flags |= XPMEM_FLAG_DESTROYING;
    spin_unlock(&seg->lock);

    xpmem_seg_down(seg);

    /* Remove signal and free from name server if this is a real segment */
    if (!(seg->flags & XPMEM_FLAG_SHADOW)) {
        xpmem_free_seg_signal(seg);
        xpmem_remove_remote(xpmem_my_part->domain_link, seg->segid);
    }

    /* Remove segment structure from its tg's list of segs */
    write_lock(&seg_tg->seg_list_lock);
    list_del_init(&seg->seg_node);
    write_unlock(&seg_tg->seg_list_lock);

    /* Remove segment structure from global list of well-known segids */
    if (seg->segid <= XPMEM_MAX_WK_SEGID) {
        write_lock(&xpmem_my_part->wk_segid_to_gid_lock);
        xpmem_my_part->wk_segid_to_gid[seg->segid] = 0;
        write_unlock(&xpmem_my_part->wk_segid_to_gid_lock);
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

    BUG_ON(current->aspace->id != seg_tg->gid);

    read_lock(&seg_tg->seg_list_lock);

    while (!list_empty(&seg_tg->seg_list)) {
        seg = list_entry((&seg_tg->seg_list)->next,
                 struct xpmem_segment, seg_node);
	if (!(seg->flags & XPMEM_FLAG_DESTROYING)) {

	    xpmem_seg_ref(seg);
	    read_unlock(&seg_tg->seg_list_lock);

	    (void)xpmem_remove_seg(seg_tg, seg);

	    xpmem_seg_deref(seg);
	    read_lock(&seg_tg->seg_list_lock);
	}
    }
    read_unlock(&seg_tg->seg_list_lock);
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

    if (current->aspace->id != seg_tg->gid) {
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
