#ifndef srdma_dat_functor_h
#define srdma_dat_functor_h

#include <stddef.h>

namespace dat {

template < typename ReturnT, typename Param1T >
class EventHandlerBase {
      public:
	virtual ReturnT operator()(Param1T) = 0;
	virtual ~ EventHandlerBase() {
	}
};

template < typename ConsumerT, typename ReturnT, typename Param1T >
class EventHandler:public EventHandlerBase < ReturnT, Param1T >
{
      private:
	typedef ReturnT(ConsumerT::*PtrMember) (Param1T);

      public:
	EventHandler(ConsumerT * const object, PtrMember member):object(object),
	    member(member) {
	}

	EventHandler(const EventHandler < ConsumerT, ReturnT,
		     Param1T > &e):object(e.object), member(e.member) {
	}

	virtual ReturnT operator() (Param1T param1) {
		return (const_cast < ConsumerT * >(object)->*member) (param1);
	}

      protected:
	EventHandler():
	object(NULL), member(NULL) {
	}

      private:

	ConsumerT * const object;
	const PtrMember member;
};

}				// namespace dat

#endif
