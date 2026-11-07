//
// Created by griffith on 19/10/2026.
//

#pragma once

#include "xiao/codegen/call_conv.hh"

#include <xiao/codegen/machenv.hh>
#include <xiao/isa/x86/reg.hh>

namespace xiao::x86_64
{

    //! sysv

    inline constexpr PReg k_sysv_preferred_int[] = { RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11 };

    inline constexpr PReg k_sysv_non_preferred_int[] = { RBX, R12, R13, R14, R15 };

    inline constexpr PReg k_sysv_preferred_float[]
        = { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15 };

    inline constexpr PReg k_sysv_non_preferred_float[] = { };

    //! win64

    inline constexpr PReg k_win64_preferred_int[] = { RAX, RCX, RDX, R8, R9, R10, R11 };

    inline constexpr PReg k_win64_non_preferred_int[] = { RBX, RSI, RDI, R12, R13, R14, R15 };

    inline constexpr PReg k_win64_preferred_float[] = { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5 };

    inline constexpr PReg k_win64_non_preferred_float[] = { XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15 };

    //! MachineEnv factories

    //! @brief Construct a MachineEnv for System V AMD64 ABI
    [[nodiscard]] inline codegen::MachineEnv sysv_env( ) noexcept
    {
        return {
            .preferred     = {    k_sysv_preferred_int,      k_sysv_preferred_float},
            .non_preferred = {k_sysv_non_preferred_int, std::span< const PReg > { }},
            .scratch       = {                     R10,                       XMM14},
        };
    }

    //! @brief Construct a MachineEnv for Windows x64 ABI
    [[nodiscard]] inline codegen::MachineEnv win64_env( ) noexcept
    {
        return {
            .preferred     = {    k_win64_preferred_int,     k_win64_preferred_float},
            .non_preferred = {k_win64_non_preferred_int, std::span< const PReg > { }},
            .scratch       = {                      R10,                        XMM4},
        };
    }

    [[nodiscard]] inline codegen::MachineEnv env_for( const codegen::CallConv cc ) noexcept
    {
        switch ( cc )
        {
            case codegen::CallConv::SystemV:         return sysv_env( );
            case codegen::CallConv::WindowsFastcall: return win64_env( );
        }
        std::unreachable( );
    }
} // namespace xiao::x86_64