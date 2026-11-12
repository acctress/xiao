#pragma once

#include <xiao/regalloc/cfg.hh>
#include <xiao/regalloc/moves.hh>
#include <xiao/codegen/vcode.hh>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace xiao::regalloc
{
    struct CheckerError
    {
        uint32_t inst { };
        uint32_t vreg { };
        uint32_t found { };
        bool     in_edit { };
    };

    template < codegen::MachInstC I >
    [[nodiscard]] std::vector< CheckerError > check_allocations( const codegen::VCode< I >& vc, const Cfg& cfg,
                                                                 const std::span< const Allocation > ops,
                                                                 const std::span< const Edit > edits, const uint32_t spill_slots )
    {
        constexpr uint32_t none  = ~0u;
        const std::size_t  width = codegen::Reg::pinned_count + spill_slots;

        const auto index = []( const Allocation& a ) -> std::size_t
        {
            return a.is_spill( ) ? codegen::Reg::pinned_count + static_cast< std::size_t >( a.slot ) : a.preg.index( );
        };

        std::vector< CheckerError > errors;

        const auto run_block = [ & ]( const uint32_t b, std::vector< uint32_t >& st, const bool report )
        {
            const auto& block = vc.blocks[ b ];
            auto        e     = std::ranges::lower_bound( edits, before( block.insts_offset ), { }, &Edit::point );

            for ( auto i { block.insts_offset }; i < block.insts_offset + block.insts_count; ++i )
            {
                while ( e != edits.end( ) && e->point == before( i ) )
                {
                    auto                    group_end = e;
                    std::vector< uint32_t > values;
                    while ( group_end != edits.end( ) && group_end->point == e->point && group_end->prio == e->prio )
                    {
                        const auto v = st[ index( group_end->from ) ];
                        if ( report && v != group_end->vreg ) errors.push_back( { i, group_end->vreg, v, true } );
                        values.push_back( v );
                        ++group_end;
                    }
                    std::size_t k { 0 };
                    for ( auto iter = e; iter != group_end; ++iter ) st[ index( iter->to ) ] = values[ k++ ];
                    e = group_end;
                }

                const auto [ off, cnt ] = vc.operand_ranges[ i ];
                for ( auto o { off }; o < off + cnt; ++o )
                {
                    const auto& op = vc.operands[ o ];
                    if ( op.is_fixed( ) || !op.vreg( ).valid( ) ) continue;
                    if ( op.kind( ) != codegen::OperandKind::Use && op.kind( ) != codegen::OperandKind::UseDef ) continue;
                    if ( !ops[ o ].is_assigned( ) ) continue;
                    const auto v = st[ index( ops[ o ] ) ];
                    if ( report && v != op.vreg( ).index( ) ) errors.push_back( { i, op.vreg( ).index( ), v, false } );
                }

                for ( const auto pr : vc.insts[ i ].clobbers( ) ) st[ pr.index( ) ] = none;

                for ( auto o { off }; o < off + cnt; ++o )
                {
                    const auto& op = vc.operands[ o ];
                    if ( op.kind( ) != codegen::OperandKind::Def && op.kind( ) != codegen::OperandKind::UseDef ) continue;
                    if ( op.is_fixed( ) )
                    {
                        st[ op.fixed_preg( )->index( ) ] = none;
                        continue;
                    }
                    const auto v = op.vreg( ).index( );
                    std::ranges::replace( st, v, none );
                    if ( ops[ o ].is_assigned( ) ) st[ index( ops[ o ] ) ] = v;
                }
            }
        };

        const auto entry = forward_dataflow( vc, cfg, width, [ & ]( const uint32_t b, auto& st ) { run_block( b, st, false ); } );

        for ( uint32_t b { 0 }; b < vc.blocks.size( ); ++b )
        {
            if ( entry[ b ].size( ) != width ) continue;
            auto st = entry[ b ];
            run_block( b, st, true );
        }

        return errors;
    }
} // namespace xiao::regalloc
