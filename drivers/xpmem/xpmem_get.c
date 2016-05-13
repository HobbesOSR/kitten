/*
 * Cross Partition Memory (XPMEM) get access support.
 */

#include <xpmem.h>
#include <xpmem_private.h>
#include <xpmem_extended.h>

/*
 * Create a new and unique apid.
 */
xpmem_apid_t
xpmem_make_apid(struct xpmem_thread_group *ap_tg)
{
    struct xpmem_id apid;
    xpmem_apid_t *apid_p = (xpmem_apid_t *)&apid;
    int uniq;

    BUG_ON(sizeof(struct xpmem_id) != sizeof(xpmem_apid_t));

    uniq = atomic_inc_return(&ap_tg->uniq_apid);
    if (uniq > XPMEM_MAX_UNIQ_ID) {
        atomic_dec(&ap_tg->uniq_apid);
        return -EBUSY;
    }

    *apid_p = 0;
    apid.gid = ap_tg->gid;
    apid.uniq = (unsigned short)uniq;

    BUG_ON(*apid_p <= 0);
    return *apid_p;
}


static int
xpmem_try_get_remote(struct xpmem_thread_group * seg_tg,
                     xpmem_segid_t               segid,
                     int                         flags,
                     int                         permit_type,
                     void                      * permit_value)
{
    xpmem_apid_t  apid      = 0;
    xpmem_domid_t domid     = 0;
    xpmem_sigid_t sigid     = 0;
    size_t        size      = 0;
    int           status    = 0;
    int           seg_flags = 0;

    status = xpmem_get_remote(
        xpmem_my_part->domain_link,
        segid,
        flags,
        permit_type,
        (u64)permit_value,
        &apid,
        (u64 *)&size,
        &domid,
        &sigid);

    if (status != 0)
        return status;

    if (apid == -1)
        return -1;

    if (sigid != 0)
	seg_flags |= XPMEM_FLAG_SIGNALLABLE;
    else if (size == 0) {
	XPMEM_ERR("Creating shadow segment that is neither a memory segment nor signalable! This should be impossible");
        xpmem_release_remote(xpmem_my_part->domain_link, segid, apid);
	return -EINVAL;
    }

    seg_flags |= XPMEM_FLAG_SHADOW;

    /* We've been given a remote apid. The strategy is to fake like the segid
     * was created locally by creating a "shadow" segment ourselves
     */
    status = xpmem_make_segment(0, size, permit_type, permit_value, 
            seg_flags, seg_tg, segid, apid, domid, sigid, NULL);
    if (status != 0) {
	XPMEM_ERR("Unable to create shadow segment");
	xpmem_release_remote(xpmem_my_part->domain_link, segid, apid);
	return status;
    }

    /* Success */
    return 0;
}


int
xpmem_get_segment(int                         flags,
                  int                         permit_type,
                  void                      * permit_value,
                  xpmem_apid_t                apid,
                  struct xpmem_segment      * seg,
                  struct xpmem_thread_group * seg_tg,
                  struct xpmem_thread_group * ap_tg)
{
    struct xpmem_access_permit *ap;
    int index;

    /* create a new xpmem_access_permit structure with a unique apid */
    ap = kmem_alloc(sizeof(struct xpmem_access_permit));
    if (ap == NULL)
        return -ENOMEM;

    spin_lock_init(&(ap->lock));
    ap->seg = seg;
    ap->tg = ap_tg;
    ap->apid = apid;
    ap->mode = flags;
    INIT_LIST_HEAD(&ap->att_list);
    INIT_LIST_HEAD(&ap->ap_node);
    INIT_LIST_HEAD(&ap->ap_hashnode);

    xpmem_ap_not_destroyable(ap);

    /* add ap to its seg's access permit list */
    spin_lock(&seg->lock);
    list_add_tail(&ap->ap_node, &seg->ap_list);
    spin_unlock(&seg->lock);

    /* add ap to its hash list */
    index = xpmem_ap_hashtable_index(ap->apid);
    write_lock(&ap_tg->ap_hashtable[index].lock);
    list_add_tail(&ap->ap_hashnode, &ap_tg->ap_hashtable[index].list);
    write_unlock(&ap_tg->ap_hashtable[index].lock);

    return 0;
}

