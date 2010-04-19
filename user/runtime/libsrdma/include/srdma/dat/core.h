#ifndef srdma_dat_core_h
#define srdma_dat_core_h

#include <signal.h>
#include <string.h>
#include <string>
#include <pthread.h>
#include <dat2/udat.h>
#include <srdma/dat/util.h>
#include <srdma/dat/functor.h>
#include <srdma/dat/dbg.h>

namespace dat {

class core {

      public:			// functions

	typedef EventHandlerBase < void, DAT_EVENT & >handler_t;
	core();
	~core();
	void setHandler( handler_t* );
	void shutdown();
	bool listen(int port);

	DAT_LMR_HANDLE lmrCreate(void *addr, size_t, DAT_LMR_TRIPLET &);
	int lmrFree(DAT_LMR_HANDLE);

	DAT_IA_HANDLE ia_handle() {
		return m_ia_handle;
	} 

	DAT_PZ_HANDLE pz_handle() {
		return m_pz_handle;
	}

	DAT_EVD_HANDLE dto_evd_handle() {
		return m_dto_evd;
	}

	DAT_EVD_HANDLE conn_evd_handle() {
		return m_conn_evd;
	}

      protected:

      private:			// functions
	core(const core &);
	void evd_free( std::string name, DAT_EVD_HANDLE handle );

	static void* foo2( void* );
	void* foo2( );

      private:			//data
	DAT_IA_HANDLE  m_ia_handle;
	DAT_PZ_HANDLE  m_pz_handle;
	DAT_CNO_HANDLE m_cno_handle;

	DAT_EVD_HANDLE m_cr_evd;
	DAT_EVD_HANDLE m_dto_evd;
	DAT_EVD_HANDLE m_conn_evd;
	DAT_EVD_HANDLE m_async_evd;
	DAT_EVD_HANDLE m_se_evd;

	DAT_EVD_HANDLE m_psp;

	handler_t*     m_handler;
	
	pthread_t	m_thread;
	DAT_EVENT	m_event;
};

inline core::core(): 
	m_async_evd( DAT_HANDLE_NULL ),
	m_handler(NULL)
{
	DAT_RETURN ret;

	ret = dat_ia_open((char *)"ofa-v2-ib0", 1, &m_async_evd, &m_ia_handle);
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_ia_open() %s\n", errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_pz_create(m_ia_handle, &m_pz_handle);
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_pz_create() %s\n", errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_cno_create(ia_handle(), DAT_OS_WAIT_PROXY_AGENT_NULL,
							&m_cno_handle);
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_cno_create() %s\n", errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_evd_modify_cno( m_async_evd, m_cno_handle );
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_evd_modify_cno() %s\n",
							errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_evd_create(ia_handle(), 10, m_cno_handle,
					     DAT_EVD_CR_FLAG, &m_cr_evd);
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_evd_create() %s\n", errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_evd_create(ia_handle(), 10, m_cno_handle,
			     DAT_EVD_DTO_FLAG, &m_dto_evd);
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_evd_create() %s\n", errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_evd_create(ia_handle(), 10, m_cno_handle,
			     DAT_EVD_CONNECTION_FLAG, &m_conn_evd);
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_evd_create() %s\n", errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_evd_create(ia_handle(), 1, m_cno_handle,
			     DAT_EVD_SOFTWARE_FLAG, &m_se_evd);
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_evd_create() %s\n", errorStr(ret).c_str() );
		throw ret;
	}

	if ( pthread_create( &m_thread, NULL, foo2, this ) ) {
		dbgFail( core, "pthread_create()\n" );
		throw -1;
	}

        m_event.event_number = DAT_SOFTWARE_EVENT;
        m_event.event_data.software_event_data.pointer = NULL;
}

inline void core::setHandler( handler_t* handler )
{
	m_handler = handler;
}

