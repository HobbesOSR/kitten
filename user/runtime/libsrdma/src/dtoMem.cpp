#include <srdma/dto.h>
#include <srdma/endPoint.h>

namespace srdma {

int Dto::registerRegion( void*addr, Size length, MemRegion::Id id )
{
	dbg(Dto, "addr=%p length=%lu id=%#x\n", addr, length, id);
	if ( m_regionMap.find( id ) != m_regionMap.end() ) {
                dbgFail(Dto, "addr=%p length=%lu id=%#x\n", addr, length, id);
                return -1;
        }
	m_regionMap[id] = new MemRegion( m_core, addr, length );

	return 0;
}

int Dto::unregisterRegion( MemRegion::Id id )
{
	dbg(Dto, "id=%#x\n", id );
	if ( m_regionMap.find( id ) == m_regionMap.end() ) {
                dbgFail(Dto, "id=%#x\n", id );
                return -1;
        }
	delete m_regionMap[ id ];
	
	m_regionMap.erase( id );
	return 0;
}

int Dto::getRegionEvent( MemRegion::Id id, Event& event )
{
	if ( m_regionMap.find( id ) == m_regionMap.end() ) {
                dbgFail(Dto, "id=%#x\n", id );
                return -1;
        }
	return m_regionMap[id]->popEvent( &event );
}

bool Dto::memEvent( MemRegion::Id id, MemOp op,
                         MemRegion::Addr addr, Size length)
{
        dbg(Dto, "op=%s raddr=%#x:%#lx length=%lu\n",
            op == Write ? "Write" : "read", id, addr, length);

	if ( m_regionMap.find( id ) == m_regionMap.end() ) {
                dbgFail(Dto, "id=%#x\n", id );
                return -1;
        }

        Event *event = new Event;
        event->op = op;
        event->addr = (void*) addr;
        event->length = length;
	
	m_regionMap[id]->pushEvent( event );

	return 0; 
}

int Dto::memOp( EndPoint& ep, MemOp op, MemRegion::Id region,
                        MemRegion::Addr addr, void *local, Size length)
{
        DAT_RMR_TRIPLET rmr;
        DAT_LMR_TRIPLET lmr;
	DAT_LMR_HANDLE  lmr_handle;

        dbg( Dto, "raddr=%#x:%#lx laddr=%p length=%lu\n",
                                        region, addr, local, length );

        if ( ! ep.findRMR( region, rmr ) ) {
		dbgFail(Dto,"ep.findRMR()\n");
                return -1;
        }

	CtlMsgDto& dto = *(CtlMsgDto*) ep.allocDto( );
        
        dbg( Dto, "virtual_address=%#lx\n", rmr.virtual_address );
        dbg( Dto, "segment_length=%d\n", rmr.segment_length );
        dbg( Dto, "rmr_context=%#x\n", rmr.rmr_context );
        
        if ( rmr.virtual_address + addr + length >  
                                rmr.virtual_address + rmr.segment_length ) {
		dbgFail(Dto,"invalid address + length\n");
		ep.freeDto( &dto );
                return -1;
        }

	lmr_handle = m_core.lmrCreate( local, length, lmr );

        rmr.virtual_address += addr;
	rmr.segment_length = lmr.segment_length;
        
        dbg( Dto, "\n");
        if ( op == Write ) {
                ep.post_rdma_write( 1, lmr, dto.cookie(), rmr );
        } else {
                ep.post_rdma_read( 1, lmr, dto.cookie(), rmr );
        }
        
        dbg( Dto, "\n");
	dto.endPoint().wait();
        dbg( Dto, "\n");
        
        m_core.lmrFree( lmr_handle );

	Msg& msg = *(Msg*) dto.lmr_triplet().virtual_address;
        
        msg.hdr.type = MsgHdr::RDMA_RESP;
        msg.hdr.u.rdmaResp.id = region;
	msg.hdr.u.rdmaResp.op = op;
        msg.hdr.u.rdmaResp.addr = addr;
	msg.hdr.u.rdmaResp.length = length;

        dbg( Dto, "raddr=%#x:%#lx length=%lu\n", region, addr, length );

        ep.post_send( dto.num_segments(), dto.lmr_triplet(), dto.cookie() );

	return 0;
}

} // namespace srdma 
