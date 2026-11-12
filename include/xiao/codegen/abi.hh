#pragma once

#include <xiao/codegen/reg.hh>
#include <xiao/ir/types.hh>
#include <xiao/codegen/call_conv.hh>

#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace xiao::codegen
{

    //! @brief A single solved argument or return location
    struct ArgLoc
    {
        std::variant< PReg, int32_t > slot;
        ir::Type                      ty;

        [[nodiscard]] bool is_reg( ) const noexcept
        { return std::holds_alternative< PReg >( slot ); }

        [[nodiscard]] bool is_stack( ) const noexcept
        { return std::holds_alternative< int32_t >( slot ); }

        [[nodiscard]] PReg reg( ) const noexcept
        { return std::get< PReg >( slot ); }

        [[nodiscard]] int32_t stack_offset( ) const noexcept
        { return std::get< int32_t >( slot ); }
    };

    //! @brief Resolved ABI assignment for one function signature
    struct ABISig
    {
        std::vector< ArgLoc > params;
        std::vector< ArgLoc > rets;
        uint32_t              stack_arg_bytes { };
        uint32_t              stack_align { 16 };
    };

    //! @brief Interface for ABI resolution
    struct ABISpec
    {
        virtual ~ABISpec( ) = default;

        [[nodiscard]] virtual ABISig compute( CallConv cc, std::span< const ir::Type > params, std::span< const ir::Type > rets ) const = 0;
    };

} // namespace xiao::codegen