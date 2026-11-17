#pragma once

#include <cstddef>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace isagen
{
    class Writer
    {
    public:
        template < typename... Args >
        void line( const std::format_string< Args... > fmt, Args&&... args )
        {
            indent( );
            std::format_to( std::back_inserter( m_out ), fmt, std::forward< Args >( args )... );
            m_out += '\n';
        }

        void raw( const std::string_view text )
        {
            indent( );
            m_out += text;
            m_out += '\n';
        }

        void blank( ) { m_out += '\n'; }

        void label( const std::string_view text )
        {
            --m_depth;
            raw( text );
            ++m_depth;
        }

        template < typename Body >
        void indented( Body&& body )
        {
            ++m_depth;
            std::forward< Body >( body )( );
            --m_depth;
        }

        template < typename Body >
        void block( const std::string_view head, Body&& body, const std::string_view tail = { } )
        {
            raw( head );
            raw( "{" );
            indented( std::forward< Body >( body ) );
            indent( );
            m_out += '}';
            m_out += tail;
            m_out += '\n';
        }

        [[nodiscard]] std::string take( ) && { return std::move( m_out ); }

    private:
        void indent( ) { m_out.append( static_cast< std::size_t >( m_depth ) * 4, ' ' ); }

        std::string m_out { };
        int         m_depth { };
    };
} // namespace isagen
