#pragma once

#include <xiao/codegen/vcode.hh>
#include <unordered_set>
#include <vector>

namespace xiao::regalloc
{
    struct BlockLiveness
    {
        std::unordered_set< uint32_t > live_in { };
        std::unordered_set< uint32_t > live_out { };
    };

    struct Liveness
    {
        std::vector< BlockLiveness > blocks;

        template < codegen::MachInstC I >
        [[nodiscard]] static Liveness compute( const codegen::VCode< I >& vc )
        {
            struct GenKill
            {
                std::unordered_set< uint32_t > gen { }, kill { };
            };

            std::vector< GenKill > gen_kill( vc.blocks.size( ) );

            for ( auto b { 0u }; b < vc.blocks.size( ); ++b )
            {
                const auto& block   = vc.blocks[ b ];
                auto& [ gen, kill ] = gen_kill[ b ];

                for ( auto i { block.insts_offset }; i < block.insts_offset + block.insts_count; ++i )
                {
                    const auto [ ops_off, ops_cnt ] = vc.operand_ranges[ i ];

                    for ( auto o { ops_off }; o < ops_off + ops_cnt; ++o )
                    {
                        const auto& op = vc.operands[ o ];
                        const auto  vr = op.vreg( );

                        if ( !vr.valid( ) || vr.index( ) < codegen::Reg::pinned_count ) continue; // pinned: covered by fixed intervals

                        const bool is_use = op.kind( ) == codegen::OperandKind::Use || op.kind( ) == codegen::OperandKind::UseDef;
                        const bool is_def = op.kind( ) == codegen::OperandKind::Def || op.kind( ) == codegen::OperandKind::UseDef;

                        if ( is_use && !kill.contains( vr.index( ) ) ) gen.insert( vr.index( ) );
                        if ( is_def ) kill.insert( vr.index( ) );
                    }
                }
            }

            std::vector< std::unordered_set< uint32_t > > live_in( vc.blocks.size( ) );
            std::vector< std::unordered_set< uint32_t > > live_out( vc.blocks.size( ) );

            bool changed { true };
            while ( changed )
            {
                changed = false;

                for ( auto b { 0u }; b < vc.blocks.size( ); ++b )
                {
                    const auto& block = vc.blocks[ b ];

                    std::unordered_set< uint32_t > new_out { };
                    for ( auto s { block.succs_offset }; s < block.succs_offset + block.succs_count; ++s )
                        for ( const auto v : live_in[ vc.succs[ s ].id ] ) new_out.insert( v );

                    const auto& [ gen, kill ]             = gen_kill[ b ];
                    std::unordered_set< uint32_t > new_in = gen;
                    for ( const auto v : new_out )
                        if ( !kill.contains( v ) ) new_in.insert( v );

                    if ( new_in != live_in[ b ] || new_out != live_out[ b ] )
                    {
                        live_in[ b ]  = std::move( new_in );
                        live_out[ b ] = std::move( new_out );
                        changed       = true;
                    }
                }
            }

            Liveness result;

            result.blocks.resize( vc.blocks.size( ) );
            for ( auto b { 0u }; b < vc.blocks.size( ); ++b )
            {
                result.blocks[ b ].live_in  = std::move( live_in[ b ] );
                result.blocks[ b ].live_out = std::move( live_out[ b ] );
            }

            return result;
        }
    };
} // namespace xiao::regalloc