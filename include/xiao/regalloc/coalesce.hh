#pragma once

#include <xiao/regalloc/ranges.hh>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace xiao::regalloc
{
    struct Piece
    {
        uint32_t  vreg { };
        LiveRange range { };
    };

    struct Bundle
    {
        std::vector< Piece >                             pieces { };
        LiveRange                                        range { };
        codegen::RegClass                                cls { };
        std::vector< Mention >                           mentions { };
        std::vector< std::pair< codegen::PReg, float > > hints { };
    };

    struct Bundles
    {
        static constexpr uint32_t none = ~0u;

        std::vector< uint32_t > bundle_of { };
        std::vector< Bundle >   bundles { };
        uint32_t                merged_copies { };

        [[nodiscard]] static Bundles build( const LiveRanges& lr )
        {
            const auto n = static_cast< uint32_t >( lr.vregs.size( ) );

            std::vector< uint32_t > parent( n );
            std::iota( parent.begin( ), parent.end( ), 0u );

            const auto find = [ & ]( uint32_t v )
            {
                while ( parent[ v ] != v )
                {
                    parent[ v ] = parent[ parent[ v ] ];
                    v           = parent[ v ];
                }
                return v;
            };

            std::vector< LiveRange > root_range( n );
            for ( uint32_t v { codegen::Reg::pinned_count }; v < n; ++v ) root_range[ v ] = lr.vregs[ v ].range;

            auto copies = lr.copies;
            std::ranges::stable_sort( copies, std::ranges::greater { }, &CopyEdge::weight );

            Bundles out;

            for ( const auto& c : copies )
            {
                if ( c.dst >= n || c.src >= n ) continue;

                const auto a = find( c.dst );
                const auto b = find( c.src );
                if ( a == b ) continue;
                if ( lr.vregs[ a ].cls != lr.vregs[ b ].cls ) continue;
                if ( root_range[ a ].empty( ) || root_range[ b ].empty( ) ) continue;
                if ( root_range[ a ].overlaps( root_range[ b ] ) ) continue;

                const auto keep  = root_range[ a ].segments.size( ) >= root_range[ b ].segments.size( ) ? a : b;
                const auto other = keep == a ? b : a;

                root_range[ keep ].merge( root_range[ other ] );
                root_range[ other ].segments.clear( );
                parent[ other ] = keep;
                ++out.merged_copies;
            }

            out.bundle_of.assign( n, none );

            std::unordered_map< uint32_t, uint32_t > bundle_of_root;
            for ( uint32_t v { codegen::Reg::pinned_count }; v < n; ++v )
            {
                if ( lr.vregs[ v ].range.empty( ) ) continue;

                const auto root    = find( v );
                auto [ it, fresh ] = bundle_of_root.try_emplace( root, static_cast< uint32_t >( out.bundles.size( ) ) );
                if ( fresh )
                    out.bundles.push_back( Bundle {
                        .range = std::move( root_range[ root ] ),
                        .cls   = lr.vregs[ root ].cls,
                    } );

                auto& bundle = out.bundles[ it->second ];
                bundle.pieces.push_back( { v, lr.vregs[ v ].range } );
                bundle.mentions.insert( bundle.mentions.end( ), lr.vregs[ v ].mentions.begin( ), lr.vregs[ v ].mentions.end( ) );
                out.bundle_of[ v ] = it->second;
            }

            for ( auto& bundle : out.bundles ) std::ranges::stable_sort( bundle.mentions, { }, &Mention::point );

            for ( const auto& [ vreg, preg, weight ] : lr.hints )
            {
                if ( vreg >= n || out.bundle_of[ vreg ] == none ) continue;

                auto& votes = out.bundles[ out.bundle_of[ vreg ] ].hints;
                if ( const auto iter = std::ranges::find( votes, preg, &std::pair< codegen::PReg, float >::first ); iter != votes.end( ) )
                    iter->second += weight;
                else
                    votes.push_back( { preg, weight } );
            }

            for ( auto& bundle : out.bundles )
                std::ranges::stable_sort( bundle.hints, std::ranges::greater { }, &std::pair< codegen::PReg, float >::second );

            return out;
        }
    };
} // namespace xiao::regalloc
