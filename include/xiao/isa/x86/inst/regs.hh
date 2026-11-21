//
// Created by griffith on 19/10/2026.
//

#pragma once

#include <xiao/codegen/call_conv.hh>
#include <xiao/codegen/machenv.hh>
#include <xiao/codegen/operand.hh>

#include <span>
#include <utility>

namespace xiao::x86_64
{
    using codegen::PReg;
    using codegen::RegClass;

    inline constexpr PReg RAX = PReg::make( 0, RegClass::Int );
    inline constexpr PReg RCX = PReg::make( 1, RegClass::Int );
    inline constexpr PReg RDX = PReg::make( 2, RegClass::Int );
    inline constexpr PReg RBX = PReg::make( 3, RegClass::Int );
    inline constexpr PReg RSP = PReg::make( 4, RegClass::Int );
    inline constexpr PReg RBP = PReg::make( 5, RegClass::Int );
    inline constexpr PReg RSI = PReg::make( 6, RegClass::Int );
    inline constexpr PReg RDI = PReg::make( 7, RegClass::Int );
    inline constexpr PReg R8  = PReg::make( 8, RegClass::Int );
    inline constexpr PReg R9  = PReg::make( 9, RegClass::Int );
    inline constexpr PReg R10 = PReg::make( 10, RegClass::Int );
    inline constexpr PReg R11 = PReg::make( 11, RegClass::Int );
    inline constexpr PReg R12 = PReg::make( 12, RegClass::Int );
    inline constexpr PReg R13 = PReg::make( 13, RegClass::Int );
    inline constexpr PReg R14 = PReg::make( 14, RegClass::Int );
    inline constexpr PReg R15 = PReg::make( 15, RegClass::Int );

    inline constexpr PReg XMM0  = PReg::make( 0, RegClass::Float );
    inline constexpr PReg XMM1  = PReg::make( 1, RegClass::Float );
    inline constexpr PReg XMM2  = PReg::make( 2, RegClass::Float );
    inline constexpr PReg XMM3  = PReg::make( 3, RegClass::Float );
    inline constexpr PReg XMM4  = PReg::make( 4, RegClass::Float );
    inline constexpr PReg XMM5  = PReg::make( 5, RegClass::Float );
    inline constexpr PReg XMM6  = PReg::make( 6, RegClass::Float );
    inline constexpr PReg XMM7  = PReg::make( 7, RegClass::Float );
    inline constexpr PReg XMM8  = PReg::make( 8, RegClass::Float );
    inline constexpr PReg XMM9  = PReg::make( 9, RegClass::Float );
    inline constexpr PReg XMM10 = PReg::make( 10, RegClass::Float );
    inline constexpr PReg XMM11 = PReg::make( 11, RegClass::Float );
    inline constexpr PReg XMM12 = PReg::make( 12, RegClass::Float );
    inline constexpr PReg XMM13 = PReg::make( 13, RegClass::Float );
    inline constexpr PReg XMM14 = PReg::make( 14, RegClass::Float );
    inline constexpr PReg XMM15 = PReg::make( 15, RegClass::Float );

    inline constexpr PReg k_sysv_preferred_int[] = { RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11 };

    inline constexpr PReg k_sysv_non_preferred_int[] = { RBX, R12, R13, R14, R15 };

    inline constexpr PReg k_sysv_preferred_float[]
        = { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15 };

    inline constexpr PReg k_win64_preferred_int[] = { RAX, RCX, RDX, R8, R9, R10, R11 };

    inline constexpr PReg k_win64_non_preferred_int[] = { RBX, RSI, RDI, R12, R13, R14, R15 };

    inline constexpr PReg k_win64_preferred_float[] = { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5 };

    inline constexpr PReg k_win64_non_preferred_float[] = { XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15 };

    inline constexpr PReg k_int_scratch[] = { R10, R11 };

    inline constexpr PReg k_sysv_float_scratch[] = { XMM14 };

    inline constexpr PReg k_win64_float_scratch[] = { XMM4 };

    //! @brief Construct a MachineEnv for System V AMD64 ABI
    [[nodiscard]] inline codegen::MachineEnv sysv_env( ) noexcept
    {
        return {
            .preferred     = {    k_sysv_preferred_int,      k_sysv_preferred_float},
            .non_preferred = {k_sysv_non_preferred_int, std::span< const PReg > { }},
            .scratch       = {           k_int_scratch,        k_sysv_float_scratch},
        };
    }

    //! @brief Construct a MachineEnv for Windows x64 ABI
    [[nodiscard]] inline codegen::MachineEnv win64_env( ) noexcept
    {
        return {
            .preferred     = {    k_win64_preferred_int,     k_win64_preferred_float},
            .non_preferred = {k_win64_non_preferred_int, std::span< const PReg > { }},
            .scratch       = {            k_int_scratch,       k_win64_float_scratch},
        };
    }

    [[nodiscard]] inline codegen::MachineEnv env_for( const codegen::CallConv cc ) noexcept
    {
        switch ( cc )
        {
            case codegen::CallConv::SystemV: return sysv_env( );
            case codegen::CallConv::WindowsFastcall: return win64_env( );
        }
        std::unreachable( );
    }
} // namespace xiao::x86_64
