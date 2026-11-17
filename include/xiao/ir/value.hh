//
// Created by griffith on 17/10/2026.
//

#pragma once

#include <xiao/ir/types.hh>
#include <xiao/ir/refs.hh>

namespace xiao::ir
{
    //! @brief A single SSA value, holds its type and
    //! instruction ref which owns this value.
    struct Value
    {
        Type    type;
        InstRef inst;
    };
} // namespace xiao::ir

template <>
struct std::formatter< xiao::ir::Value >
{
    constexpr auto parse( auto& ctx )
    { return ctx.begin( ); }

    auto format( const xiao::ir::Value& v, auto& ctx ) const
    { return std::format_to( ctx.out( ), "{}:v{}", v.type, v.inst.id ); }
};