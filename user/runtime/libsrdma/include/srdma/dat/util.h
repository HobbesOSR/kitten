
#ifndef srdma_dat_util_h
#define srdma_dat_util_h

#include <string>
#include <dat2/udat.h>

namespace dat {

inline std::string errorStr( int error ) 
{ 
	const char* major, * minor;
	dat_strerror( error, &major, &minor );
	std::string ret = major;
	ret += " ";
	ret += minor;
	return ret;
}


inline std::string eventStr(int event)
{
	switch (event) {
	case 0x00001:
		return "DAT_DTO_COMPLETION_EVENT";
	case 0x01001:
		return "DAT_RMR_BIND_COMPLETION_EVENT";
	case 0x02001:
		return "DAT_CONNECTION_REQUEST_EVENT";
	case 0x04001:
		return "DAT_CONNECTION_EVENT_ESTABLISHED";
	case 0x04002:
		return "DAT_CONNECTION_EVENT_PEER_REJECTED";
	case 0x04003:
		return "DAT_CONNECTION_EVENT_NON_PEER_REJECTED";
	case 0x04004:
		return "DAT_CONNECTION_EVENT_ACCEPT_COMPLETION_ERROR";
	case 0x04005:
		return "DAT_CONNECTION_EVENT_DISCONNECTED";
	case 0x04006:
		return "DAT_CONNECTION_EVENT_BROKEN";
	case 0x04007:
		return "DAT_CONNECTION_EVENT_TIMED_OUT";
	case 0x04008:
		return "DAT_CONNECTION_EVENT_UNREACHABLE";
	case 0x08001:
		return "DAT_ASYNC_ERROR_EVD_OVERFLOW";
	case 0x08002:
		return "DAT_ASYNC_ERROR_IA_CATASTROPHIC";
	case 0x08003:
		return "DAT_ASYNC_ERROR_EP_BROKEN";
	case 0x08004:
		return "DAT_ASYNC_ERROR_TIMED_OUT";
	case 0x08005:
		return "DAT_ASYNC_ERROR_PROVIDER_INTERNAL_ERROR";
	case 0x08101:
		return "DAT_HA_DOWN_TO_1";
	case 0x08102:
		return "DAT_HA_UP_TO_MULTI_PATH";
	case 0x10001:
		return "DAT_SOFTWARE_EVENT";
	case 0x20000:
		return "DAT_EXTENSION_EVENT";
	case 0x40000:
		return "DAT_IB_EXTENSION_EVENT";
	default:
		return "UNKOWN EVENT";
	}
}   

}				// namespace dat

#endif
