//
// Created by griffith on 17/10/2026.
//

#pragma once

#include <format>
#include <cstdint>

namespace xiao::ir
{
    //! @brief All of xiao's data types which are representable in the IR
    enum class Type : uint8_t
    {
        i8,
        i16,
        i32,
        i64,
        f32,
        f64,
        ptr,
        unit
    };
} // namespace xiao::ir

//! @brief std::formatter specialisation for ir::Type for formatting.
template <>
struct std::formatter< xiao::ir::Type >
{
    constexpr auto parse( auto& ctx )
    { return ctx.begin( ); }

    auto format( xiao::ir::Type t, auto& ctx ) const
    {
        switch ( t )
        {
            case xiao::ir::Type::i8: return std::format_to( ctx.out( ), "i8" );
            case xiao::ir::Type::i16: return std::format_to( ctx.out( ), "i16" );
            case xiao::ir::Type::i32: return std::format_to( ctx.out( ), "i32" );
            case xiao::ir::Type::i64: return std::format_to( ctx.out( ), "i64" );
            case xiao::ir::Type::f32: return std::format_to( ctx.out( ), "f32" );
            case xiao::ir::Type::f64: return std::format_to( ctx.out( ), "f64" );
            case xiao::ir::Type::ptr: return std::format_to( ctx.out( ), "ptr" );
            case xiao::ir::Type::unit: return std::format_to( ctx.out( ), "unit" );
        }
    }
};
