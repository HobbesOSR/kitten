#ifndef srdma_dtoFactory_H
#define srdma_dtoFactory_H

#include <srdma/lmrPool.h>
#include <srdma/dtoEntry.h>

namespace srdma {

class DtoFactory {
    public:
	DtoFactory( dat::core& core, int len ) :
		m_msgPool( core, len )
	{
	}

	CtlMsgDto* allocCtlMsg( EndPoint& ep ) {
		DAT_LMR_TRIPLET lmr = m_msgPool.alloc();
		return new CtlMsgDto( ep, lmr );
	}

	SendDto* allocSend( EndPoint& ep, Request& req ) {
		DAT_LMR_TRIPLET lmr = m_msgPool.alloc();
		return new SendDto( ep, req, lmr );
	}

	RecvDto* allocRecv( EndPoint& ep ) {
		DAT_LMR_TRIPLET lmr = m_msgPool.alloc();
		return new RecvDto( ep, lmr );
	}

	void free( BaseDto* dto ) {
		dbg(DtoFactory,"%p\n",dto);
		m_msgPool.free( dto->lmr_triplet() );
		delete dto; 
	} 
    private:
        LmrPool                 m_msgPool;
};

} // namespace srdma

#endif
