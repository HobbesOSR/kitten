/*
 * Cross Partition Memory (XPMEM) attach support.
 */

#include <lwk/aspace.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>

#include <xpmem.h>
#include <xpmem_private.h>
#include <xpmem_extended.h>


static inline u64
xpmem_shadow_vaddr_to_PFN(struct xpmem_attachment * att,
                          u64                       vaddr)
{
    int pgoff = ((vaddr - att->at_vaddr) & PAGE_MASK) >> PAGE_SHIFT;
    u64 idx;

    for (idx = 0; idx < att->pfn_range->nr_regions; idx++) {
	xpmem_pfn_region_t * rgn = &(att->pfn_range->pfn_list[idx]);

	if (pgoff < rgn->nr_pfns)
	    return rgn->first_pfn + pgoff;

	pgoff -= rgn->nr_pfns;
    }

    /* Impossible */
    BUG_ON(2+2 != 5);
    return 0;
}

static int
xpmem_map_pfn_region(struct xpmem_thread_group * ap_tg,
		     vaddr_t                     at_vaddr,
		     vmpagesize_t                page_size,
		     xpmem_pfn_region_t        * rgn)
{
    uint64_t pfn_off;
    vaddr_t  vaddr;
    paddr_t  paddr;
    int      status;

    vaddr   = at_vaddr;
    pfn_off = 0;

    while (pfn_off < rgn->nr_pfns) {
	paddr  = (rgn->first_pfn + pfn_off) << PAGE_SHIFT;

	status = aspace_map_pmem(
		ap_tg->aspace->id,
		paddr,
		vaddr,
		page_size
	    );

	if (status != 0) {
	    XPMEM_ERR("aspace_map_pmem() failed (%d)", status);
	    goto err;
	}

	vaddr   += page_size;
	pfn_off += page_size / VM_PAGE_4KB;
    }

    return 0;

err:
    {
	uint64_t j = 0;
	vaddr      = at_vaddr;
	while (j < pfn_off) {

	    status = aspace_unmap_pmem(
		    ap_tg->aspace->id,
		    vaddr,
		    page_size
		);

	    BUG_ON(status != 0);

	    vaddr += page_size;
	}
    }

    return status;
}

static void 
xpmem_unmap_pfn_region(struct xpmem_thread_group * ap_tg,
		       vaddr_t                     at_vaddr,
		       vmpagesize_t                page_size,
		       xpmem_pfn_region_t        * rgn)
{
    uint64_t pfn_off;
    vaddr_t  vaddr;
    int      status;

    vaddr   = at_vaddr;
    pfn_off = 0;
    
    while (pfn_off < rgn->nr_pfns) {
	status = aspace_unmap_pmem(
		ap_tg->aspace->id,
		vaddr,
		page_size
	    );

	BUG_ON(status != 0);

	vaddr += page_size;
    }
}

static int
xpmem_map_pfn_range(struct xpmem_thread_group * ap_tg,
		    vaddr_t                     at_vaddr,
		    vmpagesize_t                page_size,
		    xpmem_pfn_range_t         * range)
{
    xpmem_pfn_region_t * rgn;
    uint64_t             rgn_idx;
    uint64_t             rgn_len;
    vaddr_t              vaddr;
    int                  status;

    vaddr = at_vaddr;

    for (rgn_idx = 0; rgn_idx < range->nr_regions; rgn_idx++) {
	rgn     = &(range->pfn_list[rgn_idx]);
	rgn_len = rgn->nr_pfns * VM_PAGE_4KB;

	status = xpmem_map_pfn_region(ap_tg, vaddr, page_size, rgn);
	if (status != 0) {
	    XPMEM_ERR("xpmem_map_pfn_region() failed (%d)", status);
	    goto err;
	}

	vaddr += rgn_len;
    }

    return 0;

err:
    {
	uint64_t j;
	vaddr = at_vaddr;

	for (j = 0; j < rgn_idx; j++) {
	    rgn     = &(range->pfn_list[j]);
	    rgn_len = rgn->nr_pfns * VM_PAGE_4KB;

	    xpmem_unmap_pfn_region(ap_tg, vaddr, page_size, rgn);

	    vaddr += rgn_len;
	}
    }

    return status;
}

