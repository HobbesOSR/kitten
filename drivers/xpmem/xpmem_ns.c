/*
 * XPMEM extensions for multiple domain support.
 *
 * xpmem_ns.c: The XPMEM name service
 *
 * Author: Brian Kocoloski <briankoco@cs.pitt.edu>
 *
 */


#include <lwk/xpmem/xpmem.h>
#include <lwk/xpmem/xpmem_extended.h>
#include <lwk/xpmem/xpmem_iface.h>

#include <xpmem_partition.h>
#include <xpmem_hashtable.h>

#define MIN_UNIQ_SEGID    32
#define MIN_UNIQ_DOMID    32

struct xpmem_ns_state {
    /* lock for ns state */
    spinlock_t                     lock;
    /* Unique segid generation */

    atomic_t                       uniq_segid;

    /* Unique domid generation */
    atomic_t                       uniq_domid;

    /* map of XPMEM segids to XPMEM domids */
    struct xpmem_hashtable       * segid_map;
};


static int
alloc_xpmem_segid(struct xpmem_ns_state * ns_state, 
                  xpmem_segid_t         * segid)
{
    struct xpmem_id * id   = NULL;
    int32_t           uniq = 0;

    uniq = atomic_inc_return(&(ns_state->uniq_segid));

    if (uniq > XPMEM_MAX_UNIQ_SEGID) {
        return -1;
    }

    id       = (struct xpmem_id *)segid;
    id->uniq = (unsigned short)(uniq * XPMEM_MAX_UNIQ_APID);

    return 0;
}

static int
alloc_xpmem_domid(struct xpmem_ns_state * ns_state)
{
    return (xpmem_domid_t)atomic_inc_return(&(ns_state->uniq_domid));
}

/* Hashtable helpers */
static int
xpmem_add_segid(struct xpmem_ns_state * ns_state, 
                xpmem_segid_t           segid,
                xpmem_domid_t           domid)
{
    unsigned long flags  = 0;
    int           status = 0;

    spin_lock_irqsave(&(ns_state->lock), flags);
    {
        status = htable_insert(ns_state->segid_map,
                    (uintptr_t)segid,
                    (uintptr_t)domid);
    }
    spin_unlock_irqrestore(&(ns_state->lock), flags);

    return status;
}

static xpmem_domid_t
xpmem_search_or_remove_segid(struct xpmem_ns_state * ns_state,
                             xpmem_segid_t           segid,
                             int                     remove)
{
    unsigned long flags  = 0;
    xpmem_domid_t result = 0;

    spin_lock_irqsave(&(ns_state->lock), flags);
    {
        if (remove) {
            result = (xpmem_domid_t)htable_remove(ns_state->segid_map,
                        (uintptr_t)segid, 
                        0);
        } else {
            result = (xpmem_domid_t)htable_search(ns_state->segid_map,
                        (uintptr_t)segid); 
        }
    }
    spin_unlock_irqrestore(&(ns_state->lock), flags);

    return result;
}

static xpmem_domid_t
xpmem_search_segid(struct xpmem_ns_state * ns_state,
                   xpmem_segid_t           segid)
{
    return xpmem_search_or_remove_segid(ns_state, segid, 0);
}

static xpmem_domid_t
xpmem_remove_segid(struct xpmem_ns_state * ns_state,
                   xpmem_segid_t           segid)
{
    return xpmem_search_or_remove_segid(ns_state, segid, 1);
}


/* Process an XPMEM_PING/PONG_NS command */
static void
xpmem_ns_process_ping_cmd(struct xpmem_partition_state * part_state,
                          xpmem_link_t                   link,
                          struct xpmem_cmd_ex          * cmd)
{
    /* There's no reason not to reuse the input command struct for responses */
    struct xpmem_cmd_ex   * out_cmd  = cmd;
    xpmem_link_t            out_link = link;

