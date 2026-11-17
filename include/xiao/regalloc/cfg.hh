#pragma once

#include <xiao/codegen/machinst.hh>
#include <xiao/codegen/vcode.hh>

#include <cstdint>
#include <vector>

namespace xiao::regalloc
{
    struct Cfg
    {
        std::vector< std::vector< uint32_t > > preds { };
        std::vector< uint32_t >                block_of { };

        template < codegen::MachInstC I >
        [[nodiscard]] static Cfg build( const codegen::VCode< I >& vc )
        {
            Cfg cfg;
            cfg.preds.resize( vc.blocks.size( ) );
            cfg.block_of.resize( vc.insts.size( ) );

            for ( uint32_t b { 0 }; b < vc.blocks.size( ); ++b )
            {
                const auto& block = vc.blocks[ b ];
                for ( uint32_t k { 0 }; k < block.succs_count; ++k ) cfg.preds[ vc.succs[ block.succs_offset + k ].id ].push_back( b );
                for ( auto i { block.insts_offset }; i < block.insts_offset + block.insts_count; ++i ) cfg.block_of[ i ] = b;
            }

            return cfg;
        }
    };

    template < codegen::MachInstC I, typename Transfer >
    [[nodiscard]] std::vector< std::vector< uint32_t > > forward_dataflow( const codegen::VCode< I >& vc, const Cfg& cfg,
                                                                           const std::size_t width, Transfer&& transfer )
    {
        constexpr uint32_t none = ~0u;
        const auto         n    = vc.blocks.size( );

        std::vector< std::vector< uint32_t > > entry( n ), at_end( n );
        std::vector< bool >                    done( n, false );

        bool changed { true };
        while ( changed )
        {
            changed = false;
            for ( uint32_t b { 0 }; b < n; ++b )
            {
                std::vector< uint32_t > in;
                bool                    have { false };
                if ( b == 0 )
                {
                    in.assign( width, none );
                    have = true;
                }

                for ( const auto p : cfg.preds[ b ] )
                {
                    if ( !done[ p ] ) continue;
                    if ( !have )
                    {
                        in   = at_end[ p ];
                        have = true;
                        continue;
                    }
                    for ( std::size_t j { 0 }; j < width; ++j )
                        if ( in[ j ] != at_end[ p ][ j ] ) in[ j ] = none;
                }

                if ( !have || ( done[ b ] && in == entry[ b ] ) ) continue;

                entry[ b ] = in;
                transfer( b, in );
                at_end[ b ] = std::move( in );
                done[ b ]   = true;
                changed     = true;
            }
        }

        return entry;
    }
} // namespace xiao::regalloc