inline void core::shutdown()
{
	DAT_RETURN ret;
	dbg(core,"\n");
	
	ret = dat_evd_post_se( m_se_evd, &m_event );
	if ( ret != DAT_SUCCESS ) { 
		dbgFail( core, "dat_evd_post_se() %s\n",
					errorStr(ret).c_str() );
		throw ret;	
	}

	if( pthread_join( m_thread, NULL ) ) {
		dbgFail( core, "pthread_join()\n");
		throw -1;
	}

	ret = dat_evd_modify_cno( m_async_evd, DAT_HANDLE_NULL );
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_evd_mofiy_cno() %s\n",
					errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_evd_modify_cno( m_cr_evd, DAT_HANDLE_NULL );
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_evd_mofiy_cno() %s\n",
					errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_evd_modify_cno( m_dto_evd, DAT_HANDLE_NULL );
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_evd_mofiy_cno() %s\n",
					errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_evd_modify_cno( m_conn_evd, DAT_HANDLE_NULL );
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_evd_mofiy_cno() %s\n",
					errorStr(ret).c_str() );
		throw ret;
	}

	ret = dat_evd_modify_cno( m_se_evd, DAT_HANDLE_NULL );
	if (ret != DAT_SUCCESS) {
		dbgFail( core, "dat_evd_mofiy_cno() %s\n",
					errorStr(ret).c_str() );
		throw ret;
	}

	dbg(core,"\n");
	DAT_EVD_HANDLE evd;
	while ( dat_cno_wait( m_cno_handle, 0, &evd ) == DAT_SUCCESS ) {
		dbg( core, "got event\n");
		DAT_EVENT event;
		DAT_RETURN ret;
		if ( (ret=dat_evd_dequeue( evd, &event)) != DAT_SUCCESS ) {
			dbgFail(core, "dat_evd_dequeue() %s\n",
					errorStr(ret).c_str() );
			throw ret;
		}
	}
	dbg(core,"\n");

	if ( ( ret = dat_cno_free( m_cno_handle ) ) != DAT_SUCCESS ) {
		dbgFail( core, "dat_cno_free() %s\n", 
					errorStr(ret).c_str() );
		throw ret;
	}
	dbg(core,"\n");
}

inline void core::evd_free( std::string name, DAT_EVD_HANDLE handle )
{
	DAT_EVENT event;
	DAT_RETURN ret;
	dbg(core,"%s\n", name.c_str());

	if ( dat_evd_disable( handle ) != DAT_SUCCESS ) {
		dbgFail( core, "dat_evd_disabled() %s\n", 
						errorStr(ret).c_str() );
		throw ret;	
	}

	if ( dat_evd_set_unwaitable( handle ) != DAT_SUCCESS ) {
		dbgFail( core, "dat_evd_setunwaitable() %s\n",
						errorStr(ret).c_str() );
		throw ret;	
	}

	while ( dat_evd_dequeue( handle, & event ) == DAT_SUCCESS ) { 
		dbg(core,"%s, dequeue event\n",name.c_str());
	}

	if ( ( ret = dat_evd_free( handle ) ) != DAT_SUCCESS ) {
		dbgFail( core, "dat_evd_free() %s\n", errorStr(ret).c_str() );
		throw ret;	
	}
}

inline core::~core()
{
	DAT_RETURN ret;
	if ( dat_psp_free( m_psp ) != DAT_SUCCESS ) {
		dbgFail( core, "dat_psp_free() %s\n",
					errorStr(ret).c_str() );
		throw ret;	
	}

	evd_free( "m_cr_evd", m_cr_evd );
	evd_free( "m_dto_evd", m_dto_evd );
	evd_free( "m_conn_evd", m_conn_evd );
	evd_free( "m_se_evd", m_se_evd );

#if 0 
	if ( ( ret = dat_pz_free(m_pz_handle) ) != DAT_SUCCESS )  {
		dbgFail( core, "dat_pz_free() %s\n", errorStr(ret).c_str() );
		throw -1;
	}

	if ( dat_ia_close(m_ia_handle,DAT_CLOSE_GRACEFUL_FLAG) != 
								DAT_SUCCESS ) {
		dbgFail( core, "dat_ia_close() %s\n", errorStr(ret).c_str() );
		throw -1;
	}
#else
	if ( dat_ia_close(m_ia_handle,DAT_CLOSE_ABRUPT_FLAG) != DAT_SUCCESS ) {
		dbgFail( core, "dat_ia_close() %s\n",errorStr(ret).c_str() );
		throw -1;
	}
#endif
}