/*
 * Get permission to access a specified segid.
 */
int
xpmem_get(xpmem_segid_t segid, int flags, int permit_type, void *permit_value,
      xpmem_apid_t *apid_p)
{
    xpmem_apid_t apid       = 0;
    int          status     = 0;
    int          shadow_seg = 0;

    struct xpmem_segment *seg = NULL;
    struct xpmem_thread_group *ap_tg, *seg_tg;


    if (segid <= 0)
        return -EINVAL;

    if ((flags & ~(XPMEM_RDONLY | XPMEM_RDWR)) ||
        (flags & (XPMEM_RDONLY | XPMEM_RDWR)) ==
        (XPMEM_RDONLY | XPMEM_RDWR)){
      printk("%s: bad perms\n", __func__);
        return -EINVAL;
    }
    switch (permit_type) {
	case XPMEM_PERMIT_MODE:
	case XPMEM_GLOBAL_MODE:
          if (permit_value != NULL){

            printk("%s: bad permit val but we don't care\n", __func__);

            //return -EINVAL;
          }

	    break;
	default:
          printk("%s: I don't know this permit val\n", __func__);
	    return -EINVAL;
    }

    /* There are 2 cases that result in a remote lookup:
     * (1) The thread group encoded in the segment does not exist locally.
     *	
     * (2) The thread group exists locally, but the segment does not. The
     * ids for thread groups are not globally unique, so it's possible that
     * the same thread group id exists in two separate enclaves, but only
     * one will own the segment
     */

    seg_tg = xpmem_tg_ref_by_segid(segid);
    if (IS_ERR(seg_tg)) {
	seg_tg = xpmem_tg_ref_by_gid(current->aspace->id);
	if (IS_ERR(seg_tg))
	    return PTR_ERR(seg_tg);

	shadow_seg = 1;
    }

    if (!shadow_seg) {
	seg = xpmem_seg_ref_by_segid(seg_tg, segid);
	if (IS_ERR(seg))
	    shadow_seg = 1;
    }

    if (shadow_seg) {
        /* No local segment found. Look for a remote one */
	/* NOTE: in either case, the tg has already been ref'd. We ref the 
	 * current process' tg if no tg is found for the segid
	 */
        status = xpmem_try_get_remote(seg_tg, segid, flags, permit_type, 
		permit_value);
	if (status != 0) {
	    xpmem_tg_deref(seg_tg);
	    return status;
        }

	/* Now, get the shadow segment */
	seg = xpmem_seg_ref_by_segid(seg_tg, segid);
	if (IS_ERR(seg)) {
	    /* Error should be impossible here, but we'll
	     * check anyway. The shadow segment was created in
	     * xpmem_try_get_remote, so destroy it here */
	    xpmem_remove(segid);
	    xpmem_tg_deref(seg_tg);
	    return PTR_ERR(seg);
	}
    }

    /* find accessor's thread group structure */
    ap_tg = xpmem_tg_ref_by_gid(current->aspace->id);
    if (IS_ERR(ap_tg)) {
        BUG_ON(PTR_ERR(ap_tg) != -ENOENT);
	status = -XPMEM_ERRNO_NOPROC;
	goto err_ap_tg;
    }

    apid = xpmem_make_apid(ap_tg);
    if (apid < 0) {
	status = apid;
	goto err_apid;
    }

    status = xpmem_get_segment(flags, permit_type, permit_value, apid, 
	seg, seg_tg, ap_tg);

    if (status != 0)
	goto err_get;

    *apid_p = apid;
    xpmem_tg_deref(ap_tg);

    /*
     * The following two derefs
     *
     *      xpmem_seg_deref(seg);
     *      xpmem_tg_deref(seg_tg);
     *
     * aren't being done at this time in order to prevent the seg
     * and seg_tg structures from being prematurely kmem_free'd as long as the
     * potential for them to be referenced via this ap structure exists.
     *
     * These two derefs will be done by xpmem_release_ap() at the time
     * this ap structure is destroyed.
     */

    return status;

err_get:
err_apid:
    xpmem_tg_deref(ap_tg); 

err_ap_tg:
    /* If we created a shadow segment, destroy it on error. Else, just 
     * deref it
     */
    if (shadow_seg)
	xpmem_remove(segid);
    else
	xpmem_seg_deref(seg);
    xpmem_tg_deref(seg_tg);

    return status;
}

