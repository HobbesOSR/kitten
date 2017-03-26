/*
 * Cross Partition Memory (XPMEM) attach support.
 */

#include <lwk/aspace.h>

#include <xpmem.h>
#include <xpmem_private.h>
#include <xpmem_extended.h>

/*
 * Attach a remote XPMEM address segment
 */
static int
xpmem_try_attach_remote(struct xpmem_thread_group * ap_tg,
			xpmem_segid_t		    segid, 
                        xpmem_apid_t		    apid,
                        off_t			    offset,
                        size_t			    size,
			vaddr_t			    vaddr,
			vmflags_t		    pgflags,
                        vaddr_t			  * at_vaddr_p,
			u64                      ** pfns)
{
    int           status   = 0;
    vaddr_t       at_vaddr = 0;
    vmpagesize_t  align    = 0;
    unsigned long nr_pages = 0;

    *at_vaddr_p = 0;

    /* Note: we want the physical address to be large-page aligned, even if the 
     * mapping uses 4KB pages, unless the user specifies a target address
     */
 
    if (vaddr)
	align = VM_PAGE_4KB;
    else
	align = VM_PAGE_2MB;

    /* Find free address space and add new region here */
    status = aspace_find_hole(ap_tg->aspace->id, vaddr, size, align, &at_vaddr);
    if (status != 0) {
	XPMEM_ERR("aspace_find_hole() failed (%d)", status);
	return status;
    }

    if ((vaddr) && (vaddr != at_vaddr)) {
	XPMEM_ERR("aspace_find_hole() did not return the vaddr requested (%p, requested %p)", 
	    (void *)at_vaddr, (void *)vaddr);
	BUG();
	return -EFAULT;
    }

    if (vaddr & (align - 1)) {
	XPMEM_ERR("aspace_find_hole() returned non-page-aligned address\n");
	BUG();
	return -EFAULT;
    }

    /* Add region to aspace */
    status = aspace_add_region(ap_tg->aspace->id, at_vaddr, size, pgflags, PAGE_SIZE, "xpmem");
    if (status != 0) {
	XPMEM_ERR("aspace_add_region() failed (%d)", status);
	return status;
    }

    /* Allocate buffer for pfn list */
    nr_pages = size / PAGE_SIZE;
    *pfns = kmem_alloc(sizeof(u64) * nr_pages);
    if (*pfns == NULL) {
	aspace_del_region(ap_tg->aspace->id, at_vaddr, size);
	return -ENOMEM;
    }

    /* Attach to remote memory */
    status = xpmem_attach_remote(xpmem_my_part->domain_link, segid, apid, offset, size, (u64)at_vaddr, *pfns);
    if (status != 0) {
	XPMEM_ERR("xpmem_attach_remote() failed (%d)", status);
	aspace_del_region(ap_tg->aspace->id, at_vaddr, size);
    } else {
	*at_vaddr_p = at_vaddr;
    }

    return status;
}


/*
 * Use SMARTMAP to figure out where the mapping is
 */
static int
xpmem_make_smartmap_addr(pid_t     source_pid,
                         vaddr_t   source_vaddr, 
			 vaddr_t * dest_vaddr)
{
    unsigned long slot;
    int status;

    status = aspace_get_rank(source_pid, (id_t *)&slot);
    if (status)
	return status;

    /*
    if (source_pid == INIT_ASPACE_ID)
	slot = 0;
    else
	slot = source_pid-2;
    */

    *dest_vaddr = (((slot + 1) << SMARTMAP_SHIFT) | ((unsigned long)source_vaddr));

    printk("%s: making smartmap addr in pid %d at %lx addr is: %lx\n",
	__func__, 
	source_pid, 
	source_vaddr, 
	*dest_vaddr);

    return 0;
}

/*
 * Attach a XPMEM address segment.
 */
