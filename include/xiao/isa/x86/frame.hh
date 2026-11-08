#pragma once

#include <xiao/codegen/call_conv.hh>
#include <xiao/codegen/machbuffer.hh>
#include <xiao/codegen/machenv.hh>
#include <xiao/codegen/vcode.hh>
#include <xiao/isa/x86/encode.hh>
#include <xiao/isa/x86/inst.hh>
#include <xiao/isa/x86/reg.hh>
#include <xiao/regalloc/regalloc.hh>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

namespace xiao::x86_64
{
    struct FrameLayout
    {
        std::vector< PReg >   saved { };
        uint32_t              stack_adjust { };
        bool                  has_calls { };
        uint32_t              spill_base { };
        std::array< PReg, 2 > scratch { };

        [[nodiscard]] int32_t spill_offset( const int32_t slot ) const noexcept
        { return static_cast< int32_t >( spill_base ) + slot * 8; }
    };

    [[nodiscard]] inline FrameLayout compute_frame( const VCode< X86Inst >& vcode, const regalloc::Allocations& allocs,
                                                    const codegen::MachineEnv& env, const codegen::CallConv cc )
    {
        FrameLayout frame { };

        const auto used = [ & ]( const PReg pr )
        {
            return std::ranges::any_of( allocs.ops,
                                        [ & ]( const regalloc::Allocation& op )
                                        {
                                            return !op.is_spill( ) && op.preg == pr;
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

        const uint32_t shadow      = frame.has_calls && cc == codegen::CallConv::WindowsFastcall ? 32 : 0;
        const uint32_t spill_bytes = 8 * allocs.spill_slots;
        const uint32_t base        = shadow + spill_bytes;

        if ( frame.has_calls || allocs.spill_slots > 0 )
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
        for ( const auto pr : frame.saved )
        {
            if ( pr.hw( ) >= 8 ) buf.emit_u8( 0x41 );
            buf.emit_u8( static_cast< uint8_t >( 0x50 + ( pr.hw( ) & 7 ) ) );
        }

        if ( frame.stack_adjust ) emit_rsp_adjust( buf, 5, frame.stack_adjust );
    }

    inline void emit_epilogue( MachBuffer< LabelUse >& buf, const FrameLayout& frame )
    {
        if ( frame.stack_adjust ) emit_rsp_adjust( buf, 0, frame.stack_adjust );

        for ( auto it = frame.saved.rbegin( ); it != frame.saved.rend( ); ++it )
        {
            if ( it->hw( ) >= 8 ) buf.emit_u8( 0x41 );
            buf.emit_u8( static_cast< uint8_t >( 0x58 + ( it->hw( ) & 7 ) ) );
        }
    }
} // namespace xiao::x86_64
