#pragma once

#include <xiao/codegen/vcode.hh>
#include <cstdint>
#include <vector>
#include <bit>

namespace xiao::regalloc
{
    struct VRegSet
    {
        std::vector< uint64_t > words { };

        VRegSet( ) = default;

        explicit VRegSet( const std::size_t n ) : words( ( n + 63 ) / 64, 0 ) { }

        void insert( const uint32_t value ) noexcept
        { words[ value >> 6 ] |= uint64_t { 1 } << ( value & 63 ); }

        [[nodiscard]] bool contains( const uint32_t value ) const noexcept
        { return value >> 6 < words.size( ) && words[ value >> 6 ] >> ( value & 63 ) & 1; }

        [[nodiscard]] bool empty( ) const noexcept
        {
            for ( const auto w : words )
                if ( w ) return false;
            return true;
        }

        template < typename F >
        void for_each( F&& f ) const
        {
            for ( auto i { 0uz }; i < words.size( ); ++i )
                for ( auto w { words[ i ] }; w; w &= w - 1 ) f( static_cast< uint32_t >( i * 64 + std::countr_zero( w ) ) );
        }

        [[nodiscard]] std::vector< uint32_t > to_vector( ) const
        {
            std::vector< uint32_t > result;
            for_each(
                [ & ]( const uint32_t value )
                {
                    result.push_back( value );
                } );

            return result;
        }

        [[nodiscard]] bool operator== ( const VRegSet& ) const noexcept = default;
    };

    struct BlockLiveness
    {
        VRegSet live_in { };
        VRegSet live_out { };
    };

    struct Liveness
    {
        std::vector< BlockLiveness > blocks;

        template < codegen::MachInstC I >
        [[nodiscard]] static Liveness compute( const codegen::VCode< I >& vc )
        {
            const auto n_blocks = vc.blocks.size( );
            const auto n_vregs  = vc.vclasses.size( );

            std::vector< VRegSet > gen( n_blocks, VRegSet { n_vregs } ), kill( n_blocks, VRegSet { n_vregs } );

            for ( auto b { 0uz }; b < n_blocks; ++b )
            {
                const auto& block = vc.blocks[ b ];
                for ( auto i { block.insts_offset }; i < block.insts_offset + block.insts_count; ++i )
                {
                    const auto [ off, cnt ] = vc.operand_ranges[ i ];
                    for ( auto o { off }; o < off + cnt; ++o )
                    {
                        const auto& op = vc.operands[ o ];
                        const auto  vr = op.vreg( );

                        if ( !vr.valid( ) || vr.index( ) < codegen::Reg::pinned_count ) continue;

                        if ( op.kind( ) != codegen::OperandKind::Def && !kill[ b ].contains( vr.index( ) ) ) gen[ b ].insert( vr.index( ) );
                        if ( op.kind( ) != codegen::OperandKind::Use ) kill[ b ].insert( vr.index( ) );
                    }
                }
            }

            Liveness result;
            result.blocks.assign( n_blocks, { VRegSet { n_vregs }, VRegSet { n_vregs } } );

            for ( bool changed { true }; changed; )
            {
                changed = false;

                for ( auto b { n_blocks }; b-- > 0; )
                {
                    auto& [ in, out ] = result.blocks[ b ];
                    const auto& block = vc.blocks[ b ];

                    for ( auto s { block.succs_offset }; s < block.succs_offset + block.succs_count; ++s )
                    {
                        const auto& succ_in = result.blocks[ vc.succs[ s ].id ].live_in.words;
                        for ( auto w { 0uz }; w < out.words.size( ); ++w ) out.words[ w ] |= succ_in[ w ];
                    }

                    for ( auto w { 0uz }; w < in.words.size( ); ++w )
                    {
                        const auto nx  = gen[ b ].words[ w ] | out.words[ w ] & ~kill[ b ].words[ w ];
                        changed       |= nx != in.words[ w ];
                        in.words[ w ]  = nx;
                    }
                }
            }

            return result;
        }
    };
} // namespace xiao::regalloc