#ifndef srdma_request_h
#define srdma_request_h

#include <srdma/types.h>
#include <srdma/dat/core.h>
#include <srdma/dat/util.h>
#include <srdma/msgHdr.h>

namespace srdma {

class Request {
    public:
	Request() :
		m_evd_handle( DAT_HANDLE_NULL ),
		m_lmr_handle( DAT_HANDLE_NULL )
	{
                m_event.event_number = DAT_SOFTWARE_EVENT;
                m_event.event_data.software_event_data.pointer = NULL;
	}

	Request& init( dat::core& core, void* buf, Size length,
							ProcId& id, Tag tag )
	{
		DAT_RETURN ret;
        	m_key.count = length;
	        m_key.id = id;
	        m_key.tag = tag;
		m_buf = buf;

		dbg(Request,"this=%p buf=%p length=%lu tag=%#x\n",
					this, buf, length, tag );
        	ret = dat_evd_create( core.ia_handle(), 1, DAT_HANDLE_NULL,
			DAT_EVD_SOFTWARE_FLAG, &m_evd_handle );
		if ( ret != DAT_SUCCESS ) {
			dbgFail( Request, "dat_evd_create() %s\n",
					dat::errorStr(ret).c_str() );
			throw -1;
        	}
		if ( length > Msg::ShortMsgLength ) {
			initLmr( core );
		}
		return *this;
	}

	~Request()
	{
		if ( m_evd_handle != DAT_HANDLE_NULL ) {
        		if ( dat_evd_free( m_evd_handle ) != DAT_SUCCESS ) {
                		throw -1;
        		}
		}
		if ( m_lmr_handle != DAT_HANDLE_NULL ) {
        		if ( dat_lmr_free( m_lmr_handle ) != DAT_SUCCESS ) {
                		throw -1;
        		}
		}
	}

	void initLmr( dat::core& core )
	{
		DAT_MEM_PRIV_FLAGS flags;
		DAT_REGION_DESCRIPTION region;
        	DAT_RETURN ret;

		region.for_va = (void*) m_buf;

		flags = (DAT_MEM_PRIV_FLAGS)
            			(DAT_MEM_PRIV_LOCAL_WRITE_FLAG |
				DAT_MEM_PRIV_LOCAL_READ_FLAG |
				DAT_MEM_PRIV_REMOTE_READ_FLAG |
				DAT_MEM_PRIV_REMOTE_WRITE_FLAG);

		m_rmr_triplet.virtual_address = (DAT_VADDR) m_buf;
		m_rmr_triplet.segment_length = m_key.count;
		m_lmr_triplet.virtual_address = (DAT_VADDR) m_buf;
		m_lmr_triplet.segment_length = m_key.count;

		ret = dat_lmr_create(core.ia_handle(),
					DAT_MEM_TYPE_VIRTUAL,
					region,
					m_key.count,
					core.pz_handle(),
					flags,
					DAT_VA_TYPE_VA,
					&m_lmr_handle,
					&m_lmr_triplet.lmr_context,
					&m_rmr_triplet.rmr_context,
					NULL,
					NULL);
		if ( ret != DAT_SUCCESS ) {
			dbgFail( Request, "dat_lmr_create() %s\n",
					dat::errorStr(ret).c_str() );
			throw -1;
        	}
		dbg( Request, "addr=%#lx length=%d context=%#x\n",
					m_rmr_triplet.virtual_address,
					m_rmr_triplet.segment_length,
					m_rmr_triplet.rmr_context );
	}

	Key& key() {
		return m_key;
	}

	void* buf() {
		return m_buf;
	}

	DAT_EVD_HANDLE evd_handle() {
		return m_evd_handle;
	}

	DAT_EVENT& event() {
		return m_event;
	}

	DAT_RMR_TRIPLET& rmr_triplet() {
		return m_rmr_triplet;	
	}

	DAT_LMR_TRIPLET& lmr_triplet() {
		return m_lmr_triplet;	
	}

	void eventDataPtrSet( void* ptr ) { 
		m_event.event_data.software_event_data.pointer = ptr;
	}

    private:
	void*		m_buf;
	Key 		m_key;
	DAT_EVD_HANDLE  m_evd_handle;
	DAT_EVENT	m_event;
	DAT_LMR_HANDLE  m_lmr_handle;
	DAT_RMR_TRIPLET m_rmr_triplet;
	DAT_LMR_TRIPLET m_lmr_triplet;
};

} // namespace srdma

#endif
