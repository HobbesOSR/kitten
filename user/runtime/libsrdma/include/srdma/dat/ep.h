#ifndef srdma_dat_ep_h
#define srdma_dat_ep_h

#include <dat2/udat.h>
#include <srdma/dat/util.h>
#include <srdma/dat/dbg.h>

namespace dat {

class ep {
      public:
	ep(DAT_IA_HANDLE, DAT_PZ_HANDLE, DAT_EVD_HANDLE,
						DAT_EVD_HANDLE, DAT_EP_ATTR &);

	~ep();

	void connect(DAT_IA_ADDRESS_PTR, DAT_CONN_QUAL,
		     DAT_TIMEOUT, DAT_COUNT, const DAT_PVOID, DAT_QOS,
		     DAT_CONNECT_FLAGS flags = DAT_CONNECT_DEFAULT_FLAG);

	void query(DAT_EP_PARAM & params);

	void disconnect(DAT_CLOSE_FLAGS flags = DAT_CLOSE_GRACEFUL_FLAG);

	void post_recv(DAT_COUNT, DAT_LMR_TRIPLET &, DAT_DTO_COOKIE,
		       DAT_COMPLETION_FLAGS flags =
		       DAT_COMPLETION_DEFAULT_FLAG);

	void post_send(DAT_COUNT, DAT_LMR_TRIPLET &, DAT_DTO_COOKIE,
		       DAT_COMPLETION_FLAGS flags =
		       DAT_COMPLETION_DEFAULT_FLAG);

	void post_rdma_read(DAT_COUNT,
			    DAT_LMR_TRIPLET &, DAT_DTO_COOKIE,
			    DAT_RMR_TRIPLET &, DAT_COMPLETION_FLAGS flags =
			    DAT_COMPLETION_DEFAULT_FLAG);

	void post_rdma_write(DAT_COUNT,
			     DAT_LMR_TRIPLET &, DAT_DTO_COOKIE,
			     DAT_RMR_TRIPLET &, DAT_COMPLETION_FLAGS flags =
			     DAT_COMPLETION_DEFAULT_FLAG);

	DAT_EP_HANDLE handle();

