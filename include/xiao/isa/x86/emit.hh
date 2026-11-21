#pragma once

#include <xiao/codegen/machbuffer.hh>
#include <xiao/isa/x86/frame.hh>
#include <xiao/isa/x86/inst/inst.hh>
#include <xiao/isa/x86/inst/regs.hh>
#include <xiao/regalloc/moves.hh>
#include <xiao/regalloc/regalloc.hh>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace xiao::x86_64
{
    [[nodiscard]] inline GprMem rsp_slot( const int32_t offset ) noexcept
    { return GprMem { AddressMode { .base = Reg::from_preg( RSP ), .disp = offset } }; }

    inline void emit_spill_load( MachBuffer< LabelUse >& buf, const PReg dst, const int32_t offset )
    { inst::movq_rm { Reg::from_preg( dst ), rsp_slot( offset ) }.emit( buf ); }

    inline void emit_spill_store( MachBuffer< LabelUse >& buf, const int32_t offset, const PReg src )
    { inst::movq_mr { rsp_slot( offset ), Reg::from_preg( src ) }.emit( buf ); }

    inline void emit_move( MachBuffer< LabelUse >& buf, const FrameLayout& frame, const regalloc::Move& m )
    {
        if ( m.from.is_reg( ) && m.to.is_reg( ) )
            inst::movq_mr { GprMem { Reg::from_preg( m.to.preg ) }, Reg::from_preg( m.from.preg ) }.emit( buf );
        else if ( m.from.is_reg( ) )
            emit_spill_store( buf, frame.spill_offset( m.to.slot ), m.from.preg );
        else if ( m.to.is_reg( ) )
            emit_spill_load( buf, m.to.preg, frame.spill_offset( m.from.slot ) );
        else
        {
            emit_spill_load( buf, frame.scratch[ 1 ], frame.spill_offset( m.from.slot ) );
            emit_spill_store( buf, frame.spill_offset( m.to.slot ), frame.scratch[ 1 ] );
        }
    }

    inline void emit_edits( MachBuffer< LabelUse >& buf, const FrameLayout& frame, const std::span< const regalloc::Edit > edits )
    {
        for ( auto e = edits.begin( ); e != edits.end( ); )
        {
            std::vector< regalloc::Move > group;
            auto                          end = e;
            for ( ; end != edits.end( ) && end->prio == e->prio; ++end )
            {
                assert( ( !end->from.is_reg( ) || end->from.preg.rc( ) == RegClass::Int )
                        && ( !end->to.is_reg( ) || end->to.preg.rc( ) == RegClass::Int )
                        && "[xiao::x86_64::emit_edits] float moves not supported YET" );
                group.push_back( { end->from, end->to } );
            }
            for ( const auto& m :
                  regalloc::sequence_parallel_moves( std::move( group ), regalloc::Allocation { .preg = frame.scratch[ 0 ] } ) )
                emit_move( buf, frame, m );
            e = end;
        }
    }

    inline void emit( VCode< X86Inst >& vcode, const regalloc::Allocations& allocs, const FrameLayout& frame,
                      MachBuffer< LabelUse >& buf )
    {
        struct Spilled
        {
            uint32_t vreg { };
            int32_t  slot { };
            PReg     scratch { };
            bool     reload { };
            bool     store { };
        };

        for ( uint32_t i { 0 }; i < vcode.blocks.size( ); ++i ) ( void ) buf.new_label( );

        emit_prologue( buf, frame );

        std::vector< PReg > pregs;
        auto                edit = allocs.edits.begin( );

        for ( uint32_t bi { 0 }; bi < vcode.blocks.size( ); ++bi )
        {
            buf.bind_label( bi );

            const auto& block = vcode.blocks[ bi ];
            for ( uint32_t ii { 0 }; ii < block.insts_count; ++ii )
            {
                const auto i_idx        = block.insts_offset + ii;
                const auto [ off, cnt ] = vcode.operand_ranges[ i_idx ];

                const auto edits_end = std::find_if( edit, allocs.edits.end( ),
                                                     [ & ]( const regalloc::Edit& e ) { return e.point != regalloc::before( i_idx ); } );
                emit_edits( buf, frame, std::span< const regalloc::Edit > { edit, edits_end } );
                edit = edits_end;

                if ( const auto* jmp = vcode.insts[i_idx].inner.get_if< inst::Jmp >( ); jmp && jmp->target.id == bi + 1) continue;

                if ( const auto* jcc = vcode.insts[i_idx].inner.get_if< inst::Jcc >( );
                    jcc && (jcc->taken.id == bi + 1 || jcc->not_taken.id == bi + 1 )
                )
                {
                    if ( jcc->not_taken.id == bi + 1) emit_jcc( buf, cc_nibble( jcc->cc ), jcc->taken.id);
                    else emit_jcc( buf, cc_nibble( jcc->cc ) ^ 1, jcc->not_taken.id);
                    continue;
                }

                if ( vcode.insts[ i_idx ].as_move( ) && cnt == 2 )
                {
                    const auto& d = allocs.ops[ off ];
                    const auto& s = allocs.ops[ off + 1 ];
                    if ( d.is_spill( ) ? s.is_spill( ) && d.slot == s.slot : !s.is_spill( ) && d.preg == s.preg ) continue;
                }

                pregs.clear( );
                std::array< Spilled, 2 > spills { };
                std::size_t              n_spills { 0 };

                for ( auto o { off }; o < off + cnt; ++o )
                {
                    const auto& a = allocs.ops[ o ];
                    if ( !a.is_spill( ) )
                    {
                        pregs.push_back( a.preg );
                        continue;
                    }

                    const auto& op = vcode.operands[ o ];
                    const auto  vr = op.vreg( ).index( );

                    Spilled* const used_end = spills.data( ) + n_spills;
                    Spilled*       s        = std::find_if( spills.data( ), used_end,
                                                            [ & ]( const Spilled& x )
                                                            {
                                                   return x.vreg == vr;
                                               } );
                    if ( s == used_end )
                    {
                        assert( n_spills < spills.size( ) && "[xiao::x86_64::emit] more than two spilled vregs in one instruction" );
                        spills[ n_spills ] = Spilled { .vreg = vr, .slot = a.slot, .scratch = frame.scratch[ n_spills ] };
                        s                  = &spills[ n_spills++ ];
                    }

                    if ( op.kind( ) == OperandKind::Use || op.kind( ) == OperandKind::UseDef ) s->reload = true;
                    if ( op.kind( ) == OperandKind::Def || op.kind( ) == OperandKind::UseDef ) s->store = true;

                    pregs.push_back( s->scratch );
                }

                if ( vcode.insts[ i_idx ].is_term( ) == MachTerminator::Ret ) emit_epilogue( buf, frame );

                for ( std::size_t s { 0 }; s < n_spills; ++s )
                    if ( spills[ s ].reload ) emit_spill_load( buf, spills[ s ].scratch, frame.spill_offset( spills[ s ].slot ) );

                auto& inst = vcode.insts[ i_idx ];
                inst.apply_alloc( pregs );
                inst.resolve_frame( frame.offsets( ) );
                inst.emit( buf );

                for ( std::size_t s { 0 }; s < n_spills; ++s )
                    if ( spills[ s ].store ) emit_spill_store( buf, frame.spill_offset( spills[ s ].slot ), spills[ s ].scratch );
            }
        }

        buf.resolve_labels( );
    }

} // namespace xiao::x86_64