inline bool core::listen(int port)
{
	DAT_RETURN ret;
	dbg(core, "port=%d\n", port);

	ret = dat_psp_create(ia_handle(), port, m_cr_evd,
			     DAT_PSP_CONSUMER_FLAG, &m_psp);

	if (ret != DAT_SUCCESS) {
		dbgFail(core, "dat_psp_create() %s\n", errorStr(ret).c_str() );
		return true;
	}
	return false;
}

inline void* core::foo2( void* data )
{
	return ((core *) data)->foo2();
}

inline void* core::foo2( )
{
	DAT_RETURN ret;
	DAT_EVENT event;
	DAT_EVD_HANDLE evd;

	while( 1 ) {
		dbg(core,"calling dat_cno_wait()\n");
		ret = dat_cno_wait(m_cno_handle, DAT_TIMEOUT_INFINITE, &evd);

		if (ret != DAT_SUCCESS) {
			dbgFail(core, "dat_cno_wait() %s\n",
						errorStr(ret).c_str() );
			throw ret;
		}

		int nmore = 1;
	    	while ( nmore ) { 
			ret = dat_evd_wait(evd, 0, 1, &event, &nmore);

			if ( DAT_GET_TYPE(ret) == DAT_TIMEOUT_EXPIRED ) {
				dbg(core,"dat_evd_wait() expired\n");
				break;
			}

			if (ret != DAT_SUCCESS) {
				dbgFail(core, "dat_evd_dequeue() %s\n",
							errorStr(ret).c_str() );
				throw ret;
			}

			dbg(core, "event %#x %s nmore=%d\n", event.event_number,
				eventStr(event.event_number).c_str(), nmore);

			if ( evd == m_se_evd ) {
				dbg( core, "returning\n" );
				return NULL;
			}

			(*m_handler)(event);
	    	}
	}
}

inline DAT_LMR_HANDLE core::lmrCreate(void *addr, size_t size,
				    DAT_LMR_TRIPLET & lmr)
{
	DAT_MEM_PRIV_FLAGS flags;
	DAT_REGION_DESCRIPTION region;
	DAT_RETURN ret;
	DAT_LMR_HANDLE lmr_handle;

	dbg(EndPoint, "addr=%p size=%lu\n", addr, size);
	region.for_va = addr;

	lmr.virtual_address = (DAT_VADDR) addr;
	lmr.segment_length = size;

	flags = (DAT_MEM_PRIV_FLAGS)
	    (DAT_MEM_PRIV_LOCAL_WRITE_FLAG | DAT_MEM_PRIV_LOCAL_READ_FLAG |
	     DAT_MEM_PRIV_REMOTE_READ_FLAG | DAT_MEM_PRIV_REMOTE_WRITE_FLAG);

	ret = dat_lmr_create( ia_handle(),
				DAT_MEM_TYPE_VIRTUAL,
				region,
				size,
				pz_handle(),
				flags,
				DAT_VA_TYPE_VA,
				&lmr_handle,
				&lmr.lmr_context,
				NULL,
				NULL,
				NULL);

	if (ret != DAT_SUCCESS) {
		dbgFail(core, "dat_lmr_create() %s\n", errorStr(ret).c_str() );
		return 0;
	}

	return lmr_handle;
}
inline int core::lmrFree( DAT_LMR_HANDLE handle )
{
	return dat_lmr_free( handle );
}

} // namespace dat

#endif