/*
 * Release an access permit and detach all associated attaches.
 */
void
xpmem_release_ap(struct xpmem_thread_group *ap_tg,
          struct xpmem_access_permit *ap)
{
    int index;
    struct xpmem_thread_group *seg_tg;
    struct xpmem_attachment *att;
    struct xpmem_segment *seg;

    spin_lock(&ap->lock);
    if (ap->flags & XPMEM_FLAG_DESTROYING) {
	spin_unlock(&ap->lock);
	return;
    }
    ap->flags |= XPMEM_FLAG_DESTROYING;

    /* deal with all attaches first */
    while (!list_empty(&ap->att_list)) {
        att = list_entry((&ap->att_list)->next, struct xpmem_attachment,
                 att_node);
        xpmem_att_ref(att);
        spin_unlock(&ap->lock);

        xpmem_detach_att(ap, att);

        xpmem_att_deref(att);
        spin_lock(&ap->lock);
    }

    spin_unlock(&ap->lock);

    /*
     * Remove access structure from its hash list.
     * This is done after the xpmem_detach_att to prevent any racing
     * thread from looking up access permits for the owning thread group
     * and not finding anything, assuming everything is clean, and
     * freeing the mm before xpmem_detach_att has a chance to
     * use it.
     */
    index = xpmem_ap_hashtable_index(ap->apid);
    write_lock(&ap_tg->ap_hashtable[index].lock);
    list_del_init(&ap->ap_hashnode);
    write_unlock(&ap_tg->ap_hashtable[index].lock);

    /* the ap's seg and the seg's tg were ref'd in xpmem_get() */
    seg = ap->seg;
    seg_tg = seg->tg;

    /* remove ap from its seg's access permit list */
    spin_lock(&seg->lock);
    list_del_init(&ap->ap_node);
    spin_unlock(&seg->lock);

    /* Try to teardown a shadow segment */
    if (seg->flags & XPMEM_FLAG_SHADOW)
	xpmem_remove_seg(seg_tg, seg);
    
    xpmem_seg_deref(seg);   /* deref of xpmem_get()'s ref */
    xpmem_tg_deref(seg_tg); /* deref of xpmem_get()'s ref */

    xpmem_ap_destroyable(ap);
}

/*
 * Release all access permits and detach all associated attaches for the given
 * thread group.
 */
void
xpmem_release_aps_of_tg(struct xpmem_thread_group *ap_tg)
{
    struct xpmem_hashlist *hashlist;
    struct xpmem_access_permit *ap;
    int index;

    for (index = 0; index < XPMEM_AP_HASHTABLE_SIZE; index++) {
        hashlist = &ap_tg->ap_hashtable[index];

        read_lock(&hashlist->lock);
        while (!list_empty(&hashlist->list)) {
            ap = list_entry((&hashlist->list)->next,
                    struct xpmem_access_permit,
                    ap_hashnode);
            xpmem_ap_ref(ap);
            read_unlock(&hashlist->lock);

            xpmem_release_ap(ap_tg, ap);

            xpmem_ap_deref(ap);
            read_lock(&hashlist->lock);
        }
        read_unlock(&hashlist->lock);
    }
}

/*
 * Release an access permit for a XPMEM address segment.
 */
int
xpmem_release(xpmem_apid_t apid)
{
    struct xpmem_thread_group *ap_tg;
    struct xpmem_access_permit *ap;

    if (apid <= 0)
        return -EINVAL;

    ap_tg = xpmem_tg_ref_by_apid(apid);
    if (IS_ERR(ap_tg))
        return PTR_ERR(ap_tg);

    if (current->aspace->id != ap_tg->gid) {
        xpmem_tg_deref(ap_tg);
        return -EACCES;
    }

    ap = xpmem_ap_ref_by_apid(ap_tg, apid);
    if (IS_ERR(ap)) {
        xpmem_tg_deref(ap_tg);
        return PTR_ERR(ap);
    }
    BUG_ON(ap->tg != ap_tg);

    xpmem_release_ap(ap_tg, ap);
    xpmem_ap_deref(ap);
    xpmem_tg_deref(ap_tg);

    return 0;
}