    switch (cmd->type) {
        case XPMEM_PING_NS: {
            /* Respond with a PONG to the source */
            out_cmd->type = XPMEM_PONG_NS;

            break;
        }

        case XPMEM_PONG_NS: {
            /* We received a PONG. WTF */
            printk(KERN_ERR "XPMEM: name server received a PONG?"
                " Are there multiple name servers running?\n");

            return;
        }

        default: {
            printk(KERN_ERR "XPMEM: unknown PING operation: %s\n",
                cmd_to_string(cmd->type));
            return;
        }
    }

    /* Write the response */
    if (xpmem_send_cmd_link(part_state, out_link, out_cmd)) {
        printk(KERN_ERR "XPMEM: cannot send command on link %lli\n", link);
    }
}


/* Process an XPMEM_DOMID_REQUEST/RESPONSE command */
static void
xpmem_ns_process_domid_cmd(struct xpmem_partition_state * part_state,
                           xpmem_link_t                   link,
                           struct xpmem_cmd_ex          * cmd)
{
    struct xpmem_ns_state * ns_state = part_state->ns_state;

    /* There's no reason not to reuse the input command struct for responses */
    struct xpmem_cmd_ex   * out_cmd  = cmd;
    xpmem_link_t            out_link = link;

    switch (cmd->type) {
        case XPMEM_DOMID_REQUEST: {
            int ret = 0;

            /* A domid is requested by someone downstream from us on 'link' */
            out_cmd->domid_req.domid = alloc_xpmem_domid(ns_state);

            /* Update domid map */
            ret = xpmem_add_domid(part_state, out_cmd->domid_req.domid, link);

            if (ret == 0) {
                printk(KERN_ERR "XPMEM: cannot insert into domid hashtable\n");
                out_cmd->domid_req.domid = -1;
                goto out_domid_req;
            }

            out_domid_req:
            {
                out_cmd->type    = XPMEM_DOMID_RESPONSE;
                out_cmd->src_dom = part_state->domid;
            }

            break;
        }

        case XPMEM_DOMID_RESPONSE: {
            /* We've been allocated a domid. Interesting. */

            printk(KERN_ERR "XPMEM: name server has been allocated a domid?"
                " Are there multiple name servers running?\n");

            return;
        }
        default: {
            printk(KERN_ERR "XPMEM: unknown DOMID operation: %s\n",
                cmd_to_string(cmd->type));
            return;
        }
    }

    /* Write the response */
    if (xpmem_send_cmd_link(part_state, out_link, out_cmd)) {
        printk(KERN_ERR "XPMEM: cannot send command on link %lli\n", link);
    }
}



/* Process a regular XPMEM command. If we get here we are connected to the name
 * server already and have a domid
 */
static void
xpmem_ns_process_xpmem_cmd(struct xpmem_partition_state * part_state,
                           xpmem_link_t                   link,
                           struct xpmem_cmd_ex          * cmd)
{
    struct xpmem_ns_state * ns_state = part_state->ns_state;

    /* There's no reason not to reuse the input command struct for responses */
    struct xpmem_cmd_ex   * out_cmd  = cmd;
    xpmem_link_t            out_link = link;

    printk("XPMEM: received cmd %s on link %lli (src_dom = %lli, dst_dom = %lli)\n",
        cmd_to_string(cmd->type), link, cmd->src_dom, cmd->dst_dom);

