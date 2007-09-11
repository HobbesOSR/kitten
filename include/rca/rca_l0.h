/*
 * Copyright (c) 2003 Cray Inc.
 *
 * The contents of this file is proprietary information of Cray Inc.
 * and may not be disclosed without prior written consent.
 *
 */
/*
 * This code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#ifndef	__RCA_L0_H__
#define	__RCA_L0_H__

#include <rca/l0rca_config.h>


/*
 * Macros to read/write Seastar Scratch RAM for everyone else.
 */
#define SSMEMPUT(dest,src,nb)	memcpy((void *)dest,(void *)src,nb)
#define SSMEMGET(dest,src,nb)	memcpy((void *)dest,(void *)src,nb)

#define SSPUT64(to_ptr,value) (*(to_ptr) = (value))
#define SSPUT32(to_ptr,value) (*(to_ptr) = (value))

#define SSGET64(from_ptr,to_var) ((to_var) = *(from_ptr))
#define SSGET32(from_ptr,to_var) ((to_var) = *(from_ptr))

/* TODO - Revisit these later */
#define	LOCK_CHANNEL(chn_num)
#define	UNLOCK_CHANNEL(chn_num)

typedef int (*l0rca_down_handle_t)(int chn_num, rs_event_t* buf, int32_t num);
typedef int (*l0rca_up_handle_t)(int chn_num);

typedef struct l0rca_ch_status {
	uint32_t 	num_obj;		/* number of objects    */
	uint32_t 	ridx;			/* read index           */
	uint32_t 	widx;			/* write index */
	uint32_t 	reg_count;
	rs_event_t*	ch_buf_ptr;
} l0rca_ch_status_t;

/* 
 * API defines 
 * TODO - All API calls defined here may not be implemented in l0rca.c.
 * These are to be implemented as needed.
 */

/* NOTE
 * download means data transfer from the L0 to the Opteron
 * upload means data transfer from the Opteron to the LO; 
 */

#ifdef STANDALONE_DIAGS
/* 
 * Checks if the channel is ready or not (full).
 * Argument:	int	channel
 * Returns:
 *   1 if ready (not_full)
 *   0 if not ready
 */
int l0rca_ch_send_ready(int ch_num);

/*
 * Clears l0rca_early_cfg.initialized.
 * This function is required for memtest. Memtest has to move the location
 * of the storage area for the config in order to move on to the next
 * region to do the memory test.
 */
void l0rca_clear_initialized(void);
#endif

/*
 * Function:	l0rca_init_config
 *
 * Description: Read L0 - RCA communication config structure and populate 
 *              our personal copy. If there is any error, the OS panics
 *              as not being able to communicate with L0 is a total disaster.
 *              If already initialized then returns siliently.
 *
 * Arguments: None.
 *
 * Returns: None
 */
void l0rca_init_config(void);

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
 */
int register_ch_down(int ch_num, l0rca_down_handle_t handler, int poll);

/* 
 * Function:	unregister_ch_down
 * 
 * Description: Unregister function for the download channel. Use to indicate
 * that the channel is no longer to be used.
 * 
 * Arguments: int ch_num IN: channel number to unregister
 *
 * Returns: zero (SUCCESS)
 */
int unregister_ch_down(int ch_num);

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
 *			   if = zero - tx done interrupt invokes callback
 *			   if < zero - do nothing. It is assumed that the user
 * 			   has her own means to check for buffer drain
 *
 * Returns: -EBUSY - If another user is already registered.
 *          -EINVAL - if ch_num is not in range.
 *          zero (SUCCESS) otherwise.
 */
int register_ch_up(int ch_num, l0rca_up_handle_t handler, int tshhld, int poll);

/* 
 * Function:	unregister_ch_up
 * 
 * Description: Unregister function for the download channel. Use to indicate
 * that the channel is no longer to be used.
 * 
 * Arguments: int ch_num IN: channel number to unregister
 *
 * Returns: zero (SUCCESS)
 */
int unregister_ch_up(int ch_num);

