//
// Created by griffith on 19/10/2026.
//

#pragma once

#include <xiao/codegen/reg.hh>
#include <array>
#include <span>

namespace xiao::codegen
{
    //! @brief Register environment for a target ISA, register allocation facing
    //! Describes which registers the allocator may use and in what order
    struct MachineEnv
    {
        //! @brief Registers the allocator would prefer to use
        std::array< std::span< const PReg >, 2 > preferred;
        //! @brief Registers the allocator would use to fallback when preferred are exhausted
        std::array< std::span< const PReg >, 2 > non_preferred;
        //! @brief One scratch register per class, reserved for spill
        std::array< std::span< const PReg >, 2 > scratch;
    };
} // namespace xiao::codegen