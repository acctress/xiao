#pragma once

#include <memory>
#include <utility>

namespace xiao::support
{
    template < typename T >
    class Box
    {
    public:
        Box( ) : m_ptr { std::make_unique< T >( ) } { }

        explicit Box( T value ) : m_ptr { std::make_unique< T >( std::move( value ) ) } { }

        Box( const Box& other ) : m_ptr { other.m_ptr ? std::make_unique< T >( *other.m_ptr ) : nullptr } { }

        Box( Box&& ) noexcept = default;

        Box& operator= ( Box other ) noexcept
        {
            std::swap( m_ptr, other.m_ptr );
            return *this;
        }

        ~Box( ) = default;

        [[nodiscard]] T& operator* ( ) noexcept
        { return *m_ptr; }

        [[nodiscard]] const T& operator* ( ) const noexcept
        { return *m_ptr; }

        [[nodiscard]] T* operator->( ) noexcept
        { return m_ptr.get( ); }

        [[nodiscard]] const T* operator->( ) const noexcept
        { return m_ptr.get( ); }

    private:
        std::unique_ptr< T > m_ptr;
    };
} // namespace xiao::support