int
xpmem_attach(xpmem_apid_t apid, 
             off_t        offset, 
	     size_t       size,
	     vaddr_t      vaddr,
	     int          att_flags, 
	     vaddr_t    * at_vaddr_p)
{
    int ret, index;
    vaddr_t seg_vaddr, at_vaddr = 0;
    vmflags_t pgflags;
    struct xpmem_thread_group *ap_tg, *seg_tg;
    struct xpmem_access_permit *ap;
    struct xpmem_segment *seg;
    struct xpmem_attachment *att;
    unsigned long flags, flags2;
    u64 * pfns = NULL;

    if (apid <= 0) {
	printk("EINVAL 1\n");
        return -EINVAL;
    }

    /* The start of the attachment must be page aligned */
    if (offset_in_page(vaddr) != 0 || offset_in_page(offset) != 0) {
	printk("EINVAL 3\n");
	return -EINVAL;
    }

    /* Only one flag at this point */
    if ((att_flags) &&
	(att_flags != XPMEM_NOCACHE_MODE)) {
	printk("EINVAL 4\n");
	return -EINVAL;
    }

    /* If the size is not page aligned, fix it */
    if (offset_in_page(size) != 0) 
        size += PAGE_SIZE - offset_in_page(size);

    ap_tg = xpmem_tg_ref_by_apid(apid);
    if (IS_ERR(ap_tg))
        return PTR_ERR(ap_tg);

    ap = xpmem_ap_ref_by_apid(ap_tg, apid);
    if (IS_ERR(ap)) {
        xpmem_tg_deref(ap_tg);
        return PTR_ERR(ap);
    }

    seg = ap->seg;
    xpmem_seg_ref(seg);
    seg_tg = seg->tg;
    xpmem_tg_ref(seg_tg);

    xpmem_seg_down(seg);

    ret = xpmem_validate_access(ap_tg, ap, offset, size, XPMEM_RDWR, &seg_vaddr);
    if (ret != 0)
        goto out_1;

    /* size needs to reflect page offset to start of segment */
    size += offset_in_page(seg_vaddr);

    /*
     * Ensure thread is not attempting to attach itw own memory on top of
     * itself (i.e. ensure the destination vaddr range doesn't overlap the
     * source vaddr range)
     *
     * BJK: only make check for local segs
     */
    if ( (vaddr != 0) &&
	!(seg->flags & XPMEM_FLAG_SHADOW) &&
	 (current->aspace->id == seg_tg->tgid)) {
	if ((vaddr + size > seg_vaddr) && (vaddr < seg_vaddr + size)) {
	    printk("EINVAL 5\n");
	    ret = -EINVAL;
	    goto out_1;
	}
    }

    pgflags = VM_READ | VM_WRITE | VM_USER;

    if (seg->flags & XPMEM_FLAG_SHADOW) {
        BUG_ON(seg->remote_apid <= 0);

	if (att_flags & XPMEM_NOCACHE_MODE)
	    pgflags |= VM_NOCACHE | VM_WRITETHRU;

	/* remote - load pfns in now */
	ret = xpmem_try_attach_remote(ap_tg, seg->segid, seg->remote_apid, 
		offset, size, vaddr, pgflags, &at_vaddr, &pfns);
	if (ret != 0)
            goto out_1;
    } else {
	/* not remote - simply figure out where we are smartmapped to this process */
	if (att_flags & XPMEM_NOCACHE_MODE) {
	    ret = -ENOMEM;
	    goto out_1;
	}

	ret = xpmem_make_smartmap_addr(seg_tg->aspace->id, seg_vaddr, &at_vaddr);
	if (ret)
	    goto out_1;
    }

    /* create new attach structure */
    att = kmem_alloc(sizeof(struct xpmem_attachment));
    if (att == NULL) {
        ret = -ENOMEM;
        goto out_1;
    }

    mutex_init(&att->mutex);
    att->vaddr    = seg_vaddr;
    att->at_vaddr = at_vaddr;
    att->at_size  = size;
    att->ap       = ap;
    att->flags    = 0;
    att->pfns     = pfns;
    INIT_LIST_HEAD(&att->att_node);

    xpmem_att_not_destroyable(att);
    xpmem_att_ref(att);

    /*
     * The attach point where we mapped the portion of the segment the
     * user was interested in is page aligned. But the start of the portion
     * of the segment may not be, so we adjust the address returned to the
     * user by that page offset difference so that what they see is what
     * they expected to see.
     */
    *at_vaddr_p = at_vaddr + offset_in_page(att->vaddr);

    /* link attach structure to its access permit's att list */
    spin_lock_irqsave(&ap->lock, flags);
    if (ap->flags & XPMEM_FLAG_DESTROYING) {
	spin_unlock_irqrestore(&ap->lock, flags);
	ret = -ENOENT;
	goto out_2;
    }
    list_add_tail(&att->att_node, &ap->att_list);

    /* add att to its ap_tg's hash list */
    index = xpmem_att_hashtable_index(att->at_vaddr);
    write_lock_irqsave(&ap_tg->att_hashtable[index].lock, flags2);
    list_add_tail(&att->att_hashnode, &ap_tg->att_hashtable[index].list);
    write_unlock_irqrestore(&ap_tg->att_hashtable[index].lock, flags2);

    spin_unlock_irqrestore(&ap->lock, flags);

    ret = 0;
out_2:
    if (ret != 0) {
	att->flags |= XPMEM_FLAG_DESTROYING;
        xpmem_att_destroyable(att);
    }
    xpmem_att_deref(att);
out_1:
    xpmem_seg_up(seg);
    xpmem_ap_deref(ap);
    xpmem_tg_deref(ap_tg);
    xpmem_seg_deref(seg);
    xpmem_tg_deref(seg_tg);
    return ret;
}

static inline u64
xpmem_shadow_vaddr_to_PFN(struct xpmem_attachment * att,
                          u64                       vaddr)
{
    //int pg_off = PAGE_ALIGN(vaddr - att->at_vaddr) >> PAGE_SHIFT;
    int pg_off = ((vaddr - att->at_vaddr) & PAGE_MASK) >> PAGE_SHIFT;
    return att->pfns[pg_off];
}

