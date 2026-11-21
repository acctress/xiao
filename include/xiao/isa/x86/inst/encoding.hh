#pragma once

#include <xiao/codegen/machbuffer.hh>
#include <xiao/isa/x86/inst/args.hh>

#include <array>
#include <cassert>
#include <cstdint>
#include <utility>
#include <variant>

namespace xiao::x86_64
{
    struct LabelUse
    {
        enum class Kind : uint8_t
        {
            JmpRel32
        } kind;

        explicit constexpr LabelUse( const Kind k ) : kind { k } { }

        [[nodiscard]] static int32_t encode( const LabelUse use, const uint32_t from, const uint32_t to )
        {
            switch ( use.kind )
            {
                case Kind::JmpRel32: return static_cast< int32_t >( to ) - static_cast< int32_t >( from + 4 );
            }

            std::unreachable( );
        }
    };

    struct Opcode
    {
        bool                     p66 { };
        uint8_t                  rep { };
        bool                     rex_w { };
        std::array< uint8_t, 3 > bytes { };
        uint8_t                  len { };
    };

    [[nodiscard]] constexpr bool fits_i8( const int64_t v ) noexcept
    { return v >= INT8_MIN && v <= INT8_MAX; }

    [[nodiscard]] constexpr uint8_t modrm( const uint8_t mod, const uint8_t reg, const uint8_t rm ) noexcept
    { return static_cast< uint8_t >( ( mod << 6 ) | ( ( reg & 7 ) << 3 ) | ( rm & 7 ) ); }

    [[nodiscard]] constexpr bool needs_rex_for_byte( const PReg r ) noexcept
    { return r.valid( ) && r.hw( ) >= 4 && r.hw( ) <= 7; }

    [[nodiscard]] constexpr uint8_t cc_nibble( const CC cc ) noexcept
    {
        switch ( cc )
        {
            case CC::B: return 0x2;
            case CC::AE: return 0x3;
            case CC::E: return 0x4;
            case CC::NE: return 0x5;
            case CC::BE: return 0x6;
            case CC::A: return 0x7;
            case CC::L: return 0xC;
            case CC::GE: return 0xD;
            case CC::LE: return 0xE;
            case CC::G: return 0xF;
        }
        return 0x4;
    }

    inline void emit_rex( MachBuffer< LabelUse >& buf, const bool w64, const PReg reg, const PReg rm, const bool byte_op = false ) noexcept
    {
        uint8_t rex { 0x40 };
        if ( w64 ) rex |= 0x08;
        if ( reg.valid( ) && reg.hw( ) >= 8 ) rex |= 0x04;
        if ( rm.valid( ) && rm.hw( ) >= 8 ) rex |= 0x01;
        if ( rex != 0x40 || ( byte_op && ( needs_rex_for_byte( reg ) || needs_rex_for_byte( rm ) ) ) ) buf.emit_u8( rex );
    }

    inline void emit_rex_mem( MachBuffer< LabelUse >& buf, const bool w64, const PReg reg, const PReg base, const PReg index,
                              const bool byte_op = false ) noexcept
    {
        uint8_t rex { 0x40 };
        if ( w64 ) rex |= 0x08;
        if ( reg.valid( ) && reg.hw( ) >= 8 ) rex |= 0x04;
        if ( index.valid( ) && index.hw( ) >= 8 ) rex |= 0x02;
        if ( base.valid( ) && base.hw( ) >= 8 ) rex |= 0x01;
        if ( rex != 0x40 || ( byte_op && needs_rex_for_byte( reg ) ) ) buf.emit_u8( rex );
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

    namespace detail
    {
        [[nodiscard]] constexpr PReg preg_of( const Reg r ) noexcept
        { return r.valid( ) ? r.as_preg( ) : PReg::invalid( ); }

        inline void emit_prefixes( MachBuffer< LabelUse >& buf, const Opcode& op ) noexcept
        {
            if ( op.p66 ) buf.emit_u8( 0x66 );
            if ( op.rep ) buf.emit_u8( op.rep );
        }

        inline void emit_opcode_bytes( MachBuffer< LabelUse >& buf, const Opcode& op ) noexcept
        {
            for ( uint8_t i = 0; i < op.len; ++i ) buf.emit_u8( op.bytes[ i ] );
        }

        inline void emit_modrm_rm( MachBuffer< LabelUse >& buf, const Opcode& op, const PReg rex_reg, const uint8_t reg_field,
                                   const GprMem& rm, const bool byte ) noexcept
        {
            emit_prefixes( buf, op );

            if ( const auto* reg = std::get_if< Reg >( &rm.value ) )
            {
                const auto r = reg->as_preg( );
                emit_rex( buf, op.rex_w, rex_reg, r, byte );
                emit_opcode_bytes( buf, op );
                buf.emit_u8( modrm( 0b11, reg_field, r.hw( ) ) );
                return;
            }

            const auto& mem = std::get< AddressMode >( rm.value );
            assert( mem.amode_kind == AmodeKind::Real && "[xiao::x86_64::emit_modrm_rm] synthetic address mode reached the encoder" );
            const auto base  = preg_of( mem.base );
            const auto index = preg_of( mem.index );
            emit_rex_mem( buf, op.rex_w, rex_reg, base, index, byte );
            emit_opcode_bytes( buf, op );
            emit_mem_operand( buf, reg_field, base, index, mem.scale, mem.disp );
        }
    } // namespace detail

    inline void emit_rm( MachBuffer< LabelUse >& buf, const Opcode& op, const PReg reg, const GprMem& rm, const bool byte ) noexcept
    { detail::emit_modrm_rm( buf, op, reg, reg.hw( ), rm, byte ); }

    inline void emit_rm_digit( MachBuffer< LabelUse >& buf, const Opcode& op, const uint8_t digit, const GprMem& rm,
                               const bool byte ) noexcept
    { detail::emit_modrm_rm( buf, op, PReg::invalid( ), digit, rm, byte ); }

    inline void emit_opreg( MachBuffer< LabelUse >& buf, const Opcode& op, const PReg reg, const bool byte ) noexcept
    {
        detail::emit_prefixes( buf, op );
        emit_rex( buf, op.rex_w, PReg::invalid( ), reg, byte );
        for ( uint8_t i = 0; i + 1 < op.len; ++i ) buf.emit_u8( op.bytes[ i ] );
        buf.emit_u8( static_cast< uint8_t >( op.bytes[ op.len - 1 ] + ( reg.hw( ) & 7 ) ) );
    }

    inline void emit_op( MachBuffer< LabelUse >& buf, const Opcode& op ) noexcept
    {
        detail::emit_prefixes( buf, op );
        if ( op.rex_w ) buf.emit_u8( 0x48 );
        detail::emit_opcode_bytes( buf, op );
    }

    inline void emit_jmp( MachBuffer< LabelUse >& buf, const uint32_t label ) noexcept
    {
        buf.emit_u8( 0xE9 );
        buf.emit_label_ref( label, LabelUse { LabelUse::Kind::JmpRel32 } );
    }

    inline void emit_jcc( MachBuffer< LabelUse >& buf, const uint8_t cc, const uint32_t label ) noexcept
    {
        buf.emit_u8( 0x0F );
        buf.emit_u8( static_cast< uint8_t >( 0x80 | cc ) );
        buf.emit_label_ref( label, LabelUse { LabelUse::Kind::JmpRel32 } );
    }
} // namespace xiao::x86_64