      private:
	ep();
	ep(const ep &);
	DAT_EP_HANDLE m_handle;
};

inline ep::ep(DAT_IA_HANDLE ia,
	      DAT_PZ_HANDLE pz,
	      DAT_EVD_HANDLE dto_evd,
	      DAT_EVD_HANDLE conn_evd,
	      DAT_EP_ATTR & ep_attributes)
{
	DAT_RETURN ret = dat_ep_create(ia, pz, dto_evd, dto_evd, conn_evd,
					       &ep_attributes, &m_handle);

	if ( ret != DAT_SUCCESS ) {
		dbgFail( ep, "dat_ep_create() %s\n", errorStr(ret).c_str());
		throw ret;
	}
	dbg(ep,"m_handler=%p\n",m_handle);
}

inline ep::~ep()
{
	dbg(ep,"\n");
	DAT_RETURN ret = dat_ep_free(m_handle);
	if (ret != DAT_SUCCESS) {
		dbgFail( ep, "dat_ep_free() %s\n", errorStr(ret).c_str());
		throw ret;
	}
}

inline void ep::post_recv(DAT_COUNT count, DAT_LMR_TRIPLET & lmr,
			  DAT_DTO_COOKIE cookie, DAT_COMPLETION_FLAGS flags)
{
	dbg(ep,"cookie=%p vaddr=%#lx len=%#x context=%#x\n", cookie.as_ptr,
				lmr.virtual_address,
				lmr.segment_length,
				lmr.lmr_context);
	DAT_RETURN ret = dat_ep_post_recv(m_handle, count, &lmr, cookie, flags);
	if (ret != DAT_SUCCESS) {
		dbgFail( ep, "dat_ep_post_recv() %s\n", errorStr(ret).c_str());
		throw ret;
	}
}

inline void ep::post_send(DAT_COUNT count, DAT_LMR_TRIPLET & lmr,
			  DAT_DTO_COOKIE cookie, DAT_COMPLETION_FLAGS flags)
{
	dbg(ep,"cookie=%p vaddr=%#lx len=%#x context=%#x\n", cookie.as_ptr,
				lmr.virtual_address,
				lmr.segment_length,
				lmr.lmr_context);
	DAT_RETURN ret = dat_ep_post_send(m_handle, count, &lmr, cookie, flags);
	if (ret != DAT_SUCCESS) {
		dbgFail( ep, "dat_ep_post_send() %s\n", errorStr(ret).c_str());
		throw ret;
	}
}

inline void ep::post_rdma_read(DAT_COUNT count,
			       DAT_LMR_TRIPLET & lmr, DAT_DTO_COOKIE cookie,
			       DAT_RMR_TRIPLET & rmr,
			       DAT_COMPLETION_FLAGS flags)
{
	dbg(ep,"cookie=%p vaddr=%#lx len=%#x context=%#x\n", cookie.as_ptr,
				lmr.virtual_address,
				lmr.segment_length,
				lmr.lmr_context);
	dbg(ep,"vaddr=%#lx len=%#x context=%#x\n",
				rmr.virtual_address,
				rmr.segment_length,
				rmr.rmr_context);
	DAT_RETURN ret = dat_ep_post_rdma_read(m_handle, count, &lmr, cookie,
						       &rmr, flags);
	if (ret != DAT_SUCCESS) {
		dbgFail( ep, "dat_ep_post_rdma_read() %s\n",
						errorStr(ret).c_str());
		throw ret;
	}
}

inline void ep::post_rdma_write(DAT_COUNT count,
				DAT_LMR_TRIPLET & lmr, DAT_DTO_COOKIE cookie,
				DAT_RMR_TRIPLET & rmr,
				DAT_COMPLETION_FLAGS flags)
{
	dbg(ep,"cookie=%p vaddr=%#lx len=%#x context=%#x\n", cookie.as_ptr,
				lmr.virtual_address,
				lmr.segment_length,
				lmr.lmr_context);
	dbg(ep,"vaddr=%#lx len=%#x context=%#x\n",
				rmr.virtual_address,
				rmr.segment_length,
				rmr.rmr_context);
	DAT_RETURN ret = dat_ep_post_rdma_write(m_handle, count, &lmr, cookie,
							&rmr, flags);
	if (ret != DAT_SUCCESS) {
		dbgFail( ep, "dat_ep_post_rdma_write() %s\n",
						errorStr(ret).c_str());
		throw ret;
	}
}

inline void ep::connect(DAT_IA_ADDRESS_PTR remote_ia_addr,
			DAT_CONN_QUAL remote_conn_qual,
			DAT_TIMEOUT timeout,
			DAT_COUNT private_data_size,
			const DAT_PVOID private_data,
			DAT_QOS qos, DAT_CONNECT_FLAGS flags)
{
	DAT_RETURN ret = dat_ep_connect(m_handle, remote_ia_addr,
			remote_conn_qual, timeout, private_data_size,
			private_data, qos, flags);
	if (ret != DAT_SUCCESS) {
		dbgFail( ep, "dat_ep_connect() %s\n", errorStr(ret).c_str());
		throw ret;
	}
}

inline void ep::query(DAT_EP_PARAM & params)
{
	DAT_RETURN ret = dat_ep_query(m_handle, DAT_EP_FIELD_ALL, &params);
	if (ret != DAT_SUCCESS) {
		dbgFail( ep, "dat_ep_query() %s\n", errorStr(ret).c_str());
		throw ret;
	}
}

inline void ep::disconnect(DAT_CLOSE_FLAGS flags)
{
	dbg(ep,"flags=%#x\n", flags );
	DAT_RETURN ret = dat_ep_disconnect(m_handle, flags);
	if (ret != DAT_SUCCESS) {
		dbgFail( ep, "dat_ep_disconnect() %s\n", errorStr(ret).c_str());
		throw ret;
	}
}

inline DAT_EP_HANDLE ep::handle()
{
	return m_handle;
}

} // namespace dat
#endif
