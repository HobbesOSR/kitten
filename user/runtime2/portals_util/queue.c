#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <portals4.h>
#include "portals4_util.h"


/**
 * Describes a receive buffer.
 */
typedef struct {
	void *          start;
	ptl_me_t        me;
	ptl_handle_me_t me_h;
} rx_block_t;


/**
 * Private/internal queue type.
 */
typedef struct {
	ptl_handle_ni_t  ni_h;
	ptl_pt_index_t   pt_index;
	int              pt_is_disabled;
	int              nblocks;
	rx_block_t *     blocks;
} pq_t;


/**
 * Initializes a "many-to-one" queue.
 * This assumes a matching NI is passed in.
 */
int
ptl_queue_init(
	ptl_handle_ni_t ni_h,
	ptl_uid_t		uid,
	ptl_process_t   match_id,
	ptl_pt_index_t  pt_index,
	ptl_handle_eq_t eq_h,
	int             num_blocks,
	size_t          block_size,
	size_t			min_free,
	ptl_queue_t *   q)
{
	pq_t *pq;
	ptl_me_t me;
	int status, i;

	/* Allocate structure to track state of the queue */
	pq = MALLOC(sizeof(pq_t));
	pq->ni_h = ni_h;
	pq->pt_is_disabled = 0;

	/* Allocate the Portal table index */
	status = PtlPTAlloc(ni_h, PTL_PT_FLOWCTRL, eq_h, pt_index, &pq->pt_index);
	if (status != PTL_OK) {
		free(pq);
		return status;
	}

	/* Allocate an array of block descriptors */
	pq->nblocks = num_blocks;
	pq->blocks  = MALLOC(sizeof(rx_block_t) * pq->nblocks);

	/* Allocate memory for each block descriptor */
	for (i = 0; i < pq->nblocks; i++)
		pq->blocks[i].start = MALLOC(block_size);

	/* Create and attach an ME for each block */
	for (i = 0; i < pq->nblocks; i++) {
		rx_block_t * blk = &pq->blocks[i];
		ptl_me_t *   me  = &blk->me;

		me->start       = pq->blocks[i].start;
		me->length      = block_size;
		me->ct_handle   = PTL_CT_NONE;
		me->uid         = uid;
		me->options     = PTL_ME_OP_PUT |
		                  PTL_ME_MANAGE_LOCAL |
 		                  PTL_ME_MAY_ALIGN |
		                  PTL_ME_IS_ACCESSIBLE |
		                  PTL_ME_EVENT_LINK_DISABLE;
		me->match_id    = match_id;
		me->match_bits  = 0;
		me->ignore_bits = 0;
		me->min_free    = min_free;

		PTL_CHECK(
			PtlMEAppend(pq->ni_h, pq->pt_index, &blk->me,
			            PTL_PRIORITY_LIST, blk, &blk->me_h)
		);
	}

	/* Return an opaque pointer to the queue to the caller */
	*q = pq;
	return PTL_OK;
}


/**
 * If the portal is marked as disabled, reenable it.
 */
static void
enable_pt(pq_t *pq)
{
	if (pq->pt_is_disabled) {
		PTL_CHECK(PtlPTEnable(pq->ni_h, pq->pt_index));
		pq->pt_is_disabled = 0;
	}
}


/**
 * Processes an event, relinking unlinked buffers if necessary.
 * Returns:
 *    0 if successful and nothing for caller to do
 *    1 if caller should drain EQ and then call ptl_queue_is_drained()
 *    Anything else indicates an error
 */
int
ptl_queue_process_event(
	ptl_queue_t     q,
	ptl_event_t *   ev
)
{
	pq_t *pq = (pq_t *)q;
	rx_block_t *blk = ev->user_ptr;

	switch (ev->type) {
		case PTL_EVENT_PUT:
			/* Nothing to do */
			break;

		case PTL_EVENT_AUTO_UNLINK:
			PTL_CHECK(PtlMEAppend(pq->ni_h, pq->pt_index, &blk->me,
			                      PTL_PRIORITY_LIST, blk, &blk->me_h));
			enable_pt(pq);
			break;

		case PTL_EVENT_PT_DISABLED:
			pq->pt_is_disabled = 1;
			break;

		default:
			fprintf(stderr, "ERROR: Got unexpected %s.\n",
			        ptl_event_kind_str(ev->type));
			PTL_ABORT;
	}

	return pq->pt_is_disabled;
}


/**
 * If caller was told to drain the portals event queue associated
 * with the queue, they should call this function once they have
 * done so.  This covers the case where flow control occured due to
 * the portals event queue being full (rather than running out of
 * buffer space).
 */
void
ptl_queue_is_drained(ptl_queue_t q)
{
	enable_pt((pq_t *)q);
}


/**
 * Enqueues a message into a queue.
 * This blocks until the message is successfully delivered to the queue.
 */
int
ptl_enqueue(
	ptl_process_t   target_id,
	ptl_pt_index_t  pt_index,
	ptl_handle_md_t md_h,
	ptl_size_t      local_offset,
	ptl_size_t      length,
	ptl_handle_eq_t eq_h
)
{
	ptl_event_t ev;

	while (1) {
		/* Send the message, requesting an ACK so we know what happens to it */
		PTL_CHECK(
			PtlPut(md_h, local_offset, length, PTL_ACK_REQ,
			       target_id, pt_index, 0, 0, NULL, 0)
		);

		/* Get the PTL_EVENT_SEND */
		PTL_CHECK(PtlEQWait(eq_h, &ev));
		PTL_ASSERT(ev.type == PTL_EVENT_SEND);
		PTL_ASSERT(ev.ni_fail_type == PTL_NI_OK);

		/* Get the PTL_EVENT_ACK */
		PTL_CHECK(PtlEQWait(eq_h, &ev));
		PTL_ASSERT(ev.type == PTL_EVENT_ACK);

		/* Detect and recover from flow control occurrence */
		if (ev.ni_fail_type == PTL_NI_PT_DISABLED) {
			/* Give receiver some time to catch up then try again */
			sleep(1);
			continue;
		}

		/* Make sure our message wasn't truncated */
		PTL_ASSERT(ev.mlength == length);

		break;
	}

	return PTL_OK;
}


