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
        RingBuffer( int capacity )
            : m_Buffer{ capacity }
        {
        };

        void Push( T element )
        {
            if( IsFull( ) )
            {
                Pop( );
            }

            m_Buffer[ m_Tail ] = element;
            m_Tail = ( m_Tail + 1 ) % m_Buffer.size( );
            m_Size++;
        }

        void Pop( )
        {
            if( IsEmpty( ) )
            {
                return;
            }

            m_Head = ( m_Head + 1 ) % m_Buffer.size( );
            m_Size--;
        }

        T Front( ) const
        {
            return m_Buffer[ m_Head ];
        }

        T Back( ) const
        {
            return m_Buffer[ ( m_Tail + m_Buffer.size( ) - 1 ) % m_Buffer.size( ) ];
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

        T& operator[]( const int i )
        {
            if( i > m_Size )
            {
                throw std::out_of_range( "Index out of range" );
            }

            return m_Buffer[ ( m_Head + i ) % m_Buffer.size( ) ];
        }

    private:
        std::vector<T> m_Buffer{ };
        std::size_t m_Size{ 0 };
        std::size_t m_Head{ 0 }; // Index of the oldest element
        std::size_t m_Tail{ 0 }; // Index of the next "empty" location
    };
}