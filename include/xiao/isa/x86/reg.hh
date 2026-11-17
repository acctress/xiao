//
// Created by griffith on 19/10/2026.
//

#pragma once

#include <xiao/codegen/operand.hh>

namespace xiao::x86_64 {
    using codegen::PReg;
    using codegen::RegClass;

    inline constexpr PReg RAX = PReg::make(0,  RegClass::Int);
    inline constexpr PReg RCX = PReg::make(1,  RegClass::Int);
    inline constexpr PReg RDX = PReg::make(2,  RegClass::Int);
    inline constexpr PReg RBX = PReg::make(3,  RegClass::Int);
    inline constexpr PReg RSP = PReg::make(4,  RegClass::Int);
    inline constexpr PReg RBP = PReg::make(5,  RegClass::Int);
    inline constexpr PReg RSI = PReg::make(6,  RegClass::Int);
    inline constexpr PReg RDI = PReg::make(7,  RegClass::Int);
    inline constexpr PReg R8  = PReg::make(8,  RegClass::Int);
    inline constexpr PReg R9  = PReg::make(9,  RegClass::Int);
    inline constexpr PReg R10 = PReg::make(10, RegClass::Int);
    inline constexpr PReg R11 = PReg::make(11, RegClass::Int);
    inline constexpr PReg R12 = PReg::make(12, RegClass::Int);
    inline constexpr PReg R13 = PReg::make(13, RegClass::Int);
    inline constexpr PReg R14 = PReg::make(14, RegClass::Int);
    inline constexpr PReg R15 = PReg::make(15, RegClass::Int);

    inline constexpr PReg XMM0  = PReg::make(0,  RegClass::Float);
    inline constexpr PReg XMM1  = PReg::make(1,  RegClass::Float);
    inline constexpr PReg XMM2  = PReg::make(2,  RegClass::Float);
    inline constexpr PReg XMM3  = PReg::make(3,  RegClass::Float);
    inline constexpr PReg XMM4  = PReg::make(4,  RegClass::Float);
    inline constexpr PReg XMM5  = PReg::make(5,  RegClass::Float);
    inline constexpr PReg XMM6  = PReg::make(6,  RegClass::Float);
    inline constexpr PReg XMM7  = PReg::make(7,  RegClass::Float);
    inline constexpr PReg XMM8  = PReg::make(8,  RegClass::Float);
    inline constexpr PReg XMM9  = PReg::make(9,  RegClass::Float);
    inline constexpr PReg XMM10 = PReg::make(10, RegClass::Float);
    inline constexpr PReg XMM11 = PReg::make(11, RegClass::Float);
    inline constexpr PReg XMM12 = PReg::make(12, RegClass::Float);
    inline constexpr PReg XMM13 = PReg::make(13, RegClass::Float);
    inline constexpr PReg XMM14 = PReg::make(14, RegClass::Float);
    inline constexpr PReg XMM15 = PReg::make(15, RegClass::Float);
}