#pragma once

#include <cstdint>

namespace xiao::ir
{
    namespace extns
    {
        inline constexpr uint32_t function { 0 };
        inline constexpr uint32_t data { 1 };
    } // namespace extns

    //! @brief A symbol outside the current function
    struct ExternalName
    {
        uint32_t ns { };
        uint32_t index { };

        friend constexpr bool operator==(const ExternalName&, const ExternalName&) = default;
    };
} // namespace xiao::ir