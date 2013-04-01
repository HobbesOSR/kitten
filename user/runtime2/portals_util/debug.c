#include <stdio.h>
#include <stdlib.h>
#include <portals4.h>
#include "portals4_util.h"


/**
 * Converts a Portals4 return code to its string representation.
 */
char *
ptl_return_code_str(int return_code)
{
	switch (return_code) {
		case PTL_OK:              return "PTL_OK";
		case PTL_ARG_INVALID:     return "PTL_ARG_INVALID";
		case PTL_CT_NONE_REACHED: return "PTL_CT_NONE_REACHED";
		case PTL_EQ_DROPPED:      return "PTL_EQ_DROPPED";
		case PTL_EQ_EMPTY:        return "PTL_EQ_EMPTY";
		case PTL_FAIL:            return "PTL_FAIL";
		case PTL_IGNORED:         return "PTL_IGNORED";
		case PTL_IN_USE:          return "PTL_IN_USE";
		case PTL_INTERRUPTED:     return "PTL_INTERRUPTED";
		case PTL_LIST_TOO_LONG:   return "PTL_LIST_TOO_LONG";
		case PTL_NO_INIT:         return "PTL_NO_INIT";
		case PTL_NO_SPACE:        return "PTL_NO_SPACE";
		case PTL_PID_IN_USE:      return "PTL_PID_IN_USE";
		case PTL_PT_FULL:         return "PTL_PT_FULL";
		case PTL_PT_EQ_NEEDED:    return "PTL_PT_EQ_NEEDED";
		case PTL_PT_IN_USE:       return "PTL_PT_IN_USE";
	}
	return "UNKNOWN_RETURN_CODE";
}


/**
 * Converts a Portals4 event type ID to its string representation.
 */
char *
ptl_event_kind_str(ptl_event_kind_t type)
{
	switch (type) {
		case PTL_EVENT_GET:                   return "PTL_EVENT_GET";
		case PTL_EVENT_GET_OVERFLOW:          return "PTL_EVENT_GET_OVERFLOW";
		case PTL_EVENT_PUT:                   return "PTL_EVENT_PUT";
		case PTL_EVENT_PUT_OVERFLOW:          return "PTL_EVENT_PUT_OVERFLOW";
		case PTL_EVENT_ATOMIC:                return "PTL_EVENT_ATOMIC";
		case PTL_EVENT_ATOMIC_OVERFLOW:       return "PTL_EVENT_ATOMIC_OVERFLOW";
		case PTL_EVENT_FETCH_ATOMIC:          return "PTL_EVENT_FETCH_ATOMIC";
		case PTL_EVENT_FETCH_ATOMIC_OVERFLOW: return "PTL_EVENT_FETCH_ATOMIC_OVERFLOW";
		case PTL_EVENT_REPLY:                 return "PTL_EVENT_REPLY";
		case PTL_EVENT_SEND:                  return "PTL_EVENT_SEND";
		case PTL_EVENT_ACK:                   return "PTL_EVENT_ACK";
		case PTL_EVENT_PT_DISABLED:           return "PTL_EVENT_PT_DISABLED";
		case PTL_EVENT_LINK:                  return "PTL_EVENT_LINK";
		case PTL_EVENT_AUTO_UNLINK:           return "PTL_EVENT_AUTO_UNLINK";
		case PTL_EVENT_AUTO_FREE:             return "PTL_EVENT_AUTO_FREE";
		case PTL_EVENT_SEARCH:                return "PTL_EVENT_SEARCH";
	}
	return "UNKNOWN_EVENT_TYPE";
}


/**
 * Prints a Portals4 event to the screen.
 */
void
ptl_print_event(ptl_event_t *ev)
{
	printf("%s:\n", ptl_event_kind_str(ev->type));
	printf("    start         = %p\n",       ev->start);
	printf("    user_ptr      = %p\n",       ev->user_ptr);
	printf("    hdr_data      = %llx\n",     (unsigned long long)ev->hdr_data);
	printf("    match_bits    = %llx\n",     (unsigned long long)ev->match_bits);
	printf("    rlength       = %llu\n",     (unsigned long long)ev->rlength);
	printf("    mlength       = %llu\n",     (unsigned long long)ev->mlength);
	printf("    remote_offset = %llu\n",     (unsigned long long)ev->remote_offset);
	printf("    uid           = %d\n",       (int)ev->uid);
	printf("    initiator     = (%d, %d)\n", (int)ev->initiator.phys.nid,
	                                         (int)ev->initiator.phys.pid);
	printf("    type          = %d\n",       (int)ev->type);
	printf("    ptl_list      = %d\n",       (int)ev->ptl_list);
	printf("    pt_index      = %d\n",       (int)ev->pt_index);
	printf("    ni_fail_type  = %d\n",       (int)ev->ni_fail_type);
	printf("    atomic_op     = %d\n",       (int)ev->atomic_operation);
	printf("    atomic_type   = %d\n",       (int)ev->atomic_type);
}
