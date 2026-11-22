#pragma once

#include <xiao/codegen/reg.hh>
#include <xiao/isa/x86/inst/args.hh>

#include <array>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>

namespace xiao::x86_64
{
    [[nodiscard]] constexpr std::string_view preg_name( const PReg pr ) noexcept
    {
        if ( pr.rc( ) == RegClass::Int )
        {
            switch ( pr.hw( ) )
            {
                case 0: return "rax";
                case 1: return "rcx";
                case 2: return "rdx";
                case 3: return "rbx";
                case 4: return "rsp";
                case 5: return "rbp";
                case 6: return "rsi";
                case 7: return "rdi";
                case 8: return "r8";
                case 9: return "r9";
                case 10: return "r10";
                case 11: return "r11";
                case 12: return "r12";
                case 13: return "r13";
                case 14: return "r14";
                case 15: return "r15";
                default: return "<?>";
            }
        }

        switch ( pr.hw( ) )
        {
            case 0: return "xmm0";
            case 1: return "xmm1";
            case 2: return "xmm2";
            case 3: return "xmm3";
            case 4: return "xmm4";
            case 5: return "xmm5";
            case 6: return "xmm6";
            case 7: return "xmm7";
            case 8: return "xmm8";
            case 9: return "xmm9";
            case 10: return "xmm10";
            case 11: return "xmm11";
            case 12: return "xmm12";
            case 13: return "xmm13";
            case 14: return "xmm14";
            case 15: return "xmm15";
            default: return "<?>";
        }
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

    [[nodiscard]] constexpr std::string_view to_string( const CC cc ) noexcept
    {
        switch ( cc )
        {
            case CC::E: return "e";
            case CC::NE: return "ne";
            case CC::L: return "l";
            case CC::LE: return "le";
            case CC::G: return "g";
            case CC::GE: return "ge";
            case CC::B: return "b";
            case CC::BE: return "be";
            case CC::A: return "a";
            case CC::AE: return "ae";
        }
        return "<?>";
    }

    [[nodiscard]] inline std::string fmt_gpr( const Reg r, const uint8_t bits )
    {
        if ( r.valid( ) && r.is_physical( ) ) return std::string { gpr_name( r.as_preg( ), bits ) };
        return std::format( "{}", r );
    }

    [[nodiscard]] inline std::string fmt_address( const AddressMode& mem )
    {
        std::string out { "[" };

        switch ( mem.amode_kind )
        {
            case AmodeKind::IncomingArg: out += "in"; break;
            case AmodeKind::OutgoingArg: out += "out"; break;
            case AmodeKind::Slot: std::format_to( std::back_inserter( out ), "ss{}", mem.slot ); break;
            case AmodeKind::Real:
                if ( mem.base.valid( ) ) out += fmt_gpr( mem.base, 64 );

                if ( mem.index.valid( ) )
                {
                    if ( out.size( ) > 1 ) out += '+';
                    out += fmt_gpr( mem.index, 64 );
                    if ( mem.scale ) std::format_to( std::back_inserter( out ), "*{}", 1u << mem.scale );
                }
                break;
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