static vmpagesize_t
get_max_page_size(xpmem_pfn_range_t * range,
		  vaddr_t             target_vaddr)
{
    vmpagesize_t          page_size;
    xpmem_pfn_region_t  * rgn;
    uint64_t              rgn_idx;
    uint64_t              rgn_len;
    paddr_t               paddr;

    if (cpu_info[0].pagesz_mask & VM_PAGE_1GB)
	page_size = VM_PAGE_1GB;
    else
	page_size = VM_PAGE_2MB;

    for (rgn_idx = 0; rgn_idx < range->nr_regions; rgn_idx++) {
	rgn     = &(range->pfn_list[rgn_idx]);
	rgn_len = rgn->nr_pfns * VM_PAGE_4KB;
	paddr   = rgn->first_pfn << PAGE_SHIFT;

retry:
	if ( 
	    (      rgn_len & (page_size - 1)) ||
	    (      paddr   & (page_size - 1)) ||
	    (target_vaddr && 
	     (target_vaddr & (page_size - 1))
	    )
	   )
	{
	    /* page size too large: retry with next smallest.
	     * Note that any previous regions that were compatible
	     * with a larger page size will also be compatible with
	     * a smaller size
	     */
	    page_size /= 512;
	    if (page_size == VM_PAGE_4KB)
		break;

	    goto retry;
	}
    }

    return page_size;
}
		    

/* BJK:
 *
 * This now maps xpmem segments in with large pages as long as the
 * following conditions hold:
 *
 * (1) att->at_size is an even multiple of the large page size
 * (2) the xpmem_pfn_range_t structure we get from the source segment
 *     maps contiguous pfn regions which can all be mapped by large pages 
 *	(i.e., there are no regions that are not a multiple of large
 *		page size and would need some small pages)
 * (3) we have enough free aspace to find a large page-aligned region
 * (4) if the user requested a specific target vaddr, that vaddr is
 *     appropriately aligned
 */
static int
xpmem_map_shadow_pages(struct xpmem_thread_group * ap_tg,
		       struct xpmem_attachment   * att,
		       vaddr_t                     target_vaddr,
	               vaddr_t                   * at_vaddr_p)
{
    vaddr_t      at_vaddr;
    vmpagesize_t alignment;
    int          status;

    /* get largest page size compatible with this attachment and the virtual address */
    alignment = get_max_page_size(att->pfn_range, target_vaddr);

    /* Find free address space  */
    status = aspace_find_hole(ap_tg->aspace->id, target_vaddr, 
		att->at_size, alignment, &at_vaddr);
    if (status != 0) {
	XPMEM_ERR("aspace_find_hole() failed (%d)", status);
	return status;
    }

    if ((target_vaddr) && (target_vaddr != at_vaddr)) {
	XPMEM_ERR("aspace_find_hole() did not return the vaddr requested (%p, requested %p)", 
	    (void *)at_vaddr, (void *)target_vaddr);
	BUG();
	return -EFAULT;
    }

    if (at_vaddr & (alignment - 1)) {
	XPMEM_ERR("aspace_find_hole() returned non-page-aligned address\n");
	BUG();
    }

    /* Add region to aspace */
    /* TODO: use page flags in att->pfn_range */
    status = aspace_add_region(ap_tg->aspace->id, at_vaddr, att->at_size, 
		VM_READ | VM_WRITE | VM_USER, alignment, "xpmem");
    if (status != 0) {
	XPMEM_ERR("aspace_add_region() failed (%d)", status);
	return status;
    }

    /* Map each page in */
    status = xpmem_map_pfn_range(ap_tg, at_vaddr, alignment, 
		att->pfn_range);
    if (status != 0) {
	XPMEM_ERR("xpmem_map_pfn_range() failed (%d)", status);
	aspace_del_region(ap_tg->aspace->id, at_vaddr, att->at_size);
	return status;
    }

    *at_vaddr_p = at_vaddr;

    return 0;
}


static int
xpmem_unmap_shadow_pages(struct xpmem_thread_group * ap_tg,
		         struct xpmem_attachment   * att)
{
    return aspace_del_region(ap_tg->aspace->id, att->at_vaddr, 
	att->at_size);
}

static int
xpmem_attach_shadow_pages(struct xpmem_segment    * seg,
	  	          struct xpmem_attachment * att,
		          int                       offset)
{
    int ret;

    ret = xpmem_attach_remote(
	xpmem_my_part->domain_link, 
	seg->segid, 
	seg->remote_apid, 
	offset, 
	att->at_size, 
	&(att->pfn_range));

    return ret;
}

static int
xpmem_detach_shadow_pages(struct xpmem_segment    * seg,
			  struct xpmem_attachment * att)
{
    u64 pa;
    int ret;

    ret = xpmem_detach_remote(
	xpmem_my_part->domain_link, 
	seg->segid, 
	seg->remote_apid, 
	att->at_vaddr);

    pa = xpmem_shadow_vaddr_to_PFN(att, att->at_vaddr) << PAGE_SHIFT;
    BUG_ON(pa == 0);
    xpmem_detach_host_paddr(pa);

