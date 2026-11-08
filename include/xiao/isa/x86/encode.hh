#pragma once

#include <xiao/codegen/machbuffer.hh>
#include <xiao/isa/x86/inst.hh>
#include <cassert>
#include <cstdint>

namespace xiao::x86_64
{
    [[nodiscard]] constexpr uint8_t modrm( const uint8_t mod, const uint8_t reg, const uint8_t rm ) noexcept
    { return static_cast< uint8_t >( ( mod << 6 ) | ( ( reg & 7 ) << 3 ) | ( rm & 7 ) ); }

    inline void emit_rex( MachBuffer< LabelUse >& buf, const bool w64, const PReg reg, const PReg rm ) noexcept
    {
        uint8_t rex { 0x40 };
        if ( w64 ) rex |= 0x08;
        if ( reg.valid( ) && reg.hw( ) >= 8 ) rex |= 0x04;
        if ( rm.valid( ) && rm.hw( ) >= 8 ) rex |= 0x01;
        if ( rex != 0x40 ) buf.emit_u8( rex );
    }

    inline void emit_rex_mem( MachBuffer< LabelUse >& buf, const bool w64, const PReg reg, const PReg base, const PReg index ) noexcept
    {
        uint8_t rex { 0x40 };
        if ( w64 ) rex |= 0x08;
        if ( reg.valid( ) && reg.hw( ) >= 8 ) rex |= 0x04;
        if ( index.valid( ) && index.hw( ) >= 8 ) rex |= 0x02;
        if ( base.valid( ) && base.hw( ) >= 8 ) rex |= 0x01;
        if ( rex != 0x40 ) buf.emit_u8( rex );
    }

    inline void emit_mem_operand( MachBuffer< LabelUse >& buf, const uint8_t reg_field, const PReg base, const PReg index,
                                  const uint8_t scale_log2, const int32_t disp ) noexcept
    {
        assert( base.valid( ) && "[xiao::x86_64::emit_mem_operand] base register required" );
        assert( ( !index.valid( ) || index.hw( ) != 4 ) && "[xiao::x86_64::emit_mem_operand] rsp cannot be an index" );
        assert( scale_log2 <= 3 && "[xiao::x86_64::emit_mem_operand] scale must be 0..3" );

        const uint8_t base_low  = base.hw( ) & 7;
        const bool    needs_sib = index.valid( ) || base_low == 4;

        uint8_t mod { 0b10 };
        if ( disp == 0 && base_low != 5 ) mod = 0b00;
        else if ( disp >= INT8_MIN && disp <= INT8_MAX )
            mod = 0b01;

        if ( needs_sib )
        {
            const uint8_t index_low = index.valid( ) ? ( index.hw( ) & 7 ) : 0b100;
            buf.emit_u8( modrm( mod, reg_field, 0b100 ) );
            buf.emit_u8( static_cast< uint8_t >( ( scale_log2 << 6 ) | ( index_low << 3 ) | base_low ) );
        }
        else
        {
            buf.emit_u8( modrm( mod, reg_field, base_low ) );
        }

        if ( mod == 0b01 ) buf.emit_u8( static_cast< uint8_t >( static_cast< int8_t >( disp ) ) );
        else if ( mod == 0b10 )
            buf.emit_i32( disp );
    }
} // namespace xiao::x86_64