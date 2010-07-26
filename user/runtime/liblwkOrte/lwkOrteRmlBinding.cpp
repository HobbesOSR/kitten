
#include <lwkOrteRmlBinding.h>
#include <lwkOrteRml.h>

void* lwkOrteRmlAlloc()
{
	return new LwkOrteRml;
}

void lwkOrteRmlFree( void* ptr )
{
	delete (LwkOrteRml*) ptr;
}

void lwkOrteRmlInit( void* obj, orte_process_name_t* me )
{
	((LwkOrteRml*)obj)->init( me );
}

int  lwkOrteRmlRecvBufferNB( void *obj, orte_process_name_t * peer,
                               orte_rml_tag_t tag,
                               int flags,
                               orte_rml_buffer_callback_fn_t cbfunc,
				void *cbdata)
{
	return ((LwkOrteRml*)obj)->recvBufferNB( peer, tag, flags, cbfunc, cbdata );
}

int  lwkOrteRmlRecvNB( void *obj, orte_process_name_t * peer,
                               orte_rml_tag_t tag,
                               int flags,
                               orte_rml_buffer_callback_fn_t cbfunc,
				void *cbdata)
{
	return ((LwkOrteRml*)obj)->recvNB( peer, tag, flags, cbfunc, cbdata );
}


int lwkOrteRmlSendBufferNB( void* obj, orte_process_name_t * peer,
                               orte_buffer_t * buffer,
                               orte_rml_tag_t tag,
                               int flags,
                               orte_rml_buffer_callback_fn_t cbfunc,
				void *cbdata)
{
	return ((LwkOrteRml*)obj)->sendBufferNB( peer, buffer,
						tag, flags, cbfunc, cbdata );
}

int lwkOrteRmlBarrier( void* obj )
{
	return ((LwkOrteRml*)obj)->barrier();
}

int lwkOrteRmlRecvCancel( void* obj, orte_process_name_t * peer,
                               orte_rml_tag_t tag )
{
	return ((LwkOrteRml*)obj)->recvCancel( peer, tag );
}