    switch (cmd->type) {
        case XPMEM_MAKE: {
            int ret = 0;

            /* Allocate a unique segid to this domain */
            if (alloc_xpmem_segid(ns_state, &(cmd->make.segid))) {
                printk(KERN_ERR "XPMEM: cannot allocate segid. This is a problem\n");
                out_cmd->make.segid = -1;
                goto out_make;
            }

            /* Store in hashtable */
            ret = xpmem_add_segid(ns_state, cmd->make.segid, cmd->src_dom);

            if (ret == 0) {
                printk(KERN_ERR "XPMEM: cannot insert into segid hashtable\n");
                out_cmd->make.segid = -1;
                goto out_make;
            }

            out_make: 
            {
                out_cmd->type    = XPMEM_MAKE_COMPLETE;
                out_cmd->dst_dom = cmd->src_dom;
                out_cmd->src_dom = part_state->domid;
            }

            break;

        }

        case XPMEM_REMOVE: {
            xpmem_domid_t domid = 0;

            /* Remove segid from map */
            domid = xpmem_remove_segid(ns_state, cmd->remove.segid); 

            if (domid == 0) {
                printk(KERN_ERR "XPMEM: cannot remove segid %lli from hashtable\n",
                    cmd->remove.segid);
            }

            {
                out_cmd->type    = XPMEM_REMOVE_COMPLETE;
                out_cmd->dst_dom = cmd->src_dom;
                out_cmd->src_dom = part_state->domid;
            }

            break;

        }

        case XPMEM_GET: {
            xpmem_domid_t domid = 0;

            /* Search segid map */
            domid = xpmem_search_segid(ns_state, cmd->get.segid);

            if (domid == 0) {
                printk(KERN_ERR "XPMEM: cannot find segid %lli in hashtable\n",
                    cmd->get.segid);
                goto err_get;
            }

            /* Search domid map for link */
            out_link = xpmem_search_domid(part_state, domid);

            if (out_link == 0) {
                printk(KERN_ERR "XPMEM: cannot find domid %lli in hashtable."
                    " This should be impossible\n", domid);
                goto err_get;
            }

            out_cmd->dst_dom = domid;

            break;

            err_get:
            {
                out_cmd->get.apid = -1;
                out_cmd->type     = XPMEM_GET_COMPLETE;
                out_cmd->dst_dom  = cmd->src_dom;
                out_cmd->src_dom  = part_state->domid;
                out_link          = link;
            }

            break;
        }

        case XPMEM_RELEASE: {
            /* Extended apids are always allocated in the range [segid.uniq.
             * segid.uniq + XPMEM_MAX_UNIQ_APID), so we can simply reuse the
             * segid htable by flooring the apid uniq field. Yes, this is a hack
             */
            struct xpmem_id search_id;

            memcpy(&search_id, &(cmd->release.apid), sizeof(struct xpmem_id));
            search_id.uniq &= ~(XPMEM_MAX_UNIQ_APID - 1);

            {
                xpmem_domid_t domid = 0;

                /* Search segid map */
                domid = xpmem_search_segid(ns_state, *((xpmem_segid_t *)&search_id));

                if (domid == 0) {
                    printk(KERN_ERR "XPMEM: cannot find apid %lli in hashtable\n",
                        cmd->release.apid);
                    goto err_release;
                }

                /* Search domid map for link */
                out_link = xpmem_search_domid(part_state, domid);

                if (out_link == 0) {
                    printk(KERN_ERR "XPMEM: cannot find domid %lli in hashtable."
                        " This should be impossible\n", domid);
                    goto err_release;
                }

                out_cmd->dst_dom = domid;
            }

            break;

            err_release:
            {
                out_cmd->type    = XPMEM_RELEASE_COMPLETE;
                out_cmd->dst_dom = cmd->src_dom;
                out_cmd->src_dom = part_state->domid;
                out_link         = link;
            }

            break;
        }

        case XPMEM_ATTACH: {
            /* Extended apids are always allocated in the range [segid.uniq.
             * segid.uniq + XPMEM_MAX_UNIQ_APID), so we can simply reuse the
             * segid htable by flooring the apid uniq field. Yes, this is a hack
             */
            struct xpmem_id search_id;

            memcpy(&search_id, &(cmd->attach.apid), sizeof(struct xpmem_id));
            search_id.uniq &= ~(XPMEM_MAX_UNIQ_APID - 1);

            {
                xpmem_domid_t domid = 0;

                /* Search segid map */
                domid = xpmem_search_segid(ns_state, *((xpmem_segid_t *)&search_id));

                if (domid == 0) {
                    printk(KERN_ERR "XPMEM: cannot find apid %lli in hashtable\n",
                        cmd->attach.apid);
                    goto err_attach;
                }

                /* Search domid map for link */
                out_link = xpmem_search_domid(part_state, domid);

                if (out_link == 0) {
                    printk(KERN_ERR "XPMEM: cannot find domid %lli in hashtable."
                        " This should be impossible\n", domid);
                    goto err_attach;
                }

                out_cmd->dst_dom = domid;
            }

            break;

            err_attach:
            {
                out_cmd->type    = XPMEM_ATTACH_COMPLETE;
                out_cmd->dst_dom = cmd->src_dom;
                out_cmd->src_dom = part_state->domid;
                out_link         = link;
            }

            break;
        }

        case XPMEM_DETACH: {
            /* Ignore detaches for now, because it's not clear how to figure out
             * a destination domain from just a virtual address
             */
            {
                out_cmd->type    = XPMEM_DETACH_COMPLETE;
                out_cmd->dst_dom = cmd->src_dom;
                out_cmd->src_dom = part_state->domid;
                out_link         = link;
            }

            break;
        }

        case XPMEM_GET_COMPLETE: 
        case XPMEM_RELEASE_COMPLETE:
        case XPMEM_ATTACH_COMPLETE:
        case XPMEM_DETACH_COMPLETE: {
            /* The destination is now the original source */
            cmd->dst_dom = cmd->src_dom;

            /* The nameserver becomes the source for this command */
            cmd->src_dom = part_state->domid;

            /* Search for the appropriate link */
            out_link = xpmem_search_domid(part_state, cmd->dst_dom);

            if (out_link == 0) {
                printk(KERN_ERR "XPMEM: cannot find domid %lli in hashtable."
                    " This should be impossible\n", cmd->dst_dom);
                return;
            }

            break; 
        }


        default: {
            printk(KERN_ERR "XPMEM: unknown operation: %s\n",
                cmd_to_string(cmd->type));
            return;
        }
    }


