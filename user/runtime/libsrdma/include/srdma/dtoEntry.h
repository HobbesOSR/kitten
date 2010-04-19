#ifndef srdma_dtoEntry_H
#define srdma_dtoEntry_H

#include <srdma/request.h>
#include <dat2/dat.h>

namespace srdma {

class EndPoint;

class BaseDto {
    public:
// can we make the contructor protected so only factory can use it?
	BaseDto( EndPoint& ep, DAT_LMR_TRIPLET& lmr ) :
		m_ep(ep),
		m_num_segments( 1 ),
		m_lmr_triplet( lmr )
	{
		m_user_cookie.as_ptr = this;
	}
	virtual ~BaseDto() { 
	}

	EndPoint& endPoint()           { return m_ep; }
	DAT_DTO_COOKIE& cookie()       { return m_user_cookie; }
	DAT_COUNT	num_segments() { return m_num_segments; }
	DAT_LMR_TRIPLET& lmr_triplet() { return m_lmr_triplet; }
	
    private:
	EndPoint& 	m_ep;	
	DAT_DTO_COOKIE  m_user_cookie;
	DAT_COUNT	m_num_segments;
	DAT_LMR_TRIPLET	m_lmr_triplet;
};

class SendDto: public BaseDto {
    public:
	SendDto( EndPoint& ep, Request& req, DAT_LMR_TRIPLET& lmr ) : 
		BaseDto(ep, lmr ),
		m_req( req )
	{
		dbg( SendDto,"dto=%p\n",this);
	}
	~SendDto( ) {
		dbg( SendDto,"dto=%p\n",this);
	}

	Request& request() { return m_req; }

    private:
	Request& 	m_req;
};

class RecvDto: public BaseDto {
    public:
	RecvDto( EndPoint& ep, DAT_LMR_TRIPLET& lmr ) : 
		BaseDto(ep, lmr ),
		m_req( NULL )
	{
		dbg( RecvDto,"dto=%p\n",this);
	}
	~RecvDto( ) {
		dbg( RecvDto,"dto=%p\n",this);
	}
	void setRequest( Request* req ) { m_req = req; }
	Request& request() { return *m_req; }
    private:
	Request* 	m_req;
};

class CtlMsgDto: public BaseDto {
    public:
	CtlMsgDto( EndPoint& ep, DAT_LMR_TRIPLET& lmr ) : 
		BaseDto(ep, lmr )
	{
		dbg( CtlMsgDto,"dto=%p\n",this);
	}
	~CtlMsgDto( ) {
		dbg( CtlMsgDto,"dto=%p\n",this);
	}
};

} // namespace srdma

#endif
