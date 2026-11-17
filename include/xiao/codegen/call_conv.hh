#pragma once

#include <cstdint>
#include <string_view>

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

    [[nodiscard]] constexpr std::string_view to_string( const CallConv cc ) noexcept
    {
        switch ( cc )
        {
            case CallConv::SystemV: return "system_v";
            case CallConv::WindowsFastcall: return "windows_fastcall";
        }
        return "<?>";
    }
} // namespace xiao::codegen