/* 
 * Function:	ch_send_data
 * 
 * Description: Sends data towards the L0. 
 * The data that buf points to is sent as the payload in an rs_event structure.
 * The header is a separate parameter and the send routine directly copies
 * the header and the data into the circular buffer, thus avoiding a copy.
 * 
 * Arguments: int ch_num IN: channel number on which to send data
 * 	      rs_event_t *ev_hdr  IN: Header without len & timestamp
 * 	      void* buf  IN: Buffer with data
 * 	      unsigned int len IN: length of data to transfer
 *
 * Returns: EBUSY - If the circular channel buffer is full.
 *          EINVAL - if no user registered on the channel (Debug only)
 *          EFAULT - if buf or ev_hdr is NULL (Debug only)
 *          E2BIG -  if len exceeds max event payload (RS_MSG_LEN) (Debug only)
 *          zero (SUCCESS) otherwise.
 *
 * Notes: data in buf will be copied to the channel buffer, therfore, upon
 *	  return, user can free the buf.
 */
int ch_send_data(int ch_num, const rs_event_t *ev_hdr, 
		 void* buf, unsigned int len);

/* 
 * Function:	ch_send_event
 * 
 * Description: Sends an event to L0. 
 * 
 * Arguments: int ch_num IN: channel number on which to send the event
 * 	      const rs_event_t *evp  IN: EVent to send
 *
 * Returns: -EINVAL - if no user registered on the channel (Debug only)
 *          -EFAULT - if ev_hdr is NULL (Debug only)
 *          zero -    SUCCESS, event sent.
 *          +EBUSY -  Event not sent. Sender should retry.
 *
 * Notes: The event will be copied to the channel buffer, therfore, upon
 *	  return, user may free the space associated with the event
 */
int ch_send_event(int ch_num, const rs_event_t *evp);

/* 
 * Function:	ch_status
 * 
 * Description: Obtain status on the channel 
 * 
 * Arguments: int ch_num IN: channel number for which to obtain status
 * Arguments: l0rca_ch_status_t *st OUT: status of the channel.
 *
 * Returns: zero (SUCCESS).
 *
 * Notes: The status represents the snapshot at the time of invocation.
 */
int ch_status(int ch_num, l0rca_ch_status_t *st);

/* 
 * Function:	l0rca_ch_get_event
 * 
 * Description: Read the next available event (if any). This allows the caller
 *		to check for incoming events. It is usefult for those callers
 *		that do not have a receive callback.
 * 
 * Arguments: int ch_num IN: channel number from which to return event
 * Arguments:  rs_event_t *evp: Pointer where the event may be placed
 *
 * Returns: 0 or 1 - No of events returned (0 ==> No event otherwise always 1)
 *          < 0   - errors such as channel not registered etc.
 *          
 * Note: The receive callback is the preferred way to handle incoming events. 
 *	 This API call should only be used in cases where a receive callback 
 *	 mechanism is not feasible. For example, when interupts are disabled and
 *	 incoming events need to be serviced. An example user is a kernel 
 *	 debugger.
 */
int l0rca_ch_get_event(int ch_num, rs_event_t *evp);

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
int l0rca_poll_callback(void);

/* 
 * Function:	l0rca_get_proc_id
 * 
 * Description: Return the node/processor id.
 * 
 * Arguments: None
 *
 * Returns: The proc id.
 */
rs_node_t l0rca_get_proc_id(void);

/* 
 * Function:	l0rca_get_max_xyz
 * 
 * Description: Returns the current system dimensions. This information
 *		can be used find the coordinates of the node in the system.
 * 
 * Arguments: int32_t *mx OUT: The x value is stored here after return
 *	      int32_t *my OUT: The y value is stored here after return
 *	      int32_t *mz OUT: The z value is stored here after return
 * 
 * Returns: No return value.
 */
void l0rca_get_max_xyz(int32_t *mx, int32_t *my, int32_t *mz);

/* 
 * Function:	l0rca_event_data
 * 
 * Description: Return a pointer to the data portion and length of the
 * 		data portion of the event.
 * 
 * Arguments: rs_event_t *evp IN: Event whose data is of interest
 * 	      void **data OUT: Upon return will point to data portion of event
 * 	      int32_t *len OUT: Upon return will have the length of the data
 *	                        portion of the event
 *
 * Returns: No Return Value.
 */
void l0rca_event_data(rs_event_t *evp, void **data, int32_t *len);


#ifdef __KERNEL__
extern int l0_gdb_init(void);
extern int l0rca_kgdb_down_getc(void);
extern int gdb_hook(void);
extern int gdb_getc(int wait);
extern int gdb_putc(char chr);
extern int putDebugPacket(char *buf, int n);
#endif

#endif	/* __RCA_L0_H__ */
