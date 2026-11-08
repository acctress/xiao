#pragma once

#include <xiao/regalloc/regalloc.hh>
#include <xiao/codegen/machbuffer.hh>
#include <xiao/isa/x86/emit_impl.hh>
#include <xiao/isa/x86/frame.hh>
#include <xiao/isa/x86/inst.hh>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace xiao::x86_64
{
    inline void emit( const VCode< X86Inst >& vcode, const regalloc::Allocations& allocs, const FrameLayout& frame,
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

                if ( const auto* jmp = std::get_if< inst::Jmp >( &vcode.insts[ i_idx ].inner );
                     jmp && ii + 1 == block.insts_count && jmp->target.id == bi + 1 )
                    continue;

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

                vcode.insts[ i_idx ].emit( buf, pregs );

                for ( std::size_t s { 0 }; s < n_spills; ++s )
                    if ( spills[ s ].store ) emit_spill_store( buf, frame.spill_offset( spills[ s ].slot ), spills[ s ].scratch );
            }
        }

        buf.resolve_labels( );
    }

} // namespace xiao::x86_64
