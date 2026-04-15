#pragma once
#include <vector>
#include <stdexcept>

namespace ImageScraper
{
    /// <summary>
    /// Circular collection with a fixed size
    /// The oldest element is overridden when the collection is already full
    /// </summary>
    /// <typeparam name="T"></typeparam>
    template<typename T>
    class RingBuffer
    {
    public:
        RingBuffer( const std::size_t capacity )
            : m_Capacity{ capacity }
            , m_Buffer{ capacity }
        {
        };

        void Push( T element )
        {
            if( IsFull( ) )
            {
                Pop( );
            }

            m_Buffer[ m_End ] = element;
            m_End = ( m_End + 1 ) % m_Buffer.size( );
            m_Size++;
        }

        void Pop( )
        {
            if( IsEmpty( ) )
            {
                return;
            }

            m_Start = ( m_Start + 1 ) % m_Buffer.size( );
            m_Size--;
        }

        T Front( ) const
        {
            return m_Buffer[ m_Start ];
        }

        T Back( ) const
        {
            return m_Buffer[ ( m_End + m_Buffer.size( ) - 1 ) % m_Buffer.size( ) ];
        }

        bool IsFull( ) const
        {
            return m_Size == m_Buffer.size( );
        }

        bool IsEmpty( ) const
        {
            return m_Size == 0;
        }

        int GetSize( ) const
        {
            return static_cast<int>( m_Size );
        }

        int GetCapacity( ) const
        {
            return static_cast< int >( m_Buffer.size( ) );
        }

        void Clear( )
        {
            m_End = 0;
            m_Start = 0;
            m_Size = 0;
            m_Buffer.clear( );
            m_Buffer.resize( m_Capacity );
        }

        typename std::vector<T>::iterator begin( )
        {
            if( m_Size == 0 )
            {
                return m_Buffer.end( );
            }
            else if( m_End > m_Start )
            {
                return m_Buffer.begin( ) + m_Start;
            }
            else
            {
                return m_Buffer.begin( ) + ( m_Start % m_Buffer.size() );
            }
        }

        typename std::vector<T>::iterator end( )
        {
            if( m_Size == 0 )
            {
                return m_Buffer.end( );
            }
            else if( m_End > m_Start )
            {
                return m_Buffer.begin( ) + m_End;
            }
            else
            {
                return m_Buffer.begin( ) + m_Buffer.size();
            }
        }

        T& operator[]( const int i )
        {
            if( i > m_Size )
            {
                throw std::out_of_range( "Index out of range" );
            }

            return m_Buffer[ ( m_Start + i ) % m_Buffer.size( ) ];
        }

    private:
        std::vector<T> m_Buffer{ };
        std::size_t m_Capacity{ 0 };
        std::size_t m_Size{ 0 };
        std::size_t m_Start{ 0 }; // Index of the oldest element
        std::size_t m_End{ 0 }; // Index of the next "empty" location
    };
}