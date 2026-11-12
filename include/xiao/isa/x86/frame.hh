#pragma once

#include <ranges>
#include <xiao/codegen/call_conv.hh>
#include <xiao/codegen/machbuffer.hh>
#include <xiao/codegen/machenv.hh>
#include <xiao/codegen/vcode.hh>
#include <xiao/ir/function.hh>
#include <xiao/isa/x86/encode.hh>
#include <xiao/isa/x86/inst.hh>
#include <xiao/isa/x86/reg.hh>
#include <xiao/regalloc/regalloc.hh>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace xiao::x86_64
{
    struct FrameLayout
    {
        std::vector< PReg >    saved { };
        uint32_t               stack_adjust { };
        bool                   has_calls { };
        uint32_t               spill_base { };
        std::array< PReg, 2 >  scratch { };
        std::vector< int32_t > slot_offsets { };

        [[nodiscard]] int32_t spill_offset( const int32_t slot ) const noexcept
        { return static_cast< int32_t >( spill_base ) + slot * 8; }
    };

    [[nodiscard]] inline FrameLayout compute_frame( const VCode< X86Inst >& vcode, const regalloc::Allocations& allocs,
                                                    const MachineEnv& env, const CallConv cc,
                                                    const std::span< const ir::StackSlotData > slots = { } )
    {
        FrameLayout frame { };

        const auto used = [ & ]( const PReg pr )
        {
            const auto holds = [ & ]( const regalloc::Allocation& a )
            {
                return a.is_reg( ) && a.preg == pr;
            };
            return std::ranges::any_of( allocs.ops, holds )
                || std::ranges::any_of( allocs.edits,
                                        [ & ]( const regalloc::Edit& e )
                                        {
                                            return holds( e.from ) || holds( e.to );
                                        } );
        };

        for ( const auto pr : env.non_preferred[ 0 ] )
            if ( used( pr ) ) frame.saved.push_back( pr );

        for ( [[maybe_unused]] const auto pr : env.non_preferred[ 1 ] )
            assert( !used( pr ) && "[xiao::x86_64::compute_frame] saving callee-saved float regs not supported YET" );

        frame.has_calls = std::ranges::any_of( vcode.insts,
                                               []( const X86Inst& i )
                                               {
                                                   return i.is_call( );
                                               } );

        const uint32_t shadow = frame.has_calls && cc == CallConv::WindowsFastcall ? 32 : 0;
        uint32_t       cursor = shadow + 8 * allocs.spill_slots;

        frame.slot_offsets.reserve( slots.size( ) );
        for ( const auto& [ size, align_shift ] : slots )
        {
            assert( align_shift <= 4 && "[xiao::x86_64::compute_frame] stack slots aligned over 16 need stack realignment" );
            const uint32_t align = 1u << align_shift;
            cursor               = ( cursor + align - 1 ) & ~( align - 1 );
            frame.slot_offsets.push_back( static_cast< int32_t >( cursor ) );
            cursor += size;
        }

        const uint32_t base = ( cursor + 7 ) & ~7u;
        assert( base < 4096 && "[xiao::x86_64::compute_frame] frames of 4096 bytes or more need stack probes" );

        if ( frame.has_calls || base > 0 )
        {
            const auto     pushed = static_cast< uint32_t >( 8 * frame.saved.size( ) );
            const uint32_t pad    = ( 8 + pushed + base ) % 16 == 0 ? 0 : 8;
            frame.stack_adjust    = base + pad;
        }

        frame.spill_base = shadow;

        assert( env.scratch[ 0 ].size( ) >= 2 && "[xiao::x86_64::compute_frame] need two int scratch registers for spilling" );
        frame.scratch = { env.scratch[ 0 ][ 0 ], env.scratch[ 0 ][ 1 ] };

        return frame;
    }

    inline void emit_spill_load( MachBuffer< LabelUse >& buf, const PReg dst, const int32_t offset )
    {
        emit_rex_mem( buf, true, dst, RSP, PReg::invalid( ) );
        buf.emit_u8( 0x8B );
        emit_mem_operand( buf, dst.hw( ), RSP, PReg::invalid( ), 0, offset );
    }

    inline void emit_spill_store( MachBuffer< LabelUse >& buf, const int32_t offset, const PReg src )
    {
        emit_rex_mem( buf, true, src, RSP, PReg::invalid( ) );
        buf.emit_u8( 0x89 );
        emit_mem_operand( buf, src.hw( ), RSP, PReg::invalid( ), 0, offset );
    }

    inline void emit_move( MachBuffer< LabelUse >& buf, const FrameLayout& frame, const regalloc::Move& m )
    {
        if ( m.from.is_reg( ) && m.to.is_reg( ) )
        {
            emit_rex( buf, true, m.from.preg, m.to.preg );
            buf.emit_u8( 0x89 );
            buf.emit_u8( modrm( 0b11, m.from.preg.hw( ), m.to.preg.hw( ) ) );
        }
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

    inline void emit_rsp_adjust( MachBuffer< LabelUse >& buf, const uint8_t ext, const uint32_t n )
    {
        buf.emit_u8( 0x48 );
        if ( n <= 127 )
        {
            buf.emit_u8( 0x83 );
            buf.emit_u8( modrm( 0b11, ext, 4 ) );
            buf.emit_u8( static_cast< uint8_t >( n ) );
        }
        else
        {
            buf.emit_u8( 0x81 );
            buf.emit_u8( modrm( 0b11, ext, 4 ) );
            buf.emit_u32( n );
        }
    }

    inline void emit_prologue( MachBuffer< LabelUse >& buf, const FrameLayout& frame )
    {
        for ( const auto pr : frame.saved ) emit_push( buf, pr );

        if ( frame.stack_adjust ) emit_rsp_adjust( buf, 5, frame.stack_adjust );
    }

    inline void emit_epilogue( MachBuffer< LabelUse >& buf, const FrameLayout& frame )
    {
        if ( frame.stack_adjust ) emit_rsp_adjust( buf, 0, frame.stack_adjust );

        for ( const auto pr : std::views::reverse( frame.saved ) ) emit_pop( buf, pr );
    }
} // namespace xiao::x86_64
