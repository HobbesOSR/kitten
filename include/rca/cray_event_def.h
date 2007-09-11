/*
 * Copyright (c) 2003 Cray, Inc.
 *
 * The contents of this file are proprietary information of Cray Inc. 
 * and may not be disclosed without prior written consent.
 */
/*
 *
 * This code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */


#ifndef __CRAY_EVENT_DEF_H__
#define __CRAY_EVENT_DEF_H__


#include <lwk/types.h>
#include <lwk/time.h>
#include <rca/rca_defs.h>

typedef union rs_node_u {
    /* Little Endian */
    struct {
        uint32_t _node_arch     : 2;    /* System architecture      */
        uint32_t _node_type     : 6;    /* Component type           */
        uint32_t _node_state    : 7;    /* Component state from SM  */
        uint32_t _node_is_svc   : 1;    /* Service node bit         */

        uint32_t _node_id       : 16;   /* Node and Seastar NID     */

        uint32_t                : 2;    /* Unused                   */
        uint32_t _node_x        : 6;    /* What position in the row */
        uint32_t _node_subtype  : 4;    /* Component subtype        */
        uint32_t _node_row      : 4;    /* Which row of cabinets    */

        uint32_t _node_cage     : 4;    /* Cage in the cabinet      */
        uint32_t _node_slot     : 4;    /* Slot in the cage         */
        uint32_t _node_modcomp  : 4;    /* Component on a module    */
        uint32_t _node_link     : 4;    /* Link on seastar          */
    } __attribute__((packed)) rs_node_s;
    struct {
        uint32_t :2, :6, :7, :1, :16, :2, :6, :4, :4; /* Unused fields */
        uint32_t _node_startx   : 8;    /* What position in the row */
        uint32_t _node_endx     : 8;    /* Which row of cabinets */
    } __attribute__((packed)) rs_node_s1;
    uint64_t rs_node_flat;
} __attribute__((packed)) rs_node_t;

/* TODO: this and RCA RS_MSG_LEN define needs to be taken out soon. */
#ifndef RS_MSG_LEN 
#define RS_MSG_LEN 1
#endif

typedef uint32_t rs_error_code_t;
typedef int32_t rs_event_code_t;


// from rs_svc_id.h

/* NOTE for the following event related structures:
 * ###################################################################
 * There are following restrictions for the L0 Opteron communication
 * related structures.
 * The elements must be aligned on 4-byte boundaries.  The structure
 * size must be a multiple of 4 bytes. Structures should be packed so
 * that the compiler will not insert padding.
 * ###################################################################
 */
typedef uint32_t rs_service_t;
typedef uint32_t rs_instance_t;
typedef uint32_t  rs_priority_t;
typedef uint32_t  rs_flag_t;

/*
 * NOTE: This rs_service_id_t is packed.  If we update this structure,
 *  we need to make sure that each element is 4-byte aligned,
 *  otherwise it might break the L0 Opteron communication (size
 *  of rs_service_id_t must be a multiple of 4bytes).
 */
typedef struct rs_service_id_s {
    rs_instance_t svid_inst;    /* a sequence identifier */
    rs_service_t  svid_type;    /* the kind of service */
    rs_node_t     svid_node;    /* the x.y.z coordinates */
} __attribute__((packed)) rs_service_id_t;


/* time structure
 * rt_tv1 and rt_tv2 are hedges against field size inflation.
 */
typedef union rs_time_u {
    struct timeval _rt_tv;
    struct {
	    uint64_t _rt_tv1;
	    uint64_t _rt_tv2;
    } rs_evtime_s;      /* timeval needs to be adjusted for 32/64 bits */
} rs_time_t;

/*
 * NOTE: This rs_event_t is packed.  If we update this structure, we need to
 *      make sure that each element is 4-byte aligned, otherwise it might
 *      break the L0 Opteron communication (size of rs_event_t must be a
 *      multiple of 4bytes).
 *
 * event structure:
 * may be used as a fixed or variable length event.
 * In RCA's case, ev_data is fixed length and RS_MSG_LEN should be defined
 * before inclusion of this file.  ev_len here signifies the length in
 * bytes of significant data in ev_data.  The SMW, in contrast, treats events
 * as variable length;  RS_MSG_LEN is 1 and the actual length of the data is
 * determined when the object is allocated. In this case the real length
 * of ev_data is stored in ev_len.  RCA related events has fixed length
 * ev_data and RS_MSG_LEN is 256 (multiple of 4bytes).  Same as the SMW
 * events, real length of ev_data needs to be stored in ev_len.
 */
typedef struct rs_event_s {
    rs_event_code_t	ev_id;		     /* type of event */
    uint32_t		ev_seqnum;           /* req/rsp sequence number */
    rs_service_id_t	ev_gen;		     /* what this event pertains to */
    rs_service_id_t	ev_src;		     /* creator of this event */
    rs_flag_t		ev_flag;	     /* any bit flags */
    rs_time_t		_ev_stp;	     /* time of event creation */
    rs_priority_t	ev_priority;         /* priority [0 low, 9 high] */
    int32_t		ev_len;		     /* length of data */
    char		ev_data[RS_MSG_LEN]; /* payload (must be last) */
} __attribute__((packed)) rs_event_t;

#define rs_sizeof_event(data_length) \
    (((int)(&((rs_event_t*)0)->ev_data)) + (data_length)) 

#endif /* __CRAY_EVENT_DEF_H__ */
 
