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
xpmem_try_get_remote(xpmem_segid_t   segid,
                     int             flags,
                     int             permit_type,
                     void          * permit_value,
                     xpmem_apid_t  * remote_apid,
                     size_t        * remote_size,
                     xpmem_domid_t * remote_domid,
                     xpmem_sigid_t * remote_sigid)
{
    xpmem_apid_t  apid   = 0;
    xpmem_domid_t domid  = 0;
    xpmem_sigid_t sigid  = 0;
    size_t        size   = 0;
    int           status = 0;
    
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
        return -1;

    if (apid == -1)
        return -1;

    *remote_apid  = apid;
    *remote_size  = size;
    *remote_domid = domid;
    *remote_sigid = sigid;

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
    xpmem_apid_t  apid         = 0;
    xpmem_apid_t  remote_apid  = 0;
    xpmem_domid_t remote_domid = 0;
    xpmem_sigid_t remote_sigid = 0;
    size_t        remote_size  = 0;

    struct xpmem_segment *seg;
    struct xpmem_thread_group *ap_tg, *seg_tg;
    int status;

    if (segid <= 0)
        return -EINVAL;

    if ((flags & ~(XPMEM_RDONLY | XPMEM_RDWR)) ||
        (flags & (XPMEM_RDONLY | XPMEM_RDWR)) ==
        (XPMEM_RDONLY | XPMEM_RDWR))
        return -EINVAL;

    if (permit_type != XPMEM_PERMIT_MODE || permit_value != NULL) 
        return -EINVAL;

    seg_tg = xpmem_tg_ref_by_segid(segid);
    if (IS_ERR(seg_tg)) {
        int seg_flags = 0;

        /* No local segment found. Look for a remote one */
        if (xpmem_try_get_remote(segid, flags, permit_type, permit_value, 
                &remote_apid, &remote_size, &remote_domid, &remote_sigid) != 0) 
        {
            return PTR_ERR(seg_tg);
        }

        /* We've been given a remote apid. The strategy is to fake like the segid
         * was created locally by creating a "shadow" segment ourselves
         */
        if (remote_size > 0)
            seg_flags |= XPMEM_MEM_MODE;

        if (remote_sigid != 0)
            seg_flags |= XPMEM_SIG_MODE;

        if (seg_flags == 0) {
            XPMEM_ERR("Creating shadow segment that is neither a memory segment nor signalable!");
            return -EINVAL;
        }

        seg_flags |= XPMEM_FLAG_SHADOW;

        seg_tg = xpmem_tg_ref_by_gid(current->aspace->id);
        xpmem_make_segment(0, remote_size, permit_type, permit_value, 
            seg_flags, seg_tg, segid, remote_apid, remote_domid, remote_sigid, NULL);
    }

    seg = xpmem_seg_ref_by_segid(seg_tg, segid);
    if (IS_ERR(seg)) {
        xpmem_tg_deref(seg_tg);
        return PTR_ERR(seg);
    }

    /* find accessor's thread group structure */
    ap_tg = xpmem_tg_ref_by_gid(current->aspace->id);
    if (IS_ERR(ap_tg)) {
        BUG_ON(PTR_ERR(ap_tg) != -ENOENT);
        xpmem_seg_deref(seg);
        xpmem_tg_deref(seg_tg);
        return -XPMEM_ERRNO_NOPROC;
    }

    apid = xpmem_make_apid(ap_tg);
    if (apid < 0) {
        xpmem_tg_deref(ap_tg);
        xpmem_seg_deref(seg);
        xpmem_tg_deref(seg_tg);
        return apid;
    }
 
    status = xpmem_get_segment(flags, permit_type, permit_value, apid, seg, seg_tg, ap_tg);

    if (status == 0) {
        *apid_p = apid;
    } else {
        xpmem_seg_deref(seg);
        xpmem_tg_deref(seg_tg);
    }

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
    
    /* Release remote apid */
    if (seg->flags & XPMEM_FLAG_SHADOW) {
	BUG_ON(seg->remote_apid <= 0);
        xpmem_release_remote(xpmem_my_part->domain_link, seg->segid, seg->remote_apid);
    }

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
