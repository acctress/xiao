#pragma once

#include <xiao/codegen/machbuffer.hh>
#include <xiao/isa/x86/inst.hh>
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
} // namespace xiao::x86_64