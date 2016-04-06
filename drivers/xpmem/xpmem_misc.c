/*
 * Cross Partition Memory (XPMEM) miscellaneous functions.
 */

#include <xpmem.h>
#include <xpmem_private.h>

/*
 * xpmem_tg_ref() - see xpmem_private.h for inline definition
 */

/*
 * Return a pointer to the xpmem_thread_group structure that corresponds to the
 * specified gid. Increment the refcnt as well if found.
 */
struct xpmem_thread_group *
xpmem_tg_ref_by_gid(gid_t gid)
{
    int index;
    struct xpmem_thread_group *tg;

    index = xpmem_tg_hashtable_index(gid);
    read_lock(&xpmem_my_part->tg_hashtable[index].lock);

    list_for_each_entry(tg, &xpmem_my_part->tg_hashtable[index].list,
                                tg_hashnode) {
        if (tg->gid == gid) {
            if (tg->flags & XPMEM_FLAG_DESTROYING)
                continue;  /* could be others with this gid */

            xpmem_tg_ref(tg);
            read_unlock(&xpmem_my_part->tg_hashtable[index].lock);
            return tg; 
        }
    }   

    read_unlock(&xpmem_my_part->tg_hashtable[index].lock);
    return ERR_PTR(-ENOENT);
}

/*
 * Return a pointer to the xpmem_thread_group structure that corresponds to the
 * specified segid. Increment the refcnt as well if found.
 */
struct xpmem_thread_group *
xpmem_tg_ref_by_segid(xpmem_segid_t segid)
{
    return xpmem_tg_ref_by_gid(xpmem_segid_to_gid(segid));
}

/*
 * Return a pointer to the xpmem_thread_group structure that corresponds to the
 * specified apid. Increment the refcnt as well if found.
 */
struct xpmem_thread_group *
xpmem_tg_ref_by_apid(xpmem_apid_t apid)
{
    return xpmem_tg_ref_by_gid(xpmem_apid_to_gid(apid));
}

/*
 * Decrement the refcnt for a xpmem_thread_group structure previously
 * referenced via xpmem_tg_ref(), xpmem_tg_ref_by_gid(), or
 * xpmem_tg_ref_by_segid().
 */
void
xpmem_tg_deref(struct xpmem_thread_group *tg)
{
    BUG_ON(atomic_read(&tg->refcnt) <= 0);
    if (atomic_dec_return(&tg->refcnt) != 0)
        return;

    /*
     * Process has been removed from lookup lists and is no
     * longer being referenced, so it is safe to remove it.
     */
    BUG_ON(!(tg->flags & XPMEM_FLAG_DESTROYING));
    BUG_ON(!list_empty(&tg->seg_list));

    kmem_free(tg->ap_hashtable);
    kmem_free(tg->att_hashtable);
    kmem_free(tg);
}

/*
 * xpmem_seg_ref - see xpmem_private.h for inline definition
 */

/*
 * Return a pointer to the xpmem_segment structure that corresponds to the
 * given segid. Increment the refcnt as well.
 */
struct xpmem_segment *
xpmem_seg_ref_by_segid(struct xpmem_thread_group *seg_tg, xpmem_segid_t segid)
{
    struct xpmem_segment *seg;

    read_lock(&seg_tg->seg_list_lock);

    list_for_each_entry(seg, &seg_tg->seg_list, seg_node) {
        if (seg->segid == segid) {
            if (seg->flags & XPMEM_FLAG_DESTROYING)
                continue; /* could be others with this segid */

	    /* Shadow segments may only be lookeup up via segid by the process
	     * that created them. This is to force all xpmem_gets() of remote
	     * segids to issue independent get requests to the source domain
	     */
	    if ((seg->flags & XPMEM_FLAG_SHADOW) &&
		(current->aspace->id != seg_tg->gid))
		continue;

            xpmem_seg_ref(seg);
            read_unlock(&seg_tg->seg_list_lock);
            return seg;
        }
    }

    read_unlock(&seg_tg->seg_list_lock);
    return ERR_PTR(-ENOENT);
}

/*
 * Decrement the refcnt for a xpmem_segment structure previously referenced via
 * xpmem_seg_ref() or xpmem_seg_ref_by_segid().
 */
void
xpmem_seg_deref(struct xpmem_segment *seg)
{
    BUG_ON(atomic_read(&seg->refcnt) <= 0);
    if (atomic_dec_return(&seg->refcnt) == 0) {
	/*
	 * Segment has been removed from lookup lists and is no
	 * longer being referenced so it is safe to free it.
	 */
	BUG_ON(!(seg->flags & XPMEM_FLAG_DESTROYING));

	kmem_free(seg);
    }
}

/*
 * xpmem_ap_ref() - see xpmem_private.h for inline definition
 */

/*
 * Return a pointer to the xpmem_access_permit structure that corresponds to
 * the given apid. Increment the refcnt as well.
 */
