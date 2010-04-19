#include <srdma/dto.h>
#include <srdma/endPoint.h>
#include <string.h>

namespace srdma {

int Dto::send_post( EndPoint& ep, Request & req)
{
	DAT_COMPLETION_FLAGS flags = DAT_COMPLETION_DEFAULT_FLAG;

	dbg(Dto, "enter\n");

	SendDto& dto = * (SendDto*) ep.allocDto( &req );
	
	req.eventDataPtrSet( (void*) &dto );

	Msg& msg = *(Msg*)dto.lmr_triplet().virtual_address;

	msg.hdr.type = MsgHdr::REQ;
	msg.hdr.u.req.tag = req.key().tag;
	msg.hdr.u.req.length = req.key().count;

	if ( req.key().count <= Msg::ShortMsgLength ) {
		memcpy( msg.body, req.buf(), req.key().count ); 
	} else {
		msg.hdr.cookie = dto.cookie();
		msg.hdr.u.req.rmr_triplet = req.rmr_triplet();
		flags = DAT_COMPLETION_SUPPRESS_FLAG;
	}

	dto.endPoint().post_send( dto.num_segments(), dto.lmr_triplet(),
				  dto.cookie(), flags );
	return 0;
}

}                             // namespace srdma 
