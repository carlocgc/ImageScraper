//#pragma once
//#include <mutex>
//#include <condition_variable>
//
//template<typename ValueType, typename ErrorType>
//class Promise
//{
//public:
//    Promise( ) : m_State( State::Pending ) { };
//    ~Promise( ) = default;
//
//    void Resolve( const ValueType& value )
//    {
//        std::lock_guard<std::mutex> lock( m_Mutex );
//        if( m_State == State::Pending )
//        {
//            m_Value = value;
//            m_State = State::Resolved;
//            m_Condition.notify_one( );
//        }
//    }
//
//    void Reject( const ErrorType& error )
//    {
//        std::lock_guard<std::mutex> lock( m_Mutex );
//        if( m_State == State::Pending )
//        {
//            m_Error = error;
//            m_State = State::Rejected;
//            m_Condition.notify_one( );
//        }
//    }
//
//    template<typename V, typename E>
//    Promise<V, E> Then( std::function<V( const ValueType& )> onResolve, std::function<E( const ErrorType& )> onReject )
//    {
//        std::unique_lock<std::mutex> lock( m_Mutex );
//        while( m_State == State::Pending )
//        {
//            // Blocks
//            m_Condition.wait( lock );
//        }
//
//        if( m_State == State::Resolved )
//        {
//            V result = onResolve( m_Value );
//            return Promise<V>( result );
//        }
//        else
//        {
//            E err = onReject( m_Error );
//            return Promise<E>( m_Error );
//        }
//    }
//
//private:
//    enum class State : uint8_t
//    {
//        Pending,
//        Resolved,
//        Rejected
//    };
//
//    Promise( const ValueType& value ) : m_Value( value ), m_State( State::Resolved ) { }
//    Promise( const ErrorType& error ) : m_Error( error ), m_State( State::Rejected ) { }
//
//    ValueType m_Value;
//    ErrorType m_Error;
//    State m_State;
//    std::mutex m_Mutex;
//    std::condition_variable m_Condition;
//};
//
