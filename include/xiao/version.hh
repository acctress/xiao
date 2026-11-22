#pragma once

#include <string_view>

#define XIAO_VERSION_MAJOR  0
#define XIAO_VERSION_MINOR  1
#define XIAO_VERSION_PATCH  0
#define XIAO_VERSION_STRING "0.1.0"

namespace xiao
{
    inline constexpr int              version_major = XIAO_VERSION_MAJOR;
    inline constexpr int              version_minor = XIAO_VERSION_MINOR;
    inline constexpr int              version_patch = XIAO_VERSION_PATCH;
    inline constexpr std::string_view version       = XIAO_VERSION_STRING;
} // namespace xiao
