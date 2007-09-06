/* -*- mode: c; c-basic-offset: 8; -*-
 */
/*
 *   RCA: Interface between CRAY RCA and linux kernel
 *
 * Copyright (c) 2003 Cray Inc.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *
 */
/*
 * This code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */


#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <rca/rca_l0_linux.h>
#include <rca/rca_l0.h>
extern void	set_debug_traps(void) ;		/* GDB routine */

#if defined(CONFIG_CRAY_RS_DEBUG)
#define assert(expr) \
if(!(expr)) {                                   \
	printk( "Assertion failed! %s,%s,%s,line=%d\n", \
	       #expr,__FILE__,__FUNCTION__,__LINE__);          \
}
#else
#define assert(expr)
#endif /* CONFIG_CRAY_RS_DEBUG */

/* States for up channel. Used for calling the tx_done callback only if reqd */
#define	CHAN_TX_STATE_AVAIL	(0)
#define	CHAN_TX_STATE_FULL	(1)
#define GET_CHAN_STATE(x)	((x)->state)
#define SET_CHAN_STATE(x, s)	((x)->state = (s))

typedef struct l0rca_mapped_ch {
	uint32_t num_obj;        /* number of objects    */
	uint32_t intr_bit;
	volatile uint32_t *ch_ridx;             /* Pointer to ridx */
	volatile uint32_t *ch_widx;             /* Pointer to widx */
	rs_event_t		*ch_buf_ptr;	/* Pointer to channel buffer */
	l0rca_down_handle_t	down_handler;
	l0rca_up_handle_t	up_handler;
	uint32_t		reg_count;
	int 			poll;		/* timeout */
	int 			tshhld;		/* timeout */
	int 			state;		/* outgoing chan full or not */
} l0rca_mapped_ch_t;

#define L0RCA_INITVAL   (0xAA55F00D)

typedef struct l0rca_mapped_config {
	uint32_t	version;	/* config version */
	rs_node_t	proc_id;	/* node id */
	int32_t		proc_num;	/* cpu number (0-3) */
	l0rca_mapped_ch_t	ch_data[NUM_L0RCA_CHANNELS];
	volatile uint32_t	*l0rca_l0_intr_ptr;	/* interrupt to L0 */
        uint32_t        initialized;
} l0rca_mapped_config_t;

/* Our copy with virt addrs; so we can directly access the config area */
l0rca_mapped_config_t l0rca_early_cfg = {0};

/* Pointer to the actual config struct in Seastar RAM shared memory */
l0rca_config_t *l0rca_cfg = NULL;

/* Store the size of the event header without the msg body */
uint32_t rs_ev_hdr_sz;


void send_intr_to_l0(l0rca_mapped_ch_t *ch)
{
	volatile uint32_t *addr = &(((l0rca_intr_t *)(l0rca_early_cfg.l0rca_l0_intr_ptr))->l0r_intr_set);
	SSPUT32(addr, (ch)->intr_bit);
}

/* 
 * Function:	l0rca_event_data
 * 
 * Description: Return a pointer to the data portion and length of the
 * 		data portion of the event.
 * 		NB: Reflect any changes to l0rca_event_data in gdb_l0rca_event_data
 * 
 * Arguments: rs_event_t *evp IN: Event whose data is of interest
 * 	      void **data OUT: Upon return will point to data portion of event
 * 	      int32_t *len OUT: Upon return will have the length of the data
 *	                        portion of the event
 *
 * Returns: No Return Value.
 * Note: The main purpose of this routine is to read in the data portion of the
 * event from Seastar memory. 
 */
void l0rca_event_data(rs_event_t *evp, void **data, int32_t *len)
{
        /* Get length and data portion from the event */
        *len = evp->ev_len;
        *data = &evp->ev_data;
} /* l0rca_event_data */

/* 
 * Function:	l0rca_get_proc_id
 * 
 * Description: Return the node/processor id.
 * 
 * Arguments: None
 *
 * Returns: The proc id.
 */
rs_node_t l0rca_get_proc_id(void)
{
	return l0rca_cfg->proc_id;
}

/* 
 * Function:	l0rca_get_proc_num
 * 
 * Description: Returns this processor's CPU number, 0-3.
 * 
 * Arguments: None.
 * 
 * Returns: The CPU number of this node.
 */
int l0rca_get_proc_num(void)
{
    int tmp;
    SSGET32(&(l0rca_cfg->proc_num),tmp);
    return(tmp);
}

/*
 * Function:	set_chan_tx_state
 *
 * Description:	Looks at the read and write indices to determine if the
 *		channel is full or not. Sets state accordingly.
 *		NB: Reflect any changes to set_chan_tx_state in gdb_set_chan_tx_state.
 *
 * Arguments: l0rca_mapped_ch_t *ch_ptr IN: Pointer to channel being set.
 *
 * Returns: None
 */
static inline void set_chan_tx_state(l0rca_mapped_ch_t *ch_ptr)
{
	volatile uint32_t wx, rx;
	SSGET32(ch_ptr->ch_widx, wx);
	SSGET32(ch_ptr->ch_ridx, rx);
	int not_full = (wx - rx) < ch_ptr->num_obj;
	SET_CHAN_STATE(ch_ptr, not_full  ? CHAN_TX_STATE_AVAIL : CHAN_TX_STATE_FULL);
}


/*
 * Function:	l0rca_init_config
 *
 * Description: Read L0 - RCA communication config structure and populate 
 *              our personal copy. If there is any error, the OS panics
 *              since not being able to communicate with L0 is a total disaster.
 *              If already initialized then returns silently.
 *
 * Arguments: None.
 *
 * Returns: None
 */

void l0rca_init_config(void)
{
	rs_event_t *ev_buf;
	int i;
	volatile uint64_t tmp64;
	volatile uint32_t tmp32;

	/* 
	 * Check if already initialized; No locking is needed as this 
	 * is called during the boot process from within the kernel (as 
	 * opposed from a driver)
	 */
	if (L0RCA_INITVAL == l0rca_early_cfg.initialized)
		return;

	l0rca_cfg = (l0rca_config_t *)rca_l0_comm_va(L0_SIC_RAM);

	/* TODO - first order of business is to check the Version Number */
	/* Also, should we panic if version mismatch? */
#ifdef CONFIG_CRAY_RS_DEBUG
	printk ("l0 config at virtual %p\n", l0rca_cfg);
	printk ("Phys event bufs 0x%llx intr reg 0x%llx\n",
		l0rca_cfg->l0rca_buf_addr,
		l0rca_cfg->l0rca_l0_intr_addr);
#endif /* CONFIG_CRAY_RS_DEBUG */

	/* convert event buffer address from physical to virtual */
	SSGET64(&(l0rca_cfg->l0rca_buf_addr),tmp64);
	ev_buf = (rs_event_t *)rca_l0_comm_va(tmp64);

	/* convert intr reg address from physical to virtual */
	SSGET64(&(l0rca_cfg->l0rca_l0_intr_addr), tmp64);
	l0rca_early_cfg.l0rca_l0_intr_ptr = (uint32_t *)rca_l0_comm_va(tmp64);

#ifdef CONFIG_CRAY_RS_DEBUG
        printk ("event bufs %p intr reg %p\n", ev_buf,
                l0rca_early_cfg.l0rca_l0_intr_ptr);
#endif /* CONFIG_CRAY_RS_DEBUG */

	/* Now setup the channel buffers */
	for (i = 0; i < NUM_L0RCA_CHANNELS; i++)
	{
		int num_obj; 
		SSGET32(&(l0rca_cfg->chnl_data[i].num_obj), tmp32);
		num_obj = tmp32;

		/* Skip if channel is unused */
		if (!num_obj)
			continue;

		/* Ensure num_obj is a power of 2 */
		if (num_obj & (num_obj - 1))
		{
			panic ("l0rca_init_config: num_obj[%u] for channel %d is not power of 2\n",
			       num_obj, i);
		}
		l0rca_early_cfg.ch_data[i].num_obj = num_obj;
		SSGET32(&(l0rca_cfg->chnl_data[i].l0_intr_bit), tmp32);
		l0rca_early_cfg.ch_data[i].intr_bit = tmp32;

		/* Point to the read/writew index */
		l0rca_early_cfg.ch_data[i].ch_ridx = &l0rca_cfg->chnl_data[i].ridx;
		l0rca_early_cfg.ch_data[i].ch_widx = &l0rca_cfg->chnl_data[i].widx;
		l0rca_early_cfg.ch_data[i].ch_buf_ptr = ev_buf;

		ev_buf += num_obj;

#ifdef CONFIG_CRAY_RS_DEBUG
		printk ("Buffer %p for channel %d rd %u wr %u\n", 
			l0rca_early_cfg.ch_data[i].ch_buf_ptr, i,
			l0rca_cfg->chnl_data[i].ridx,
			l0rca_cfg->chnl_data[i].widx);
#endif /* CONFIG_CRAY_RS_DEBUG */
	}	/* End of for */

	/* Set the remaining fields */
	SSGET32(&(l0rca_cfg->version), l0rca_early_cfg.version);
	SSGET32(&(l0rca_cfg->proc_id), l0rca_early_cfg.proc_id);
	SSGET32(&(l0rca_cfg->proc_num), l0rca_early_cfg.proc_num);

	/* Assumes ev_data is the last element TODO: this should be
         * defined via a macro in the rs_event_t structure definition */
	rs_ev_hdr_sz = offsetof(rs_event_t, ev_data);

	/* Indicate we have a initialized the mapped copy */
	l0rca_early_cfg.initialized = L0RCA_INITVAL;

        return;
}

/* 
 * Function:	register_ch_up
 * 
 * Description: Register function for the upload channel. It is expected that
 * there be at most one registered user for an upload channel. This user 
 * provides a callback to be invoked when the buffer drains below tshhld
 * (only if the buffer became full last time the tshhld was crossed)
 * 
 * Arguments: int ch_num IN: channel number to register on
 * 	      l0rca_up_handle_t handler IN: callback routine
 *            int tshhld IN: buffer to drain before invoking callback; ignored
 *			     if poll is negative.
 * 	      int poll IN: if > zero - duration in ms to check for buffer drain
 *			   if = zero - tx done interrupt invokes callback (TODO)
 *			   if < zero - do nothing. It is assumed that the user (TODO)
 * 			   has her own means to check for buffer drain
 *
 * Returns: -EBUSY - If another user is already registered.
 *          -EINVAL - if ch_num is not in range.
 *          zero (SUCCESS) otherwise.
 *
 * Note: It is expected that the buffer is empty upon call to this routine. 
 *	 This should be true on system startup and after an unregister call.
 *
 * Note: As of 12/1/04, the polling is forced, and is hard coded in 
 *   l0rca_linux.c.  Thus, the tshhld and poll params have no meaning.
 */
int
register_ch_up(int ch_num, l0rca_up_handle_t handler, int tshhld, int poll)
{
        volatile uint32_t wx, rx;
	l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[ch_num];

	if (NUM_L0RCA_CHANNELS <= ch_num)
		return -EINVAL;

	/* Allow only one user per channel */
	if (ch_ptr->reg_count)
		return -EBUSY;

        SSGET32(ch_ptr->ch_widx, wx);
        SSGET32(ch_ptr->ch_ridx, rx);
	assert(wx == rx);

	SET_CHAN_STATE(ch_ptr, CHAN_TX_STATE_AVAIL);
	ch_ptr->down_handler = NULL;	/* Clear the down handler */
	ch_ptr->reg_count++;
	ch_ptr->poll = poll;
	ch_ptr->up_handler = handler;
	ch_ptr->tshhld = tshhld;

	return 0;
}

/* 
 * Function:	register_ch_down
 * 
 * Description: Register function for the download channel. It is expected that
 * there be at most one registered user for a download channel. This user 
 * provides a callback to be invoked when data from L0 arrives on the channel.
 * 
 * Arguments: int ch_num IN: channel number to register on
 * 	      l0rca_down_handle_t handler IN: callback routine
 * 	      int poll IN: if > zero - duration in ms to check for event
 *			   if = zero - event arrival is interrupt driven.
 *			   if < zero - do nothing. It is assumed that the user
 * 			   has her own means to check for event arrival.
 *
 * Returns: EBUSY - If another user is already registered.
 *          zero (SUCCESS) otherwise.
 * 
 * Note: As of 12/1/04, the polling is forced, and is hard coded in 
 *   l0rca_linux.c.  Thus, the poll parameter has no meaning.
 */
int register_ch_down(int ch_num, l0rca_down_handle_t handler, int poll)
{
	l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[ch_num];

	if (NUM_L0RCA_CHANNELS <= ch_num)
		return -EINVAL;

	/* Allow only one user per channel */
	if (ch_ptr->reg_count)
		return -EBUSY;

	ch_ptr->reg_count++;
	ch_ptr->down_handler = handler;
	ch_ptr->poll = poll;

	/* Do any OS specific initialization e.g. set irq, timers etc. */
	if (l0rca_os_init())
	{
		panic ("Unable to initialize OS service for L0RCA interface\n");
	}

	return 0;
}

#ifdef CONFIG_CRAY_RS_DEBUG
uint32_t l0rca_buf_full;
#endif /* CONFIG_CRAY_RS_DEBUG */
/* 
 * Function:	ch_send_data
 * 
 * Description: Sends data towards the L0. 
 * The data that buf points to is sent as the payload in an rs_event structure.
 * The header is a separate parameter and the send routine directly copies
 * the header and the data into the circular buffer, thus avoiding a copy.
 *
 * NB: Reflect any changes to ch_send_data in gdb_ch_send_data
 * 
 * Arguments: int ch_num IN: channel number on which to send data
 * 	      rs_event_t *ev_hdr  IN: Header without len & timestamp
 * 	      void* buf  IN: Buffer with data
 * 	      int len    IN: length of data to transfer
 *
 * Returns: -EINVAL - if no user registered on the channel (Debug only)
 *          -EFAULT - if buf or ev_hdr is NULL (Debug only)
 *          -E2BIG -  if len exceeds max event payload (RS_MSG_LEN) (Debug only)
 *          zero -    SUCCESS, all bytes sent.
 *          > 0 -     Bytes not sent. Sender should retry.
 *
 * Notes: data in buf will be copied to the channel buffer, therfore, upon
 *	  return, user can free the buf. buf may not be NULL and the len must
 *	  not be zero. Use ch_send_event to send events with zero length data
 *	  portion.
 */

int ch_send_data(int ch_num, const rs_event_t *ev_hdr, 
		 void* buf, unsigned int len)
{
	l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[ch_num];
	uint32_t ev_len = 0;
	volatile uint32_t tmpwx, tmprx;

#ifdef CONFIG_CRAY_RS_DEBUG
	uint32_t wr, rd, no_events;
	if ((NULL == ev_hdr) || (NULL == buf))
	{
		return -EFAULT;
	}

	if ((NUM_L0RCA_CHANNELS <= ch_num) || (0 == len) ||
	    (L0RCA_INITVAL != l0rca_early_cfg.initialized) ||
	    (0 == ch_ptr->reg_count))
	{
		return -EINVAL;
	}

	/* Normalize the write & read indexes */
        SSGET32(ch_ptr->ch_widx, tmpwx);
	wr = tmpwx & (ch_ptr->num_obj - 1);
        SSGET32(ch_ptr->ch_ridx, tmprx);
	rd = tmprx & (ch_ptr->num_obj - 1);


	/* Calculate the number of events we will be sending */
	no_events = (len + RS_MSG_LEN -1)/RS_MSG_LEN;
	if ((wr + (no_events) - rd) < ch_ptr->num_obj)
	{
            l0rca_buf_full++;
	}

#endif /* CONFIG_CRAY_RS_DEBUG */

	/* Optimize for buf not full and only one event needed to send */

	/* 
	 * Masking of indexes is not needed for the check in while() below.
	 * Both widx & ridx are unsigned and increase monotonically such that
	 * ridx cannot lag behind widx by more than the circular buf size i.e.
	 * num_obj. widx will overflow before ridx.
	 * 'widx - ridx' will always yield the difference between the two 
	 * even if widx has overflowed and ridx has not yet overflowed.
	 */
	SSGET32(ch_ptr->ch_widx, tmpwx);
	SSGET32(ch_ptr->ch_ridx, tmprx);
	while ((tmpwx - tmprx) < ch_ptr->num_obj)
	{
		rs_event_t *wr_ev_ptr = 
		  &ch_ptr->ch_buf_ptr[tmpwx & (ch_ptr->num_obj - 1)];

		/* Copy same header for each event */
		SSMEMPUT(wr_ev_ptr, (uint32_t*) ev_hdr, rs_ev_hdr_sz);

		/* Copy the data portion over */
		ev_len = (RS_MSG_LEN > len) ? len : RS_MSG_LEN;
		SSPUT32(&(wr_ev_ptr->ev_len), ev_len);
		SSMEMPUT((char*)wr_ev_ptr + rs_ev_hdr_sz, buf, RS_MSG_LEN);

		/* TODO: Set the timestamp in each event */
#if 0
		SSPUT32(wr_ev_ptr->_ev_stp, 0x0);
#endif  /* 0 */
		len -= ev_len;
		/* 
		 * After updating the widx, DO NOT access any field in that 
		 * event. Though not desirable, the reader is free to alter 
		 * fields in the event.
		 */
		SSGET32(ch_ptr->ch_widx, tmpwx);
		SSPUT32(ch_ptr->ch_widx, tmpwx + 1);
		set_chan_tx_state(ch_ptr);

		/* Let L0 know an ev is available */
                send_intr_to_l0(ch_ptr);

		if (0 == len)
			break;

		buf = (void *)(((char *)buf) + RS_MSG_LEN);

		SSGET32(ch_ptr->ch_widx, tmpwx);
		SSGET32(ch_ptr->ch_ridx, tmprx);
	}	/* End of while */
	return len;	/* bytes remaining, if any */
}

/* 
 * Function:	ch_send_event
 * 
 * Description: Sends an event to L0. An event with zero length data portion
 * 		is supported.
 * 
 * Arguments: int ch_num IN: channel number on which to send the event
 * 	      const rs_event_t *evp  IN: EVent to send
 *
 * Returns: -EINVAL - if no user registered on the channel (Debug only)
 *          -EFAULT - if evp is NULL (Debug only)
 *          zero -    SUCCESS, event sent.
 *          +EBUSY -  Event not sent. Sender should retry.
 *
 * Notes: The event will be copied to the channel buffer, therfore, upon
 *	  return, user may free the space associated with the event
 */
int ch_send_event(int ch_num, const rs_event_t *evp)
{
	int ret = 0;
	l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[ch_num];
        volatile uint32_t tmpwx, tmprx;

#ifdef CONFIG_CRAY_RS_DEBUG
	if (NULL == evp)
	{
		return -EFAULT;
	}

	if ((NUM_L0RCA_CHANNELS <= ch_num) ||
	    (L0RCA_INITVAL != l0rca_early_cfg.initialized) ||
	    (0 == ch_ptr->reg_count))
	{
		return -EINVAL;
	}
#endif /* CONFIG_CRAY_RS_DEBUG */

	/* Optimize for circular buffer not full */

	/* 
	 * Masking of indexes is not needed for the check in while() below.
	 * Both widx & ridx are unsigned and increase monotonically such that
	 * ridx cannot lag behind widx by more than the circular buf size i.e.
	 * num_obj. widx will overflow before ridx.
	 * 'widx - ridx' will always yield the difference between the two 
	 * even if widx has overflowed and ridx has not yet overflowed.
	 */
	SSGET32(ch_ptr->ch_widx,tmpwx);
	SSGET32(ch_ptr->ch_ridx,tmprx);
	if((tmpwx - tmprx) < ch_ptr->num_obj)
	{
		rs_event_t *wr_ev_ptr = 
		  &ch_ptr->ch_buf_ptr[tmpwx & (ch_ptr->num_obj - 1)];

		/* Copy header & data length for the event */
		SSMEMPUT(wr_ev_ptr, (uint32_t*)evp, rs_ev_hdr_sz + evp->ev_len);

		SSGET32(ch_ptr->ch_widx, tmpwx);
		SSPUT32(ch_ptr->ch_widx, tmpwx + 1);
		set_chan_tx_state(ch_ptr);

		/* Let L0 know an ev is available */
                send_intr_to_l0(ch_ptr);
	} else
	{
		ret = EBUSY;
	}

	return ret;
}

/* 
 * Function:	l0rca_ch_get_event
 * 
 * Description: Read an event from L0 (if any). If an event is availabe then 
 *		the read pointer is advanced.
 *		NB: Reflect any changes to l0rca_ch_get_event in gdb_l0rca_ch_get_event
 * 
 * Arguments: int ch_num IN: channel number from which to read the event
 * 	      rs_event_t *evp  IN: Buffer to place the event
 *
 * Returns: -EINVAL - if no user registered on the channel (Debug only)
 *          -EFAULT - if evp is NULL (Debug only)
 *          zero -    no event to receive at this time
 *          > 0 -     Event received
 *
 */
int l0rca_ch_get_event(int ch_num, rs_event_t *evp)
{
	l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[ch_num];
	int ret = 0;
	uint32_t nbytes;
	volatile uint32_t tmpwx, tmprx;

#ifdef CONFIG_CRAY_RS_DEBUG
	if (NULL == evp)
		return -EFAULT;

	if ((NUM_L0RCA_CHANNELS <= ch_num) ||
	    (L0RCA_INITVAL != l0rca_early_cfg.initialized) ||
	    (0 == ch_ptr->reg_count))
		return -EINVAL;
#endif /* CONFIG_CRAY_RS_DEBUG */

	SSGET32(ch_ptr->ch_widx,tmpwx);
	SSGET32(ch_ptr->ch_ridx,tmprx);
	if(tmpwx != tmprx)
	{
		rs_event_t *wr_ev_ptr = 
		  &ch_ptr->ch_buf_ptr[tmprx & (ch_ptr->num_obj - 1)];

		/* Copy over the event */
		SSGET32(&(wr_ev_ptr->ev_len), nbytes);
		SSMEMGET(evp, (uint32_t*)wr_ev_ptr, nbytes + rs_ev_hdr_sz);

		/* Update the rd index */
		SSGET32(ch_ptr->ch_ridx, tmprx);
		SSPUT32(ch_ptr->ch_ridx, tmprx + 1);

		/* Let L0 know that the  event has been drained */
		send_intr_to_l0(ch_ptr);

		ret = 1;
	}

	return ret;
}

/* 
 * Function:	unregister_ch_down
 * 
 * Description: Unregister function for the upload channel. Use to indicate
 * that the channel is no longer to be used. The read & write pointers are 
 * equalized to make the circ buffer empty.
 * 
 * Arguments: int ch_num IN: channel number to unregister
 *
 * Returns: -EINVAL - if ch_num is not correct or no user registered
 *          zero (SUCCESS)
 */
int unregister_ch_down(int ch_num)
{
	l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[ch_num];
	volatile uint32_t tmpwx;

	if (NUM_L0RCA_CHANNELS <= ch_num)
		return -EINVAL;

	/* Check if user is using this channel */
	if (!ch_ptr->reg_count)
		return -EINVAL;

	ch_ptr->down_handler = NULL;
	ch_ptr->reg_count--;

	/* Equalize the read & write pointers i.e. drain the circ buffer */
	/* NOTE: We cannot really stop the L0 from sending data */
	SSGET32(ch_ptr->ch_widx, tmpwx);
	SSPUT32(ch_ptr->ch_ridx, tmpwx);

	return 0;
}

/* 
 * Function:	unregister_ch_up
 * 
 * Description: Unregister function for the download channel. Use to indicate
 * that the channel is no longer to be used.
 * 
 * Arguments: int ch_num IN: channel number to unregister
 *
 * Returns: -EINVAL - if ch_num is not correct or no user registered
 *          zero (SUCCESS)
 */
int unregister_ch_up(int ch_num)
{
	l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[ch_num];
	volatile uint32_t tmpwx, tmprx;

	if (NUM_L0RCA_CHANNELS <= ch_num)
		return -EINVAL;

	/* Check if user is using this channel */
	if (!ch_ptr->reg_count)
		return -EINVAL;

	/* Wait for events to be drained by the L0 */
	SSGET32(ch_ptr->ch_widx,tmpwx);
	SSGET32(ch_ptr->ch_ridx,tmprx);
	while(tmpwx != tmprx)
	{
		udelay(1000);
		SSGET32(ch_ptr->ch_widx,tmpwx);
		SSGET32(ch_ptr->ch_ridx,tmprx);
	}

	ch_ptr->up_handler = NULL;
	ch_ptr->reg_count--;

	return 0;
}

/* TODO: If we decide to use the PKT_MODE register to indicate the channels
 * that need attention then use PKT_MODE instead of looking at each channel.
 * TODO: Currently the rx_done callback is invoked for each event in the 
 * incoming channel. Change this to be called with all the pending events.
 * Note that since the channel is a circular buffer and the pending events
 * wrap around, then the callback needs to be invoked twice - once with events
 * upto the "end" of the circular buffer and then with events starting from the
 * "start" of the circular buffer.
 * TODO: To prevent thrashing of the tx_done callback in case the circular
 * buffer is being operated under almost full condition, obey the threshold
 * specified at the registration. The tx_done callback is only called once the
 * circular buffer occupancy is below the specified threshold.
 */
/* 
 * Function:	l0rca_poll_callback
 * 
 * Description: Scan the incoming channels and call the receive callback
 *		(if any) in case an event is pending to be processed.
 * 		Update the read pointer. Next scan the outgoing channels
 * 		and if the channel was full, call the transmit done callback
 *		so that events may be sent.
 * 
 * Arguments: None
 *
 * Returns: 0 if no events were processed, else 1.
 *
 * Note: It is possible that this routine is called from interrupt
 * 	 context. The callbacks invoked *must* not block.
 */

int l0rca_poll_callback(void)
{
	int i;
	int rval = 0;
	rs_event_t ev, *wr_ev_ptr;
	volatile uint32_t tmpwx, tmprx;

	/* Loop through all channels with incoming events */
	for (i = 1; i < NUM_L0RCA_CHANNELS; i+=2)
	{
		l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[i];

		/* GDB handled separately.
		 */
		if (i == L0RCA_CH_KGDB_DOWN) 
		    continue;

		if ((0 == ch_ptr->reg_count) || (NULL == ch_ptr->down_handler))
			continue;
		
		if ((ch_ptr->ch_ridx == 0) || (ch_ptr->ch_widx == 0)) {
		    continue;
		}

		if (!ch_ptr->num_obj) {
		    SSGET32(ch_ptr->ch_widx, tmpwx);
		    SSGET32(ch_ptr->ch_ridx, tmprx);
		    continue;
		}

		SSGET32(ch_ptr->ch_widx, tmpwx);
		SSGET32(ch_ptr->ch_ridx, tmprx);
		if (tmpwx != tmprx)
		{
			wr_ev_ptr = (rs_event_t*)&ch_ptr->ch_buf_ptr[tmprx & (ch_ptr->num_obj - 1)];
			/* read the entire event */
			SSMEMGET((uint32_t*)&ev, (uint32_t*)wr_ev_ptr, rs_sizeof_event(RS_MSG_LEN));

			/* Call callback routine with one event */
			(ch_ptr->down_handler)(i, &ev, 1);

			/* We are done with this event */
			rval++;
			SSGET32(ch_ptr->ch_ridx, tmprx);
			SSPUT32(ch_ptr->ch_ridx, tmprx + 1);

			/* Let L0 know that the  event has been drained */
			send_intr_to_l0(ch_ptr);
		} /* End of if */
	} /* End of for incoming channels */

	/* Loop through all channels with outgoing events */
	for (i = 0; i < NUM_L0RCA_CHANNELS; i+=2)
	{
		l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[i];

		/* GDB handled separately.
		 */
		if (i == L0RCA_CH_KGDB_UP)
			continue;

		if(NULL != ch_ptr->up_handler)
		{
			/* Lock needed for mutex with tx routine */
			LOCK_CHANNEL(i);
			if (CHAN_TX_STATE_FULL == GET_CHAN_STATE(ch_ptr))
			{
				/* Call the callback if chan no longer full */
				assert(0 != ch_ptr->reg_count);
				SSGET32(ch_ptr->ch_widx, tmpwx);
				SSGET32(ch_ptr->ch_ridx, tmprx);
				if((tmpwx - tmprx) < ch_ptr->num_obj)
				{
					rval++;
					SET_CHAN_STATE(ch_ptr,CHAN_TX_STATE_AVAIL);
					UNLOCK_CHANNEL(i);
					(ch_ptr->up_handler)(i);
					LOCK_CHANNEL(i);
				}
			} /* End of if */
			UNLOCK_CHANNEL(i);
		}
	} /* End of for */

	return rval;
}

#if defined(CONFIG_CRAY_XT_KGDB) || defined(CONFIG_CRAY_KGDB)
/* Kernel mode GDB via L0 interface routines.
 *
 * Note that this code was derived from gdbl0.c,
 * which was derived from Linux gdbserial.c,
 * which contains no copyright notice.  Parts
 * of this may need to be rewritten if a GPL
 * copyright notice was removed from gdbserial.c
 *
 */

#define LRPRINTF  printk

#undef	PRNT				/* define for debug printing */

#define	GDB_BUF_SIZE	512		/* power of 2, please */

static char	gdb_buf[GDB_BUF_SIZE] ;
static int	gdb_buf_in_inx ;
static int	gdb_buf_in_cnt ;
static int	gdb_buf_out_inx ;

static int initialized = -1;

int gdb_store_overflow;
int gdb_read_error;

/* Preset this with constant fields in the event header */
static rs_event_t l0rca_gdb_ev_template = {0};

/*
 * Function:    rcal0_gdb_template
 *
 * Description: Hand craft a event header to be sent with each outgoing event
 *
 * Arguments: None.
 *
 * Returns: None
 *
 * Note: The len & timestamp are not filled in.
 */
static void rcal0_gdb_template(void)
{
	l0rca_gdb_ev_template.ev_id = ec_kgdb_output;
	l0rca_gdb_ev_template.ev_gen = RCA_MKSVC(RCA_INST_ANY,
	    RCA_SVCTYPE_TEST0, l0rca_get_proc_id());
	l0rca_gdb_ev_template.ev_src = l0rca_gdb_ev_template.ev_gen;
	l0rca_gdb_ev_template.ev_priority = RCA_LOG_DEBUG;
	l0rca_gdb_ev_template.ev_flag = 0;	/* For Debugging */

	/* Timestamp, len & data is filled at the time of sending event */
}

/*
 * Function:    l0_gdb_init
 *
 * Description: Take steps to set things up for early_printk output
 *
 * Arguments: struct console *console IN: pointer to console struct
 *            char *input IN: Not used
 *
 * Returns: zero (SUCCESS)
 */
int l0_gdb_init(void)
{

	int ret;

	/* RCA already initialized on Q
	 */
	/* Read the configuration information provided by L0 */
	l0rca_init_config();

	/* Setup the Event template to use for outgoing events */
	rcal0_gdb_template();

	/* Set up channel internal state by calling
	 * registration routines.
	 */

	/* Register with the KGDB out channel to send gdb data */
	ret = register_ch_up (L0RCA_CH_KGDB_UP, NULL, 0, 0);

	if (!ret)
	{
		/* Register with the KGDB in channel to receive gdb commands */
		ret = register_ch_down(L0RCA_CH_KGDB_DOWN, NULL, 0);
	}
	
	return ret;
}

extern void breakpoint(void);


int gdb_hook(void)
{
	int retval;

	/*
	 * Call GDB routine to setup the exception vectors for the debugger.
	 */
	
	/* Setup both kgdb channels */
	retval = l0_gdb_init();

	/* TODO: on Linux the call to printk in this
	 * routine no longer generate output.
	 *
	 * Did something change in the setup of the
	 * console channel?
	 */
	if (retval == 0)
	{
		initialized = 1;
	} else
	{
		initialized = 0;
		LRPRINTF("gdb_hook: l0_gdb_init() failed: %d\n", retval);
		return (-1);
	}
	return 0;

} /* gdb_hook */

/*
 * Function:   l0rca_kgdb_down_getc()
 * 
 * Description:	Checks for an event on the KGDB DOWN L0 channel
 *		and returns the first character, other characters
 *              are thrown away.  Used to detect control C for
 *              breakpointing the kernel.  Returns 0 when no
 *		input is available or on error where gdb_read_error
 *	        is incremented.
 */
int l0rca_kgdb_down_getc(void)
{
	char	*chp;
	int ret, len;
	rs_event_t ev = {0};

	if ((ret = l0rca_ch_get_event(L0RCA_CH_KGDB_DOWN, &ev)) <= 0) {
		if (ret < 0)
		     gdb_read_error++;
		return 0;
	}
	l0rca_event_data(&ev, (void *)&chp, &len);
	if (len > 0)
		return *chp;
	return 0;
} /* l0rca_kgdb_down_getc */

/* Only routines used by the Kernel trap mode KGDB interface
 * should follow this point.
 *
 * These functions should only call other `gdb_.*' functions.
 *
 * This is best done by examining the assembly file and
 * ensuring that all assembly call statements only call
 * routines that match `gdb_.*', in all of the routines
 * that follow.
 *
 * NB: SETTING BREAKPOINTS IN THE FOLLOWING ROUTINES MAY
 * BREAK THE KERNEL DEBUGGER.
 */

/* 
 * Function:	gdb_l0rca_event_data
 * 
 * Description:	Clone of l0rca_event_data to only be called by kernel GDB.
 * 		data portion of the event.
 * 
 * Arguments: rs_event_t *evp IN: Event whose data is of interest
 * 	      void **data OUT: Upon return will point to data portion of event
 * 	      int32_t *len OUT: Upon return will have the length of the data
 *	                        portion of the event
 *
 * Returns: No Return Value.
 */
void gdb_l0rca_event_data(rs_event_t *evp, void **data, int32_t *len)
{
        /* Get length and data portion from the event */
        *len = evp->ev_len;
        *data = &evp->ev_data;
} /* gdb_l0rca_event_data */

/*
 * Function:	gdb_set_chan_tx_state
 *
 * Description:	Clone of set_chan_tx_state to only be called by kernel GDB.
 */
static inline void gdb_set_chan_tx_state(l0rca_mapped_ch_t *ch_ptr)
{

	int not_full = (*ch_ptr->ch_widx - *ch_ptr->ch_ridx) < ch_ptr->num_obj;

	SET_CHAN_STATE(ch_ptr, not_full  ? CHAN_TX_STATE_AVAIL : CHAN_TX_STATE_FULL);
}

static void gdb_memcpy(void *dest, const void *src, int cnt)
{
    int i;
    char *pd = dest;
    const char *ps = src;

    for (i = 0; i < cnt; i++)
	pd[i] = ps[i];
}

/* 
 * Function:	gdb_ch_send_data
 *
 * Description:	Clone of ch_send_data to be only called by kernel GDB.
 * 
 */
int gdb_ch_send_data(int ch_num, const rs_event_t *ev_hdr, 
		 void* buf, unsigned int len)
{
	l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[ch_num];

	/* No registration checks needed.
	 */

	/* Optimize for buf not full and only one event needed to send */

	/* 
	 * Masking of indexes is not needed for the check in while() below.
	 * Both widx & ridx are unsigned and increase monotonically such that
	 * ridx cannot lag behind widx by more than the circular buf size i.e.
	 * num_obj. widx will overflow before ridx.
	 * 'widx - ridx' will always yield the difference between the two 
	 * even if widx has overflowed and ridx has not yet overflowed.
	 */
	while((*ch_ptr->ch_widx - *ch_ptr->ch_ridx) < ch_ptr->num_obj)
	{
		rs_event_t *wr_ev_ptr = 
		  &ch_ptr->ch_buf_ptr[*ch_ptr->ch_widx & (ch_ptr->num_obj - 1)];

		/* Copy same header for each event */
		gdb_memcpy(wr_ev_ptr, (void *) ev_hdr, rs_ev_hdr_sz);

		/* Copy the data portion over */
		wr_ev_ptr->ev_len = (RS_MSG_LEN > len) ? len : RS_MSG_LEN;
		gdb_memcpy((char *)wr_ev_ptr + rs_ev_hdr_sz, buf,
		       wr_ev_ptr->ev_len);

		/* TODO: Set the timestamp in each event */
#if 0
		wr_ev_ptr->_ev_stp = 0x0;
#endif	/* 0 */
		
		len -= wr_ev_ptr->ev_len;
		/* 
		 * After updating the widx, DO NOT access any field in that 
		 * event. Though not desirable, the reader is free to alter 
		 * fields in the event.
		 */
		(*ch_ptr->ch_widx)++;
		gdb_set_chan_tx_state(ch_ptr);

		/* Let L0 know an ev is available */
                send_intr_to_l0(ch_ptr);

		if (0 == len)
			break;

		buf += RS_MSG_LEN;
	}	/* End of while */

	return len;	/* bytes remaining, if any */
}

/* 
 * Function:	gdb_l0rca_ch_get_event
 * 
 * Description:	Clone of l0rca_ch_get_event to only be called by kernel GDB.
 *		the read pointer is advanced.
 * 
 */
int gdb_l0rca_ch_get_event(int ch_num, rs_event_t *evp)
{
	l0rca_mapped_ch_t *ch_ptr = &l0rca_early_cfg.ch_data[ch_num];
	int ret = 0;

	/* No registration checks needed
	 */

	if(*ch_ptr->ch_widx != *ch_ptr->ch_ridx)
	{
		rs_event_t *wr_ev_ptr = 
		  &ch_ptr->ch_buf_ptr[*ch_ptr->ch_ridx & (ch_ptr->num_obj - 1)];

		/* Copy over the event */
		gdb_memcpy(evp, (void *)wr_ev_ptr, wr_ev_ptr->ev_len+rs_ev_hdr_sz);

		/* Update the rd index */
		(*ch_ptr->ch_ridx)++;

		/* Let L0 know that the  event has been drained */
		send_intr_to_l0(ch_ptr);

		ret = 1;
	}

	return ret;
}

/*
 * Function:    gdb_store_char_in_buf
 *
 * Description: Check for overflow and place the incoming character into 
 * 		the local buffer for later retreival.
 *
 * Arguments: int ch IN: Incoming character
 *
 * Returns: zero  - (SUCCESS)
 * 	    -1    - Buffer Overflow
 */
static int gdb_store_char_in_buf(char ch)
{
	if (gdb_buf_in_cnt >= GDB_BUF_SIZE)
	{	/* buffer overflow, clear it */
		gdb_buf_in_inx = 0 ;
		gdb_buf_in_cnt = 0 ;
		gdb_buf_out_inx = 0 ;
		return -1;
	}

	gdb_buf[gdb_buf_in_inx++] = ch;
	gdb_buf_in_inx &= (GDB_BUF_SIZE - 1) ;
	gdb_buf_in_cnt++;

	return 0;
}

/*
 * Wait until the interface can accept a char, then write it.
 */
static void gdb_write_char(char chr)
{
	int ret;

	while ((ret = gdb_ch_send_data(L0RCA_CH_KGDB_UP, &l0rca_gdb_ev_template,
	       (void *)&chr, sizeof(char))) > 0)
	{
		/* Buffer full; keep trying.... */
		;
	}	/* End of while */

	return;
} /* gdb_write_char */

/*
 * gdb_getc
 *
 * This is a GDB stub routine.  It waits for a character from the
 * L0 interface and then returns it.
 */
int gdb_getc(int wait)
{
	char	*chp, *end_buf;
	int ret, len;
	rs_event_t ev = {0};

#ifdef PRNT
	LRPRINTF("gdb_getc:") ;
#endif
	/* First check if the receive callback has any chars pending */
	if (gdb_buf_in_cnt == 0)
	{
		/* No chars from rx_callback; Loop until a char is available */
		while ((ret = gdb_l0rca_ch_get_event(L0RCA_CH_KGDB_DOWN, &ev)) 
		        <= 0)
		{
			if (ret < 0)
			{
				/* Error!! This is death */
				gdb_read_error++;
				return -1;
			}
			if (! wait)
				return 0;
		} /* End of while */

		/* Get the data in the event */
		gdb_l0rca_event_data(&ev, (void *)&chp, &len);

		/* We have an event; fill the local buffer */
		for (end_buf = chp+len; chp < end_buf; chp++)
		{
			if (gdb_store_char_in_buf(*chp) < 0)
			{
				gdb_store_overflow++;
			}
		} /* End of for */
	} /* End of if */

	/* There should be something for us in the local buffer now */
	chp = &gdb_buf[gdb_buf_out_inx++] ;
	gdb_buf_out_inx &= (GDB_BUF_SIZE - 1) ;
	gdb_buf_in_cnt--;

#ifdef PRNT
	LRPRINTF("%c\n", *chp > ' ' && *chp < 0x7F ? *chp : ' ') ;
#endif
	return(*chp) ;

} /* gdb_getc */

/*
 * gdb_putc
 *
 * This is a GDB stub routine.  It waits until the interface is ready
 * to transmit a char and then sends it.  If there is no serial
 * interface connection then it simply returns to its caller, having
 * pretended to send the char.
 */
int gdb_putc(char chr)
{
#ifdef PRNT
	LRPRINTF("gdb_putc: chr=%02x '%c'\n", chr,
		 chr > ' ' && chr < 0x7F ? chr : ' ') ;
#endif

	gdb_write_char(chr);	/* this routine will wait */

	return 1;

} /* gdb_putc */

int	putDebugPacket(char *buf, int n)
{
	int ret = -1;

	/* Loop sending the data */
	while (n)
	{
		if ((ret = gdb_ch_send_data(L0RCA_CH_KGDB_UP, &l0rca_gdb_ev_template,
                    (void *)buf, n)) <= 0) {
                        /* Either error or we are done */
                        break;
                }

                if (n > ret) {
                        /* Some bytes were sent, point to the remaining data */
                        buf += (n - ret);
                        n = ret;
                }
	}	

	return ret;
}
#endif /* CONFIG_CRAY_KGDB */
