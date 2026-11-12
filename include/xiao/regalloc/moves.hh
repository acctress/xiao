#pragma once

#include <xiao/regalloc/allocator.hh>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace xiao::regalloc
{
    enum class EditPrio : uint8_t
    {
        EdgeIn,
        SpillAtDef,
        Split,
        EdgeOut,
    };

    struct Edit
    {
        ProgPoint  point { };
        EditPrio   prio { };
        uint32_t   vreg { };
        Allocation from { };
        Allocation to { };
    };

    struct Move
    {
        Allocation from { };
        Allocation to { };
    };

    [[nodiscard]] inline std::vector< Move > sequence_parallel_moves( std::vector< Move > moves, const Allocation temp )
    {
        std::erase_if( moves,
                       []( const Move& m )
                       {
                           return m.from == m.to;
                       } );

        std::vector< Move > out;
        out.reserve( moves.size( ) + 1 );

        while ( !moves.empty( ) )
        {
            bool progress { false };
            for ( std::size_t k { 0 }; k < moves.size( ); )
            {
                if ( std::ranges::any_of( moves,
                                          [ & ]( const Move& o )
                                          {
                                              return &o != &moves[ k ] && o.from == moves[ k ].to;
                                          } ) )
                {
                    ++k;
                    continue;
                }
                out.push_back( moves[ k ] );
                moves.erase( moves.begin( ) + static_cast< std::ptrdiff_t >( k ) );
                progress = true;
            }

            if ( progress ) continue;

            const auto dst = moves.front( ).to;
            out.push_back( { dst, temp } );
            for ( auto& m : moves )
                if ( m.from == dst ) m.from = temp;
        }

        return out;
    }
} // namespace xiao::regalloc
