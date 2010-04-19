#include <srdma/dm.h>
#include <srdma/dbg.h>
#include <string.h>
#include <stdlib.h>

namespace srdma {

bool Dto::event(DAT_DTO_COMPLETION_EVENT_DATA & event)
{
	switch (event.operation) {
	case DAT_DTO_SEND:
		return send_event(event);
	case DAT_DTO_RECEIVE:
		return recv_event(event);
	case DAT_DTO_RDMA_READ:
		return rdma_read_event(event);
	case DAT_DTO_RDMA_WRITE:
		return rdma_write_event(event);
	default:
		dbg(Dto, "DTO_???\n");
		break;
	}
	return false;
}

bool Dto::rdma_read_event(DAT_DTO_COMPLETION_EVENT_DATA & event)
{
	BaseDto*  dto = (BaseDto*) event.user_cookie.as_ptr;
	dbg(Dto,"dto=%p\n",&dto);

        RecvDto* recvDto = dynamic_cast<RecvDto*>( dto );
        if ( recvDto ) {
		dbg(Dto,"recvDto\n");	
		CtlMsgDto& ctlDto = *(CtlMsgDto*)
				recvDto->endPoint().allocDto();

	        Msg& msg = *(Msg*) ctlDto.lmr_triplet().virtual_address;
        	msg.hdr.type = MsgHdr::ACK;     
		msg.hdr.cookie = 
	         ((Msg*) recvDto->lmr_triplet().virtual_address)->hdr.cookie;

        	ctlDto.endPoint().post_send( ctlDto.num_segments(),
                                ctlDto.lmr_triplet(), ctlDto.cookie());
		return postSe( recvDto->request() );
	}

	dto->endPoint().wakeup();

	dbg( Dto,"\n");
	return false;
}

bool Dto::rdma_write_event(DAT_DTO_COMPLETION_EVENT_DATA & event)
{
	dbg(Dto,"\n");
	BaseDto*  dto = (BaseDto*) event.user_cookie.as_ptr;
	dto->endPoint().wakeup();
	return false;
}

bool Dto::postSe( Request& req )
{
	DAT_RETURN ret;

	dbg( Dto, "req=%p\n", &req );
	if ( ! &req ) {
		dbgFail( Dto, "req == NULL\n");
	}

	if ( (ret = dat_evd_post_se( req.evd_handle(), 
				&req.event() ) ) != DAT_SUCCESS) {
		dbgFail(Dto, "dat_evd_post_se() %sx\n", errorStr(ret).c_str() );
		return true;
	}
	return false;
}

bool Dto::send_event(DAT_DTO_COMPLETION_EVENT_DATA & event)
{
	SendDto & dto = *(SendDto *) event.user_cookie.as_ptr;
	Msg& msg = *(Msg*) dto.lmr_triplet().virtual_address;

	dbg(Dto,"dto=%p id=%#x:%d count=%d\n", &dto, dto.endPoint().id().nid,
				 dto.endPoint().id().pid, msg.hdr.count);

	switch ( msg.hdr.type ) {

	case MsgHdr::REQ:
		dbg( Dto, "REQ\n" );
		postSe( dto.request() );
		break;

	case MsgHdr::ACK:
		{
			dbg(Dto, "ACK\n");
			EndPoint& ep = dto.endPoint();
			ep.freeDto( &dto );
		}
		break;

	case MsgHdr::RMR_RESP:
		{
			dbg(Dto, "RMR_RESP\n");
			EndPoint& ep = dto.endPoint();
			ep.freeDto( &dto );
		}
		break;

	case MsgHdr::RDMA_RESP:
		{
			dbg(Dto, "RDMA_RESP\n");
			EndPoint& ep = dto.endPoint();
			ep.freeDto( &dto );
		}
		break;
	default:

		dbgFail(Dto, "invalid msg type %d for send\n", msg.hdr.type );
		exit(-1);
	}
	return true;
}

bool Dto::recv_event(DAT_DTO_COMPLETION_EVENT_DATA & event)
{
	BaseDto& dto = *(BaseDto *) event.user_cookie.as_ptr;
	Msg& msg = *(Msg*) dto.lmr_triplet().virtual_address;
	bool retval = true;

	dto.endPoint().postRecvBuf();

	dbg(Dto,"dto=%p id=%#x:%d count=%d\n", &dto, dto.endPoint().id().nid,
				 dto.endPoint().id().pid, msg.hdr.count);

	switch (msg.hdr.type) {
	case MsgHdr::REQ:
		retval = reqMsg( *static_cast<RecvDto*>(&dto));
		break;

	case MsgHdr::ACK:
		retval =  ackMsg( *static_cast<CtlMsgDto*>(&dto));
		break;

	case MsgHdr::RMR_REQ:
		retval = rmrReqMsg( *static_cast<CtlMsgDto*>(&dto));
		break;

	case MsgHdr::RMR_RESP:
		retval = rmrRespMsg( *static_cast<CtlMsgDto*>(&dto));
		break;

	case MsgHdr::RDMA_RESP:
		retval = rdmaRespMsg( *static_cast<CtlMsgDto*>(&dto));
		break;
	}
	return retval;
}

bool Dto::ackMsg( CtlMsgDto & dto )
{
	Msg& msg = *(Msg*) dto.lmr_triplet().virtual_address;
	dbg(Dto, "cookie=%p\n",msg.hdr.cookie.as_ptr);
	SendDto* sendDto = (SendDto*) msg.hdr.cookie.as_ptr;

	postSe( sendDto->request() );
	
	EndPoint& ep = dto.endPoint();
	ep.freeDto( &dto );

	return false;
}

bool Dto::reqMsg( RecvDto& dto)
{
	Msg& msg = *(Msg*) dto.lmr_triplet().virtual_address;
	Key key;

	key.id = dto.endPoint().id();
	key.tag = msg.hdr.u.req.tag;
	key.count = msg.hdr.u.req.length;
	Request *req;


	pthread_mutex_lock(&m_lock);
	req = reqFind( key );
	dbg( Dto, "msgLen=%ld msgTag=%#x req=%p\n",  key.count, key.tag, req );

	if (req) {
		req->key() = key;
		req->eventDataPtrSet( (void*) &dto );

		if (key.count <= Msg::ShortMsgLength) {
			dbg(Dto,"found short\n");
			memcpy( req->buf(), msg.body, key.count );
			postSe( *req );
		} else {
			dbg(Dto,"found long\n");
			dto.setRequest( req );	
			dto.endPoint().post_rdma_read(1,
					req->lmr_triplet(), 
					dto.cookie(), 
					msg.hdr.u.req.rmr_triplet );
		}
	} else {
		m_rcvdMap.insert(std::make_pair(key, &dto));
	}
	pthread_mutex_unlock(&m_lock);

	return false;
}

bool Dto::keyCmp(const Key & lh, const Key & rh)
{
	if (lh.count > rh.count)
		return false;
	if (lh.tag != (Tag) - 1 && lh.tag != rh.tag)
		return false;
	if (lh.id.nid != (Nid) - 1 && lh.id.nid != rh.id.nid)
		return false;
	if (lh.id.pid != (Pid) - 1 && lh.id.pid != rh.id.pid)
		return false;
	return true;
}

Request *Dto::reqFind(Key & key)
{
	reqMap_t::iterator iter;
	dbg(Dto, "Find nid=%#x pid=%d tag=%#x size=%lu\n",
	    key.id.nid, key.id.pid, key.tag, key.count);

	for (iter = m_recvMap.begin(); iter != m_recvMap.end(); ++iter) {

		dbg(Dto, "   nid=%#x pid=%d tag=%#x size=%lu\n",
		    (*iter).first.id.nid,
		    (*iter).first.id.pid,
		    (*iter).first.tag, (*iter).first.count);

		if (!keyCmp((*iter).first, key))
			continue;

		Request *req = (*iter).second;

		m_recvMap.erase(iter);
		return req;
	}
	return NULL;
}

RecvDto *Dto::findRecvDto( Key & key )
{
	RecvDto* dto = NULL;

        for (dtoMap_t::iterator iter = m_rcvdMap.begin();
             iter != m_rcvdMap.end(); iter++) {
                dbg(Dto, "    nid=%d pid=%d tag=%#x size=%lu\n",
                    (*iter).first.id.nid, (*iter).first.id.pid,
                    (*iter).first.tag, (*iter).first.count);

                if (!keyCmp(key, (*iter).first)) {
                        continue;
                }
                dto = (*iter).second;
                m_rcvdMap.erase(iter);
                break;
        }
	dbg(Dto,"dto=%p\n",dto);

	return dto;
}

bool Dto::rmrReqMsg( CtlMsgDto& dto)
{
	Msg& msg = *(Msg*)dto.lmr_triplet().virtual_address;
	MemRegion::Id id = msg.hdr.u.rmrReq.id;


	msg.hdr.type = MsgHdr::RMR_RESP;

	if ( m_regionMap.find(id) != m_regionMap.end() ) {
		msg.hdr.u.rmrResp.rmr = m_regionMap[id]->rmr_triplet();
	} else {
		msg.hdr.u.rmrResp.rmr.rmr_context = 0;
		dbgFail(Dto,"couldn't find rmr for %d\n",id);
	}
	dbg(Dto, "id=%#x context=%x\n", id, msg.hdr.u.rmrResp.rmr.rmr_context);

	dto.endPoint().post_send(dto.num_segments(), dto.lmr_triplet(),
							dto.cookie());

	return false;
}

bool Dto::rmrRespMsg( CtlMsgDto& dto)
{
	Msg& msg = *(Msg*)dto.lmr_triplet().virtual_address;

	dbg(Dto, "dto=%p context=%#x\n", &dto,
			msg.hdr.u.rmrResp.rmr.rmr_context);

	CtlMsgDto& _dto = *(CtlMsgDto *) msg.hdr.cookie.as_ptr;
	Msg& _msg = *(Msg*)_dto.lmr_triplet().virtual_address;

	_msg.hdr.u.rmrResp.rmr = msg.hdr.u.rmrResp.rmr;
	dbg(Dto, "%#x\n", _msg.hdr.u.rmrResp.rmr.rmr_context);
	dbg(Dto, "%#x\n", _msg.hdr.u.rmrResp.rmr.segment_length);
	dbg(Dto, "%#lx\n", _msg.hdr.u.rmrResp.rmr.virtual_address);
	_dto.endPoint().wakeup();

	EndPoint& ep = dto.endPoint();
	ep.freeDto( &dto );

	return false;
}

bool Dto::rdmaRespMsg( CtlMsgDto& dto)
{
	Msg& msg = *(Msg*)dto.lmr_triplet().virtual_address;

	memEvent( msg.hdr.u.rdmaResp.id, msg.hdr.u.rdmaResp.op,
			msg.hdr.u.rdmaResp.addr, msg.hdr.u.rdmaResp.length );
	EndPoint& ep = dto.endPoint();
	ep.freeDto( &dto );
	return false;
}

} // namespace srdma 
