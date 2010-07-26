#ifndef _pct_log_h
#define _pct_log_h

#include <stdio.h>
#include <cstdarg>
#include <string>
#include <deque>
#include <pthread.h>

template < int ENABLE = 1 >
class Log {
    public:
        Log( std::string prefix = "", bool enable = true ) :
            m_prefix( prefix ), m_enabled( enable )
        {
            pthread_mutex_init(&m_mutex,NULL);
        }
    
        void enable() {
            m_enabled = true;
        }
        void disable() {
            m_enabled = false;
        }

        void prepend( std::string str ) {
            m_prefix = str + m_prefix; 
        }

        void write( const std::string fmt, ... )
        {
            if ( ENABLE ) {

                if ( ! m_enabled ) return;

                std::string buf;
                buf.resize(512);

                va_list ap;
                va_start( ap,fmt );
                vsnprintf( &buf[0], 512, fmt.c_str(), ap ); 
                va_end( ap);
                pthread_mutex_lock(&m_mutex);
                m_queue.push_back( m_prefix + buf );
                pthread_mutex_unlock(&m_mutex);
            }
        }

        void dump() {
            while ( ! m_queue.empty() ) {
                printf("%s",m_queue.front().c_str());
                m_queue.pop_front();
            }
        }

    private:
        std::string m_prefix;
        bool        m_enabled;
        std::deque<std::string> m_queue;
        pthread_mutex_t         m_mutex;
};

#endif
