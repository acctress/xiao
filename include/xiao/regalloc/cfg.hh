#pragma once

#include <xiao/codegen/machinst.hh>
#include <xiao/codegen/vcode.hh>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace xiao::regalloc
{
    struct Cfg
    {
        static constexpr uint32_t none = ~0u;

        std::vector< std::vector< std::pair< uint32_t, uint32_t > > > preds { };
        std::vector< uint32_t >                                       cond_branch { };
        std::vector< uint32_t >                                       block_of { };

        template < codegen::MachInstC I >
        [[nodiscard]] static Cfg build( const codegen::VCode< I >& vc )
        {
            Cfg cfg;
            cfg.preds.resize( vc.blocks.size( ) );
            cfg.cond_branch.assign( vc.blocks.size( ), none );
            cfg.block_of.assign( vc.insts.size( ), none );

            for ( uint32_t b { 0 }; b < vc.blocks.size( ); ++b )
            {
                const auto& block = vc.blocks[ b ];
                for ( uint32_t k { 0 }; k < block.succs_count; ++k )
                    cfg.preds[ vc.succs[ block.succs_offset + k ].id ].push_back( { b, k } );

                for ( auto i { block.insts_offset }; i < block.insts_offset + block.insts_count; ++i )
                {
                    cfg.block_of[ i ] = b;
                    if ( cfg.cond_branch[ b ] == none && vc.insts[ i ].is_term( ) == codegen::MachTerminator::CondBranch )
                        cfg.cond_branch[ b ] = i;
                }
            }

            return cfg;
        }

        template < codegen::MachInstC I >
        [[nodiscard]] static bool leaves_at_branch( const codegen::VCode< I >& vc, const uint32_t pred, const uint32_t succ_index ) noexcept
        { return succ_index + 1 < vc.blocks[ pred ].succs_count; }

        [[nodiscard]] bool after_branch( const uint32_t inst ) const noexcept
        {
            const auto br = cond_branch[ block_of[ inst ] ];
            return br != none && inst > br;
        }
    };

    template < codegen::MachInstC I, typename Transfer >
    [[nodiscard]] std::vector< std::vector< uint32_t > > forward_dataflow( const codegen::VCode< I >& vc, const Cfg& cfg,
                                                                           const std::size_t width, Transfer&& transfer )
    {
        constexpr uint32_t none = ~0u;
        const auto         n    = vc.blocks.size( );

        std::vector< std::vector< uint32_t > > entry( n ), at_end( n ), at_branch( n );
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

                for ( const auto [ p, k ] : cfg.preds[ b ] )
                {
                    if ( !done[ p ] ) continue;
                    const auto& src = Cfg::leaves_at_branch( vc, p, k ) ? at_branch[ p ] : at_end[ p ];
                    if ( !have )
                    {
                        in   = src;
                        have = true;
                        continue;
                    }
                    for ( std::size_t j { 0 }; j < width; ++j )
                        if ( in[ j ] != src[ j ] ) in[ j ] = none;
                }

                if ( !have || ( done[ b ] && in == entry[ b ] ) ) continue;

                entry[ b ] = in;
                auto state = in;
                auto snap  = in;
                transfer( b, state, snap );
                at_end[ b ]    = std::move( state );
                at_branch[ b ] = std::move( snap );
                done[ b ]      = true;
                changed        = true;
            }
        }

        return entry;
    }
} // namespace xiao::regalloc
