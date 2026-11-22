#pragma once

#include <xiao/codegen/operand.hh>
#include <xiao/codegen/reg.hh>
#include <xiao/isa/x86/inst/regs.hh>

#include <concepts>
#include <cstdint>
#include <span>
#include <type_traits>
#include <variant>

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

    enum class AmodeKind : uint8_t
    {
        Real,
        IncomingArg,
        OutgoingArg,
        Slot
    };

    struct AddressMode
    {
        Reg       base { };
        Reg       index { };
        uint8_t   scale { };
        int32_t   disp { };
        AmodeKind amode_kind { };
        int32_t   slot { };
    };

    struct FrameOffsets
    {
        std::span< const int32_t > slots { };
        int32_t                    incoming_base { };
    };

    [[nodiscard]] static constexpr AddressMode incoming_arg( const int32_t offset ) noexcept
    { return { .disp = offset, .amode_kind = AmodeKind::IncomingArg }; }

    [[nodiscard]] static constexpr AddressMode outgoing_arg( const int32_t offset ) noexcept
    { return { .disp = offset, .amode_kind = AmodeKind::OutgoingArg }; }

    [[nodiscard]] static constexpr AddressMode stack_slot( const int32_t slot, const int32_t offset ) noexcept
    { return { .disp = offset, .amode_kind = AmodeKind::Slot, .slot = slot }; }

    struct GprMem
    {
        std::variant< Reg, AddressMode > value { };

        GprMem( ) = default;

        explicit GprMem( const Reg reg ) noexcept : value { reg } { }

        explicit GprMem( const AddressMode& mem ) noexcept : value { mem } { }
    };

    template < typename M, typename V >
        requires std::same_as< std::remove_const_t< M >, AddressMode >
    void visit_mem( M& mode, V& v )
    {
        if constexpr ( requires { v.mem( mode ); } ) v.mem( mode );
        v.use( mode.base );
        v.use( mode.index );
    }

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
        visit_mem( mem, v );
    }
} // namespace xiao::x86_64
