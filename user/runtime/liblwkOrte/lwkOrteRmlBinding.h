#ifndef _LWK_ORTE_RML_BINDING_H
#define _LWK_ORTE_RML_BINDING_H

#include "orte_config.h"
#include "orte/orte_constants.h"
#include "orte/mca/rml/base/base.h"

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif
    void* lwkOrteRmlAlloc();
    void lwkOrteRmlFree( void* );
    void lwkOrteRmlInit( void*, orte_process_name_t* myname );
    int  lwkOrteRmlRecvBufferNB( void*, orte_process_name_t * peer,
                               orte_rml_tag_t tag,
                               int flags,
                               orte_rml_buffer_callback_fn_t
                               cbfunc, void *cbdata);
    int  lwkOrteRmlRecvNB( void*, orte_process_name_t * peer,
                               orte_rml_tag_t tag,
                               int flags,
                               orte_rml_buffer_callback_fn_t
                               cbfunc, void *cbdata);
    int lwkOrteRmlSendBufferNB( void*, orte_process_name_t * peer,
                               orte_buffer_t * buffer,
                               orte_rml_tag_t tag,
                               int flags,
                               orte_rml_buffer_callback_fn_t
                               cbfunc, void *cbdata);
    int lwkOrteRmlBarrier( void* );
    int lwkOrteRmlRecvCancel( void*, orte_process_name_t * peer, orte_rml_tag_t tag );

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif
