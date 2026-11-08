//
// Created by griffith on 17/10/2026.
//

#pragma once

#include <xiao/ir/value.hh>

namespace xiao::ir
{
    //! @brief An enum of all IR instruction opcodes.
    enum class IROp
    {
        iconst,
        iadd,
        isub,
        imul,
        idiv,
        icmp,
        br,
        brif,
        ret,
        call,
        store,
        load,
    };

    //! @brief Condition kinds for icmp.
    enum class ICmpCC : uint8_t
    {
        eq,
        ne,
        slt,
        sle,
        sgt,
        sge,
        ult,
        ule,
        ugt,
        uge
    };

    //! @brief A single IR instruction in SSA form.
    //! @note Operands are stored in a flat pool in an owning IRFunction,
    //! indexed by `ops_offset` and `ops_count`.
    struct IRInst
    {
        IROp     op { };
        ICmpCC   cc { };
        ValueRef result { };
        uint32_t ops_offset { };
        uint16_t ops_count { };
        uint64_t imm { };
    };

    //! @brief Read the offset out of `inst.imm`
    [[nodiscard]] constexpr int32_t mem_offset( const IRInst& inst ) noexcept
    { return static_cast< int32_t >( static_cast< int64_t >( inst.imm ) ); }
} // namespace xiao::ir

template <>
struct std::formatter< xiao::ir::IROp >
{
    constexpr auto parse( auto& ctx )
    { return ctx.begin( ); }

    auto format( xiao::ir::IROp op, auto& ctx ) const
    {
        using enum xiao::ir::IROp;
        switch ( op )
        {
            case iconst: return std::format_to( ctx.out( ), "iconst" );
            case iadd: return std::format_to( ctx.out( ), "iadd" );
            case isub: return std::format_to( ctx.out( ), "isub" );
            case imul: return std::format_to( ctx.out( ), "imul" );
            case idiv: return std::format_to( ctx.out( ), "idiv" );
            case icmp: return std::format_to( ctx.out( ), "icmp" );
            case br: return std::format_to( ctx.out( ), "br" );
            case brif: return std::format_to( ctx.out( ), "brif" );
            case ret: return std::format_to( ctx.out( ), "ret" );
            case call: return std::format_to( ctx.out( ), "call" );
            case store: return std::format_to( ctx.out( ), "store" );
            case load: return std::format_to( ctx.out( ), "load" );
        }
    }
};

template <>
struct std::formatter< xiao::ir::ICmpCC >
{
    constexpr auto parse( auto& ctx )
    { return ctx.begin( ); }

    auto format( xiao::ir::ICmpCC cc, auto& ctx ) const
    {
        using enum xiao::ir::ICmpCC;
        switch ( cc )
        {
            case eq: return std::format_to( ctx.out( ), "eq" );
            case ne: return std::format_to( ctx.out( ), "ne" );
            case slt: return std::format_to( ctx.out( ), "slt" );
            case sle: return std::format_to( ctx.out( ), "sle" );
            case sgt: return std::format_to( ctx.out( ), "sgt" );
            case sge: return std::format_to( ctx.out( ), "sge" );
            case ult: return std::format_to( ctx.out( ), "ult" );
            case ule: return std::format_to( ctx.out( ), "ule" );
            case ugt: return std::format_to( ctx.out( ), "ugt" );
            case uge: return std::format_to( ctx.out( ), "uge" );
        }
    }
};

template <>
struct std::formatter< xiao::ir::IRInst >
{
    constexpr auto parse( auto& ctx )
    { return ctx.begin( ); }

    auto format( const xiao::ir::IRInst& inst, auto& ctx ) const
    {
        if ( inst.op == xiao::ir::IROp::iconst )
            return std::format_to( ctx.out( ), "v{} = {} {} #{}", inst.result.id, inst.op, xiao::ir::Type::i64, inst.imm );
        if ( inst.op == xiao::ir::IROp::icmp )
            return std::format_to( ctx.out( ), "v{} = {} {} v{} v{}", inst.result.id, inst.op, inst.cc, inst.ops_offset,
                                   inst.ops_offset + 1 );
        return std::format_to( ctx.out( ), "v{} = {} [ops {}..{}]", inst.result.id, inst.op, inst.ops_offset,
                               inst.ops_offset + inst.ops_count );
    }
};