#pragma once

#include <xiao/ir/function.hh>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace xiao::ir
{
    class DomTree
    {
    public:
        static constexpr uint32_t NONE = ~0u;

        explicit DomTree( const IRFunction& fn )
        {
            const auto n = static_cast< uint32_t >( fn.blocks.size( ) );

            m_rpo.assign( n, 0 );
            m_idom.assign( n, NONE );

            if ( n == 0 ) return;

            const auto succ = [ & ]( const IRBlock& block, const uint32_t k ) -> uint32_t
            {
                const auto at = static_cast< std::size_t >( block.succs_offset ) + k;
                return at < fn.succs.size( ) ? fn.succs[ at ].id : NONE;
            };

            std::vector< std::vector< uint32_t > > preds( n );
            for ( uint32_t b { }; b < n; ++b )
                for ( uint8_t k { }; k < fn.blocks[ b ].succs_count; ++k )
                    if ( const auto s = succ( fn.blocks[ b ], k ); s < n ) preds[ s ].push_back( b );

            std::vector< uint32_t >                       po; //! post order
            std::vector< bool >                           seen( n );
            std::vector< std::pair< uint32_t, uint8_t > > stack {
                { 0, 0 }
            };

            seen[ 0 ] = true;
            while ( !stack.empty( ) )
            {
                auto& [ b, k ] = stack.back( );

                if ( const auto& block = fn.blocks[ b ]; k < block.succs_count )
                {
                    if ( const auto s = succ( block, k++ ); s < n && !seen[ s ] )
                    {
                        seen[ s ] = true;
                        stack.push_back( { s, 0 } );
                    }

                    continue;
                }

                po.push_back( b );
                stack.pop_back( );
            }

            const auto reached = static_cast< uint32_t >( po.size( ) );
            for ( uint32_t i { }; i < reached; ++i ) m_rpo[ po[ i ] ] = reached - i;

            m_idom[ 0 ] = 0;
            for ( bool changed { true }; changed; )
            {
                changed = false;
                for ( auto iter = po.rbegin( ) + 1; iter != po.rend( ); ++iter )
                {
                    uint32_t next = NONE;
                    for ( const auto p : preds[ *iter ] )
                        if ( m_idom[ p ] != NONE ) next = next == NONE ? p : intersect( p, next );

                    if ( next != m_idom[ *iter ] )
                    {
                        m_idom[ *iter ] = next;
                        changed         = true;
                    }
                }
            }
        }

        [[nodiscard]] bool is_reachable( const uint32_t b ) const noexcept
        { return b < m_rpo.size( ) && m_rpo[ b ] != 0; }

        [[nodiscard]] uint32_t idom( const uint32_t b ) const noexcept
        { return m_idom[ b ]; }

        [[nodiscard]] bool block_dominates( const uint32_t a, uint32_t b ) const noexcept
        {
            if ( a == b ) return true;
            if ( !is_reachable( a ) || !is_reachable( b ) ) return false;
            while ( m_rpo[ b ] > m_rpo[ a ] ) b = m_idom[ b ];
            return a == b;
        }

    private:
        std::vector< uint32_t > m_rpo;
        std::vector< uint32_t > m_idom;

        [[nodiscard]] uint32_t intersect( uint32_t a, uint32_t b ) const noexcept
        {
            while ( a != b )
            {
                while ( m_rpo[ a ] > m_rpo[ b ] ) a = m_idom[ a ];
                while ( m_rpo[ b ] > m_rpo[ a ] ) b = m_idom[ b ];
            }

            return a;
        }
    };
} // namespace xiao::ir