/* BJK: note that this function can be called with ap->tg->aspace already locked. This
 * happens if the task calls exit() with attachments still open, in which case the 
 * kernel locks the aspace and then closes all open files with the aspace locked.
 *
 * We can tell if this is the case by checking the XPMEM_FLAG_DESTROYING flag in the tg,
 * which gets set in xpmem_flush_tg() in xpmem_main.c
 *
 * If this is the case, we don't really need to teardown the mappings because that is going
 * to happen whenver aspace_destroy() is called
 */
static void
__xpmem_detach_att(struct xpmem_access_permit * ap, 
		   struct xpmem_attachment    * att)
{
    aspace_mapping_t mapping;
    int status, index;
    unsigned long flags;

    /* No address space to update on remote attachments - they are purely for bookkeeping */
    if (!(att->flags & XPMEM_FLAG_REMOTE)) {

	if (!(ap->tg->flags & XPMEM_FLAG_DESTROYING)) {
	    /* Lookup aspace mapping */
	    status = aspace_lookup_mapping(ap->tg->aspace->id, att->at_vaddr, &mapping);
	    if (status != 0) {
		XPMEM_ERR("aspace_lookup_mapping() failed (%d)", status);
		return;
	    }

	    /* On shadow attachments, we added a new aspace region. Remove it now */
	    if (ap->seg->flags & XPMEM_FLAG_SHADOW) {
		u64 pa;

		BUG_ON(mapping.flags & VM_SMARTMAP);

		/* Remove aspace mapping */
		status = aspace_del_region(ap->tg->aspace->id, mapping.start, mapping.end - mapping.start);
		if (status != 0) {
		    XPMEM_ERR("aspace_del_region() failed (%d)", status);
		    return;
		}

		/* Perform remote detachment */
		xpmem_detach_remote(xpmem_my_part->domain_link, ap->seg->segid, ap->seg->remote_apid, att->at_vaddr);

		/* Free from Palacios, if we're in a VM */
		pa = xpmem_shadow_vaddr_to_PFN(att, att->at_vaddr) << PAGE_SHIFT;
		BUG_ON(pa == 0);
		xpmem_detach_host_paddr(pa);

		kmem_free(att->pfns);
	    }
	    else {
		/* If this was a real attachment, it should be using SMARTMAP */
		BUG_ON(!(mapping.flags & VM_SMARTMAP));
	    }
	}

	/* Remove from att hash list - only done on local memory */
	index = xpmem_att_hashtable_index(att->at_vaddr);
	write_lock_irqsave(&ap->tg->att_hashtable[index].lock, flags);
	list_del(&att->att_hashnode);
	write_unlock_irqrestore(&ap->tg->att_hashtable[index].lock, flags);
    }

    /* Remove from ap list */
    spin_lock_irqsave(&ap->lock, flags);
    list_del_init(&att->att_node);
    spin_unlock_irqrestore(&ap->lock, flags);
}


/*
 * Detach an attached XPMEM address segment. This is functionally identical
 * to xpmem_detach(). It is called when ap and att are known.
 */
void
xpmem_detach_att(struct xpmem_access_permit * ap,
                 struct xpmem_attachment    * att)
{
    mutex_lock(&att->mutex);

    if (att->flags & XPMEM_FLAG_DESTROYING) {
	mutex_unlock(&att->mutex);
	return;
    }
    att->flags |= XPMEM_FLAG_DESTROYING;

    __xpmem_detach_att(ap, att);

    mutex_unlock(&att->mutex);

    xpmem_att_destroyable(att);
}

/*
 * Detach an attached XPMEM address segment.
 */
int
xpmem_detach(vaddr_t at_vaddr)
{
    struct xpmem_thread_group *tg;
    struct xpmem_access_permit *ap;
    struct xpmem_attachment *att;

    tg = xpmem_tg_ref_by_tgid(current->aspace->id);
    if (IS_ERR(tg))
	return PTR_ERR(tg);

    att = xpmem_att_ref_by_vaddr(tg, at_vaddr);
    if (IS_ERR(att)) {
	xpmem_tg_deref(tg);
	return PTR_ERR(att);
    }

    mutex_lock(&att->mutex);

    if (att->flags & XPMEM_FLAG_DESTROYING) {
	mutex_unlock(&att->mutex);
	xpmem_att_deref(att);
	xpmem_tg_deref(tg);
	return 0;
    }
    att->flags |= XPMEM_FLAG_DESTROYING;

    ap = att->ap;
    xpmem_ap_ref(ap);

    if (current->aspace->id != ap->tg->tgid) {
	att->flags &= ~XPMEM_FLAG_DESTROYING;
        xpmem_ap_deref(ap);
        mutex_unlock(&att->mutex);
        xpmem_att_deref(att);
        xpmem_tg_deref(tg);
        return -EACCES;
    }

    __xpmem_detach_att(ap, att);

    mutex_unlock(&att->mutex);

    xpmem_att_destroyable(att);

    xpmem_ap_deref(ap);
    xpmem_att_deref(att);
    xpmem_tg_deref(tg);

    return 0;
}