struct xpmem_access_permit *
xpmem_ap_ref_by_apid(struct xpmem_thread_group *ap_tg, xpmem_apid_t apid)
{
    int index;
    struct xpmem_access_permit *ap;

    index = xpmem_ap_hashtable_index(apid);
    read_lock(&ap_tg->ap_hashtable[index].lock);

    list_for_each_entry(ap, &ap_tg->ap_hashtable[index].list,
                ap_hashnode) {
        if (ap->apid == apid) {
            if (ap->flags & XPMEM_FLAG_DESTROYING)
                break;  /* can't be others with this apid */

            xpmem_ap_ref(ap);
            read_unlock(&ap_tg->ap_hashtable[index].lock);
            return ap;
        }
    }

    read_unlock(&ap_tg->ap_hashtable[index].lock);

    return ERR_PTR(-ENOENT);
}

/*
 * Decrement the refcnt for a xpmem_access_permit structure previously
 * referenced via xpmem_ap_ref() or xpmem_ap_ref_by_apid().
 */
void
xpmem_ap_deref(struct xpmem_access_permit *ap)
{
    BUG_ON(atomic_read(&ap->refcnt) <= 0);
    if (atomic_dec_return(&ap->refcnt) == 0) {
        /*
         * Access has been removed from lookup lists and is no
         * longer being referenced so it is safe to remove it.
         */
        BUG_ON(!(ap->flags & XPMEM_FLAG_DESTROYING));
        kmem_free(ap);
    }
}

/*
 * xpmem_att_ref() - see xpmem_private.h for inline definition
 */

/*
 * Return a pointer to the xpmem_attachment structure that corresponds to the
 * given address. Increment the refcnt as well.
 */
struct xpmem_attachment *
xpmem_att_ref_by_vaddr(struct xpmem_thread_group * att_tg,
                       vaddr_t                     vaddr)
{
    int index;
    struct xpmem_attachment *att;

    index = xpmem_att_hashtable_index(vaddr);
    read_lock(&att_tg->att_hashtable[index].lock);

    list_for_each_entry(att, &att_tg->att_hashtable[index].list,
                att_hashnode) {
        if (att->at_vaddr == vaddr) {
            if (att->flags & XPMEM_FLAG_DESTROYING)
                break;  /* can't be others with this address */

            xpmem_att_ref(att);
            read_unlock(&att_tg->att_hashtable[index].lock);
            return att;
        }
    }

    read_unlock(&att_tg->att_hashtable[index].lock);

    return ERR_PTR(-ENOENT);

}


/*
 * Decrement the refcnt for a xpmem_attachment structure previously referenced
 * via xpmem_att_ref().
 */
void
xpmem_att_deref(struct xpmem_attachment *att)
{
    BUG_ON(atomic_read(&att->refcnt) <= 0);
    if (atomic_dec_return(&att->refcnt) == 0) {
        /*
         * Attach has been removed from lookup lists and is no
         * longer being referenced so it is safe to remove it.
         */
        BUG_ON(!(att->flags & XPMEM_FLAG_DESTROYING));
        kmem_free(att);
    }
}


/*
 * Ensure that a user is correctly accessing a segment for a copy or an attach.
 */
int
xpmem_validate_access(struct xpmem_thread_group * tg, struct xpmem_access_permit *ap, off_t offset,
              size_t size, int mode, vaddr_t *vaddr)
{
    /* ensure that this thread has permission to access segment */
    if (tg->gid != ap->tg->gid ||
        (mode == XPMEM_RDWR && ap->mode == XPMEM_RDONLY))
        return -EACCES;

    if (offset < 0 || size == 0 || offset + size > ap->seg->size)
        return -EINVAL;

    *vaddr = ap->seg->vaddr + offset;
    return 0;
}

gid_t
xpmem_segid_to_gid(xpmem_segid_t segid)
{
    gid_t gid;

    BUG_ON(segid <= 0);

    /* If the segid is greater than XPMEM_MAX_WK_SEGID, it is a regular segid that maps 
     * directly to a thread group. Else, it is a well-known segid 
     */
    if (segid > XPMEM_MAX_WK_SEGID) {
        gid = ((struct xpmem_id *)&segid)->gid;
    } else {
        read_lock(&xpmem_my_part->wk_segid_to_gid_lock);
        gid = xpmem_my_part->wk_segid_to_gid[segid];
        read_unlock(&xpmem_my_part->wk_segid_to_gid_lock);
    }

    return gid;
}

unsigned short
xpmem_segid_to_uniq(xpmem_segid_t segid)
{
    BUG_ON(segid <= 0);
    return ((struct xpmem_id *)&segid)->uniq;
}

gid_t
xpmem_apid_to_gid(xpmem_apid_t apid)
{
    BUG_ON(apid <= 0);
    return ((struct xpmem_id *)&apid)->gid;
}

unsigned short
xpmem_apid_to_uniq(xpmem_apid_t apid)
{
    BUG_ON(apid <= 0);
    return ((struct xpmem_id *)&apid)->uniq;
}
