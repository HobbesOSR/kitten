
#ifndef _eventHandler_h
#define _eventHandler_h

#ifndef NULL
#define NULL (void*)0
#endif

template <typename ReturnT, typename Param1T>
class EventHandlerBase
{
    public:
        virtual ReturnT operator()(Param1T) = 0;
        virtual ~EventHandlerBase() {
    }
};

template <typename ConsumerT, typename ReturnT,typename Param1T>
class EventHandler: public EventHandlerBase<ReturnT,Param1T>
{
    private:
        typedef ReturnT (ConsumerT::*PtrMember)(Param1T);

    public:
        EventHandler( ConsumerT* const object, PtrMember member) :
                object(object), member(member) {
        }

        EventHandler( const EventHandler<ConsumerT,ReturnT,Param1T>& e ) :
                object(e.object), member(e.member) {
        }

        virtual ReturnT operator()(Param1T param1) {
            return (const_cast<ConsumerT*>(object)->*member)(param1);
        }
    
    protected:
        EventHandler() :
            object(NULL), member(NULL)
        { }

    private:

        // CONSTRUCT Data
        ConsumerT* const object;
        const PtrMember  member;
};


template <typename ReturnT, typename Param1T, typename Param2T >
class EventHandlerBase2
{
    public:
        virtual ReturnT operator()(Param1T,Param2T) = 0;
        virtual ~EventHandlerBase2() {
    }
};

template <typename ConsumerT, typename ReturnT,
        typename Param1T, typename Param2T >
class EventHandler2: public EventHandlerBase2<ReturnT,Param1T,Param2T>
{
    private:
        typedef ReturnT (ConsumerT::*PtrMember)(Param1T,Param2T);

    public:
        EventHandler2( ConsumerT* const object, PtrMember member) :
                object(object), member(member) {
        }

        EventHandler2( const EventHandler2<ConsumerT,ReturnT,Param1T,Param2T>& e ) :
                object(e.object), member(e.member) {
        }

        virtual ReturnT operator()(Param1T param1, Param2T param2) {
            return (const_cast<ConsumerT*>(object)->*member)(param1,param2);
        }
    
    protected:
        EventHandler2() :
            object(NULL), member(NULL)
        { }

    private:

        // CONSTRUCT Data
        ConsumerT* const object;
        const PtrMember  member;
};


template < typename ConsumerT,
            typename ReturnT,
            typename EventT,
            typename ArgT >
class EventHandler1Arg: public EventHandler< ConsumerT, ReturnT, EventT >
{
    private:
        typedef ReturnT (ConsumerT::*PtrMember)( EventT, ArgT );

    public:
        EventHandler1Arg( ConsumerT* const object,
                            PtrMember member, ArgT arg ) :
            m_object( object ),
            m_member( member ),
            m_arg( arg )
        { }

        EventHandler1Arg( const EventHandler1Arg< ConsumerT, ReturnT,
                                        EventT, ArgT >& e ) :
            m_object(e.m_object),
            m_member(e.m_member),
            m_arg(e.m_arg)
        { }

        virtual ReturnT operator()( EventT event ) {
            return (const_cast<ConsumerT*>(m_object)->*m_member)(event,m_arg);
        }
    private:
        // CONSTRUCT Data
        ConsumerT* const    m_object;
        const PtrMember     m_member;
        ArgT                m_arg;
};

template < typename ConsumerT,
            typename ReturnT,
            typename EventT,
            typename Arg1T,
            typename Arg2T >
class EventHandler2Arg: public EventHandler< ConsumerT, ReturnT, EventT >
{
    private:
        typedef ReturnT (ConsumerT::*PtrMember)( EventT, Arg1T, Arg2T );

    public:
        EventHandler2Arg( ConsumerT* const object,
                    PtrMember member, Arg1T arg1, Arg2T arg2 ) :
            m_object( object ),
            m_member( member ),
            m_arg1( arg1 ),
            m_arg2( arg2 )
        { }

        EventHandler2Arg( const EventHandler2Arg< ConsumerT, ReturnT,
                                        EventT, Arg1T, Arg2T >& e ) :
            m_object(e.m_object),
            m_member(e.m_member),
            m_arg1(e.m_arg1),
            m_arg2(e.m_arg2)
        { }

        virtual ReturnT operator()( EventT event ) {
            return (const_cast<ConsumerT*>(m_object)->*m_member)
                        (event,m_arg1,m_arg2);
        }
    private:
        // CONSTRUCT Data
        ConsumerT* const    m_object;
        const PtrMember     m_member;
        Arg1T               m_arg1;
        Arg2T               m_arg2;
};

template < typename ConsumerT,
            typename ReturnT,
            typename EventT,
            typename Arg1T,
            typename Arg2T,
            typename Arg3T >
class EventHandler3Arg: public EventHandler< ConsumerT, ReturnT, EventT >
{
    private:
        typedef ReturnT (ConsumerT::*PtrMember)( EventT, Arg1T, Arg2T, Arg3T );

    public:
        EventHandler3Arg( ConsumerT* const object,
                    PtrMember member, Arg1T arg1, Arg2T arg2, Arg3T arg3) :
            m_object( object ),
            m_member( member ),
            m_arg1( arg1 ),
            m_arg2( arg2 ),
            m_arg3( arg3 )
        { }

        EventHandler3Arg( const EventHandler3Arg< ConsumerT, ReturnT,
                                        EventT, Arg1T, Arg2T, Arg3T >& e ) :
            m_object(e.m_object),
            m_member(e.m_member),
            m_arg1(e.m_arg1),
            m_arg2(e.m_arg2),
            m_arg3(e.m_arg3)
        { }

        virtual ReturnT operator()( EventT event ) {
            return (const_cast<ConsumerT*>(m_object)->*m_member)
                        (event,m_arg1,m_arg2,m_arg3);
        }
    private:
        // CONSTRUCT Data
        ConsumerT* const    m_object;
        const PtrMember     m_member;
        Arg1T               m_arg1;
        Arg2T               m_arg2;
        Arg3T               m_arg3;
};

#endif
