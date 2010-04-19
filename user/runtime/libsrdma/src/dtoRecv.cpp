#include <srdma/dto.h>
#include <srdma/endPoint.h>
#include <string.h>

namespace srdma {

int Dto::recv_post(Request& req)
{
	dbg(Dto, "id=%#x:%d tag=%#x size=%lu\n", req.key().id.nid,
	    req.key().id.pid, req.key().tag, req.key().count);

	pthread_mutex_lock(&m_lock);

	RecvDto* dto = findRecvDto( req.key() );

	if ( dto ) {

		req.eventDataPtrSet( (void*) dto );

	        Msg& msg = *(Msg*)dto->lmr_triplet().virtual_address;

		req.key().id = dto->endPoint().id();

		dbg(Dto, "nid=%#x pid=%d\n",
				req.key().id.nid, req.key().id.pid);
		dbg(Dto,"body=%p\n",msg.body);

		if ( msg.hdr.u.req.length <= Msg::ShortMsgLength) {
			dbg(Dto, "found short\n");
			memcpy( req.buf(), msg.body, 
				msg.hdr.u.req.length );
			postSe(req);

		} else {
			dbg(Dto, "found long\n");
			dto->setRequest( &req );
			Msg msg = *(Msg*)dto->lmr_triplet().virtual_address;
			dto->endPoint().post_rdma_read(1,
					req.lmr_triplet(),
					dto->cookie(),
					msg.hdr.u.req.rmr_triplet);
		}

	} else {
		m_recvMap.insert(std::make_pair(req.key(), &req));
	}
	pthread_mutex_unlock(&m_lock);
	return 0;
}

} // namespace srdma 