    return ret;
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
    struct xpmem_thread_group *ap_tg, *seg_tg;
    struct xpmem_access_permit *ap;
    struct xpmem_segment *seg;
    struct xpmem_attachment *att;
    unsigned long flags, flags2;

    if (apid <= 0) {
        return -EINVAL;
    }

    /* The start of the attachment must be page aligned */
    if (offset_in_page(vaddr) != 0 || offset_in_page(offset) != 0) {
	return -EINVAL;
    }

    /* Only one flag at this point */
    if ((att_flags) &&
	(att_flags != XPMEM_NOCACHE_MODE)) {
	return -EINVAL;
    }

    /* If the size is not page aligned, fix it */
    if (offset_in_page(size) != 0) 
        size += VM_PAGE_4KB - offset_in_page(size);

    /* Make sure size is not too larget for pfn lists */
    if (!(xpmem_attach_size_valid(size))) {
	XPMEM_ERR("Attach size (%li bytes) too large (max %llu bytes)\n",
	    size, (XPMEM_MAX_NR_PFNS << PAGE_SHIFT));
	return -EINVAL;
    }

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
	    ret = -EINVAL;
	    goto out_1;
	}
    }

    /* create new attach structure */
    att = kmem_alloc(sizeof(struct xpmem_attachment));
    if (att == NULL) {
        ret = -ENOMEM;
        goto out_1;
    }

    memset(att, 0, sizeof(struct xpmem_attachment));

    mutex_init(&att->mutex);
    att->vaddr    = seg_vaddr;
    att->at_size  = size;
    att->ap       = ap;
    att->flags    = 0;
    INIT_LIST_HEAD(&att->att_node);

    xpmem_att_not_destroyable(att);
    xpmem_att_ref(att);

    if (seg->flags & XPMEM_FLAG_SHADOW) {
        BUG_ON(seg->remote_apid <= 0);

	/* fetch pfns now */ 
	ret = xpmem_attach_shadow_pages(seg, att, offset);
	if (ret != 0) {
	    XPMEM_ERR("Failed to attach shadow pages");
	    goto out_2;
	}

	att->flags = XPMEM_FLAG_SHADOW;

	/* map them in */
	ret = xpmem_map_shadow_pages(ap_tg, att, vaddr, &at_vaddr); 
	if (ret != 0) {
	    XPMEM_ERR("Failed to map shadow pages");
            goto out_3;
	}
    } else {
	/* not remote - simply figure out where we are smartmapped to this process */
	if (att_flags & XPMEM_NOCACHE_MODE) {
	    ret = -ENOMEM;
	    goto out_2;
	}

	ret = xpmem_make_smartmap_addr(seg_tg->aspace->id, seg_vaddr, &at_vaddr);
	if (ret)
	    goto out_2;
    }

    att->at_vaddr = at_vaddr;

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
	goto out_4;
    }
    list_add_tail(&att->att_node, &ap->att_list);

    /* add att to its ap_tg's hash list */
    index = xpmem_att_hashtable_index(att->at_vaddr);
    write_lock_irqsave(&ap_tg->att_hashtable[index].lock, flags2);
    list_add_tail(&att->att_hashnode, &ap_tg->att_hashtable[index].list);
    write_unlock_irqrestore(&ap_tg->att_hashtable[index].lock, flags2);

    spin_unlock_irqrestore(&ap->lock, flags);

    ret = 0;

out_4:
    if (ret != 0) {
	if (seg->flags & XPMEM_FLAG_SHADOW) {
	    xpmem_unmap_shadow_pages(ap_tg, att);
	}
    }
out_3:
    if (ret != 0) {
	if (seg->flags & XPMEM_FLAG_SHADOW) {
	    xpmem_detach_shadow_pages(seg, att);
	}
    }
out_2:
    if (ret != 0) {
	att->flags |= XPMEM_FLAG_DESTROYING;
	/* TODO: munmap */
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
		BUG_ON(mapping.flags & VM_SMARTMAP);
		BUG_ON(mapping.end - mapping.start != att->at_size);

		/* Remove aspace mapping */
	        status = xpmem_unmap_shadow_pages(ap->tg, att);
		if (status != 0) {
		    XPMEM_ERR("xpmem_unmap_shadow_pages() failed (%d)", 
			status);
		}

                /* Detach from the source */
		status = xpmem_detach_shadow_pages(ap->seg, att);
		if (status != 0) {
		    XPMEM_ERR("xpmem_detach_shadow_pages() failed (%d)", 
			status);
		}
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
