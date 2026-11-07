#pragma once

#include <cstdint>

namespace xiao::codegen
{
    //! @brief Calling convention tag
    enum class CallConv : uint8_t
    {
        SystemV,
        WindowsFastcall
    };

    [[nodiscard]] constexpr CallConv host_call_conv( ) noexcept
    {
#if defined( _WIN64 )
        return CallConv::WindowsFastcall;
#else
        return CallConv::SystemV;
#endif
    }
} // namespace xiao::codegen