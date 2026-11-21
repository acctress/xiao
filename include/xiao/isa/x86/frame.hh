#pragma once

#include <xiao/codegen/call_conv.hh>
#include <xiao/codegen/machbuffer.hh>
#include <xiao/codegen/machenv.hh>
#include <xiao/codegen/vcode.hh>
#include <xiao/ir/function.hh>
#include <xiao/isa/x86/inst/inst.hh>
#include <xiao/isa/x86/inst/regs.hh>
#include <xiao/regalloc/regalloc.hh>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <ranges>
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
        int32_t                incoming_base { };

        [[nodiscard]] int32_t spill_offset( const int32_t slot ) const noexcept
        { return static_cast< int32_t >( spill_base ) + slot * 8; }

        [[nodiscard]] FrameOffsets offsets( ) const noexcept
        { return FrameOffsets { .slots = slot_offsets, .incoming_base = incoming_base }; }
    };

    [[nodiscard]] inline FrameLayout compute_frame( const VCode< X86Inst >& vcode, const regalloc::Allocations& allocs,
                                                    const MachineEnv& env, const std::span< const ir::StackSlotData > slots = { } )
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

        const auto calls_fc = []( const X86Inst& i )
        {
            if ( const auto* c = i.inner.get_if< inst::Call >( ) ) return c->info->cc == CallConv::WindowsFastcall;
            if ( const auto* c = i.inner.get_if< inst::CallInd >( ) ) return c->info->cc == CallConv::WindowsFastcall;
            return false;
        };

        const uint32_t shadow = std::ranges::any_of( vcode.insts, calls_fc ) ? 32 : 0;
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

        frame.spill_base    = shadow;
        frame.incoming_base = static_cast< int32_t >( frame.stack_adjust + 8 * frame.saved.size( ) + 8 );

        assert( env.scratch[ 0 ].size( ) >= 2 && "[xiao::x86_64::compute_frame] need two int scratch registers for spilling" );
        frame.scratch = { env.scratch[ 0 ][ 0 ], env.scratch[ 0 ][ 1 ] };

        return frame;
    }

    inline void emit_rsp_adjust( MachBuffer< LabelUse >& buf, const uint8_t ext, const uint32_t n )
    {
        const GprMem rsp { Reg::from_preg( RSP ) };
        const bool   sub = ext == 5;

        if ( fits_i8( n ) )
        {
            const auto imm = static_cast< int8_t >( n );
            if ( sub ) inst::subq_mi8 { rsp, imm }.emit( buf );
            else
                inst::addq_mi8 { rsp, imm }.emit( buf );
            return;
        }

        const auto imm = static_cast< int32_t >( n );
        if ( sub ) inst::subq_mi32 { rsp, imm }.emit( buf );
        else
            inst::addq_mi32 { rsp, imm }.emit( buf );
    }

    inline void emit_prologue( MachBuffer< LabelUse >& buf, const FrameLayout& frame )
    {
        for ( const auto pr : frame.saved ) inst::pushq_r { Reg::from_preg( pr ) }.emit( buf );

        if ( frame.stack_adjust ) emit_rsp_adjust( buf, 5, frame.stack_adjust );
    }

    inline void emit_epilogue( MachBuffer< LabelUse >& buf, const FrameLayout& frame )
    {
        if ( frame.stack_adjust ) emit_rsp_adjust( buf, 0, frame.stack_adjust );

        for ( const auto pr : std::views::reverse( frame.saved ) ) inst::popq_r { Reg::from_preg( pr ) }.emit( buf );
    }
} // namespace xiao::x86_64
