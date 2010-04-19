#ifndef srdma_lmrPool_h
#define srdma_lmrPool_h

#include <vector>
#include <pthread.h>
#include <deque>
#include <srdma/dbg.h>
#include <srdma/dat/core.h>
#include <srdma/dat/util.h>

namespace srdma {

using namespace  dat;

class LmrPool {
    public:
	LmrPool( dat::core& core, size_t chunkLen );
	~LmrPool();
	DAT_LMR_TRIPLET alloc( );
	void free( DAT_LMR_TRIPLET& );

    private:
	std::deque<DAT_LMR_TRIPLET>	m_lmrQ;
        DAT_REGION_DESCRIPTION 		m_region;
	std::vector<unsigned char>	m_data;

	dat::core&	m_core;
        DAT_LMR_HANDLE 	m_lmr_handle;
	int		m_count;

	pthread_mutex_t           m_lock;
};

inline LmrPool::LmrPool( dat::core& core, size_t chunkLen ) :
	m_core( core ),
	m_count( 0 )
{
        DAT_RETURN ret;
	DAT_LMR_TRIPLET lmr;
        int count = 30;
        int length = chunkLen * count;

	pthread_mutex_init(&m_lock, NULL);

        dbg(LmrPool, "chunkLen=%lu\n", chunkLen );

	lmr.segment_length = chunkLen;

	m_data.resize( length );
        m_region.for_va = &m_data.at(0);

        ret = dat_lmr_create(m_core.ia_handle(),
                             DAT_MEM_TYPE_VIRTUAL,
                             m_region,
                             length,
                             m_core.pz_handle(),
                             DAT_MEM_PRIV_LOCAL_WRITE_FLAG,
                             DAT_VA_TYPE_VA,
                             &m_lmr_handle,
                             &lmr.lmr_context,
			     NULL,
			     NULL,
			     NULL );

        if (ret != DAT_SUCCESS) {
                dbgFail(LmrPool, "dat_lmr_create() %s\n",
						errorStr(ret).c_str() );
                throw ret;
        }

        for (int i = 0; i < count; i++) {
		lmr.virtual_address = (unsigned long) 
				m_region.for_va + (i * lmr.segment_length);
		dbg(LmrPool,"chunk address %#lx\n",
				(unsigned long) lmr.virtual_address);
		m_lmrQ.push_back( lmr );
        }
}
inline LmrPool::~LmrPool()
{
	DAT_RETURN ret;	
        dbg(LmrPool, "pid=%d\n", getpid() );
	ret = dat_lmr_free( m_lmr_handle );
}

inline DAT_LMR_TRIPLET LmrPool::alloc(  )
{
	DAT_LMR_TRIPLET lmr;
	pthread_mutex_lock(&m_lock);
	if ( m_lmrQ.empty() ) {
		dbgFail(LmrPool,"empty\n");
		throw -1;
	}
	++m_count;
	lmr = m_lmrQ.front();
        dbg(LmrPool, "count=%d addr=%#lx\n", m_count, lmr.virtual_address );
	m_lmrQ.pop_front();
	pthread_mutex_unlock(&m_lock);
	return lmr;
}
inline void LmrPool::free( DAT_LMR_TRIPLET& lmr )
{
	pthread_mutex_lock( &m_lock );
	--m_count;
        dbg(LmrPool, "count=%d addr=%#lx\n", m_count, lmr.virtual_address );
	m_lmrQ.push_back( lmr ); 
	pthread_mutex_unlock( &m_lock );
}

} // namespace srdma

#endif
