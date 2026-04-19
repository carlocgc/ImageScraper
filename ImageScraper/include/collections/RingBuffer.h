#pragma once

#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <vector>

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
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;

        template<bool IsConst>
        class IteratorBase
        {
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using reference = std::conditional_t<IsConst, const T&, T&>;
            using pointer = std::conditional_t<IsConst, const T*, T*>;

            IteratorBase( ) = default;

            template<bool OtherConst, typename = std::enable_if_t< IsConst || !OtherConst >>
            IteratorBase( const IteratorBase< OtherConst >& other )
                : m_Buffer{ other.m_Buffer }
                , m_Index{ other.m_Index }
            {
            }

            reference operator*( ) const
            {
                return m_Buffer->At( m_Index );
            }

            pointer operator->( ) const
            {
                return &m_Buffer->At( m_Index );
            }

            IteratorBase& operator++( )
            {
                ++m_Index;
                return *this;
            }

            IteratorBase operator++( int )
            {
                IteratorBase copy{ *this };
                ++( *this );
                return copy;
            }

            IteratorBase& operator--( )
            {
                --m_Index;
                return *this;
            }

            IteratorBase operator--( int )
            {
                IteratorBase copy{ *this };
                --( *this );
                return copy;
            }

            bool operator==( const IteratorBase& other ) const
            {
                return m_Buffer == other.m_Buffer && m_Index == other.m_Index;
            }

            bool operator!=( const IteratorBase& other ) const
            {
                return !( *this == other );
            }

        private:
            using BufferType = std::conditional_t< IsConst, const RingBuffer, RingBuffer >;

            IteratorBase( BufferType* buffer, size_type index )
                : m_Buffer{ buffer }
                , m_Index{ index }
            {
            }

            BufferType* m_Buffer{ nullptr };
            size_type   m_Index{ 0 };

            friend class RingBuffer;
            template<bool>
            friend class IteratorBase;
        };

        using iterator = IteratorBase<false>;
        using const_iterator = IteratorBase<true>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        RingBuffer( const size_type capacity )
            : m_Capacity{ capacity }
            , m_Buffer( capacity )
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

        void RemoveAt( int index )
        {
            if( index < 0 || index >= static_cast<int>( m_Size ) )
            {
                return;
            }

            for( size_type i = static_cast<size_type>( index ); i + 1 < m_Size; ++i )
            {
                m_Buffer[ ToBufferIndex( i ) ] = m_Buffer[ ToBufferIndex( i + 1 ) ];
            }

            m_End = ( m_End + m_Buffer.size( ) - 1 ) % m_Buffer.size( );
            m_Size--;
        }

        reference Front( )
        {
            return At( 0 );
        }

        const_reference Front( ) const
        {
            return At( 0 );
        }

        reference Back( )
        {
            return At( m_Size - 1 );
        }

        const_reference Back( ) const
        {
            return At( m_Size - 1 );
        }

        bool IsFull( ) const
        {
            return m_Size == m_Buffer.size( );
        }

        bool IsEmpty( ) const
        {
            return m_Size == 0;
        }

        size_type Size( ) const
        {
            return m_Size;
        }

        int GetSize( ) const
        {
            return static_cast<int>( Size( ) );
        }

        size_type Capacity( ) const
        {
            return m_Buffer.size( );
        }

        int GetCapacity( ) const
        {
            return static_cast<int>( Capacity( ) );
        }

        void Clear( )
        {
            m_End = 0;
            m_Start = 0;
            m_Size = 0;
            m_Buffer.clear( );
            m_Buffer.resize( m_Capacity );
        }

        iterator begin( )
        {
            return iterator{ this, 0 };
        }

        const_iterator begin( ) const
        {
            return cbegin( );
        }

        const_iterator cbegin( ) const
        {
            return const_iterator{ this, 0 };
        }

        iterator end( )
        {
            return iterator{ this, m_Size };
        }

        const_iterator end( ) const
        {
            return cend( );
        }

        const_iterator cend( ) const
        {
            return const_iterator{ this, m_Size };
        }

        reverse_iterator rbegin( )
        {
            return reverse_iterator{ end( ) };
        }

        const_reverse_iterator rbegin( ) const
        {
            return crbegin( );
        }

        const_reverse_iterator crbegin( ) const
        {
            return const_reverse_iterator{ cend( ) };
        }

        reverse_iterator rend( )
        {
            return reverse_iterator{ begin( ) };
        }

        const_reverse_iterator rend( ) const
        {
            return crend( );
        }

        const_reverse_iterator crend( ) const
        {
            return const_reverse_iterator{ cbegin( ) };
        }

        T& operator[]( const int i )
        {
            if( i < 0 || i >= static_cast<int>( m_Size ) )
            {
                throw std::out_of_range( "Index out of range" );
            }

            return At( static_cast<size_type>( i ) );
        }

        const T& operator[]( const int i ) const
        {
            if( i < 0 || i >= static_cast<int>( m_Size ) )
            {
                throw std::out_of_range( "Index out of range" );
            }

            return At( static_cast<size_type>( i ) );
        }

    private:
        reference At( size_type index )
        {
            if( index >= m_Size )
            {
                throw std::out_of_range( "Index out of range" );
            }

            return m_Buffer[ ToBufferIndex( index ) ];
        }

        const_reference At( size_type index ) const
        {
            if( index >= m_Size )
            {
                throw std::out_of_range( "Index out of range" );
            }

            return m_Buffer[ ToBufferIndex( index ) ];
        }

        size_type ToBufferIndex( size_type logicalIndex ) const
        {
            return ( m_Start + logicalIndex ) % m_Buffer.size( );
        }

        std::vector<T> m_Buffer{ };
        size_type m_Capacity{ 0 };
        size_type m_Size{ 0 };
        size_type m_Start{ 0 }; // Index of the oldest element
        size_type m_End{ 0 }; // Index of the next "empty" location
    };
}