    /* Write the response */
    if (xpmem_send_cmd_link(part_state, out_link, out_cmd)) {
        printk(KERN_ERR "XPMEM: cannot send command on link %lli\n", link);
    }
}



int
xpmem_ns_deliver_cmd(struct xpmem_partition_state * part_state,
                      xpmem_link_t                   link,
                      struct xpmem_cmd_ex          * cmd)
{
    switch (cmd->type) {
        case XPMEM_PING_NS:
        case XPMEM_PONG_NS:
            xpmem_ns_process_ping_cmd(part_state, link, cmd);
            break;

        case XPMEM_DOMID_REQUEST:
        case XPMEM_DOMID_RESPONSE:
            xpmem_ns_process_domid_cmd(part_state, link, cmd);
            break;

        default:
            xpmem_ns_process_xpmem_cmd(part_state, link, cmd);
            break;
    }   

    return 0;
}


int
xpmem_ns_init(struct xpmem_partition_state * part_state)
{
    struct xpmem_ns_state * ns_state = kmem_alloc(sizeof(struct xpmem_ns_state));
    if (!ns_state) {
        return -1;
    }

    /* Create segid map */
    ns_state->segid_map = create_htable(0, xpmem_hash_fn, xpmem_eq_fn);
    if (!ns_state->segid_map) {
        return -1;
    }

    /* Create everything else */
    spin_lock_init(&(ns_state->lock));
    atomic_set(&(ns_state->uniq_segid), MIN_UNIQ_SEGID);
    atomic_set(&(ns_state->uniq_domid), MIN_UNIQ_DOMID);

    /* Name server partition has a well-known domid */
    part_state->domid = XPMEM_NS_DOMID;

    part_state->ns_state = ns_state;

    printk("XPMEM name service initialized\n");

    return 0;
}

int
xpmem_ns_deinit(struct xpmem_partition_state * part_state)
{
    struct xpmem_ns_state * ns_state = part_state->ns_state;

    if (!ns_state) {
        return 0;
    }

    /* Free segid map */
    free_htable(ns_state->segid_map, 0, 0);

    kmem_free(ns_state);
    part_state->ns_state = NULL;

    printk("XPMEM name service deinitialized\n");

    return 0;
}
