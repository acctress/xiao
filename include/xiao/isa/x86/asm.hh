#pragma once

#include <xiao/isa/x86/reg.hh>
#include <xiao/codegen/operand.hh>
#include <xiao/isa/x86/encode.hh>
#include <xiao/isa/x86/fmt.hh>

#include <array>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>

namespace xiao::x86_64
{
    struct Opcode
    {
        bool                     p66 { };
        uint8_t                  rep { };
        bool                     rex_w { };
        std::array< uint8_t, 3 > bytes { };
        uint8_t                  len { };
    };

    struct GprMem
    {
        std::variant< Reg, AddressMode > value { };

        GprMem( ) = default;

        explicit GprMem( const Reg reg ) noexcept : value { reg } { }

        explicit GprMem( const AddressMode& mem ) noexcept : value { mem } { }
    };

    template < typename G, typename V >
    void visit_gpr_mem( G& rm, V& v, const OperandKind kind )
    {
        if ( auto* reg = std::get_if< Reg >( &rm.value ) )
        {
            switch ( kind )
            {
                case OperandKind::Use: v.use( *reg ); break;
                case OperandKind::Def: v.def( *reg ); break;
                case OperandKind::UseDef: v.usedef( *reg ); break;
            }
            return;
        }

        auto& mem = std::get< AddressMode >( rm.value );
        v.use( mem.base );
        v.use( mem.index );
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

            const auto& mem   = std::get< AddressMode >( rm.value );
            const auto  base  = preg_of( mem.base );
            const auto  index = preg_of( mem.index );
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

    [[nodiscard]] constexpr std::string_view gpr_name( const PReg pr, const uint8_t bits ) noexcept
    {
        constexpr std::array< std::string_view, 16 > b { "al",  "cl",  "dl",   "bl",   "spl",  "bpl",  "sil",  "dil",
                                                         "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" };
        constexpr std::array< std::string_view, 16 > w { "ax",  "cx",  "dx",   "bx",   "sp",   "bp",   "si",   "di",
                                                         "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w" };
        constexpr std::array< std::string_view, 16 > d { "eax", "ecx", "edx",  "ebx",  "esp",  "ebp",  "esi",  "edi",
                                                         "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };

        const auto hw = pr.hw( );
        if ( pr.rc( ) != RegClass::Int || hw >= 16 ) return preg_name( pr );

        switch ( bits )
        {
            case 8: return b[ hw ];
            case 16: return w[ hw ];
            case 32: return d[ hw ];
            default: return preg_name( pr );
        }
    }

    [[nodiscard]] inline std::string fmt_gpr( const Reg r, const uint8_t bits )
    {
        if ( r.valid( ) && r.is_physical( ) ) return std::string { gpr_name( r.as_preg( ), bits ) };
        return std::format( "{}", r );
    }

    [[nodiscard]] inline std::string fmt_address( const AddressMode& mem )
    {
        std::string out { "[" };
        if ( mem.base.valid( ) ) out += fmt_gpr( mem.base, 64 );

        if ( mem.index.valid( ) )
        {
            if ( out.size( ) > 1 ) out += '+';
            out += fmt_gpr( mem.index, 64 );
            if ( mem.scale ) std::format_to( std::back_inserter( out ), "*{}", 1u << mem.scale );
        }

        if ( mem.disp > 0 ) std::format_to( std::back_inserter( out ), "+0x{:X}", mem.disp );
        else if ( mem.disp < 0 )
            std::format_to( std::back_inserter( out ), "-0x{:X}", -static_cast< int64_t >( mem.disp ) );

        out += ']';
        return out;
    }

    [[nodiscard]] constexpr std::string_view ptr_size( const uint8_t bits ) noexcept
    {
        switch ( bits )
        {
            case 8: return "byte";
            case 16: return "word";
            case 32: return "dword";
            default: return "qword";
        }
    }

    [[nodiscard]] inline std::string fmt_mem( const AddressMode& mem, const uint8_t bits )
    {
        if ( bits == 0 ) return fmt_address( mem );
        return std::format( "{} ptr {}", ptr_size( bits ), fmt_address( mem ) );
    }

    [[nodiscard]] inline std::string fmt_gpr_mem( const GprMem& rm, const uint8_t bits )
    {
        if ( const auto* reg = std::get_if< Reg >( &rm.value ) ) return fmt_gpr( *reg, bits );
        return std::format( "{} ptr {}", ptr_size( bits ), fmt_address( std::get< AddressMode >( rm.value ) ) );
    }
} // namespace xiao::x86_64
