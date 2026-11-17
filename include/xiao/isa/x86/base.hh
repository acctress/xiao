#pragma once

#include <xiao/isa/x86/reg.hh>
#include <xiao/codegen/reg.hh>

#include <cstdint>
#include <string_view>
#include <utility>

namespace xiao::x86_64
{
    using namespace xiao::codegen;

    enum class CC : uint8_t
    {
        E,
        NE,
        L,
        LE,
        G,
        GE,
        B,
        BE,
        A,
        AE
    };

    enum class RelocDistance : uint8_t
    {
        Near,
        Far,
    };

    struct AddressMode
    {
        Reg     base { };
        Reg     index { };
        uint8_t scale { };
        int32_t disp { };
    };

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
} // namespace xiao::x86_64
