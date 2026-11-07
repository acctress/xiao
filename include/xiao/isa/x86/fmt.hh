#pragma once

#include "xiao/codegen/reg.hh"

#include <string_view>

namespace xiao::x86_64
{
    [[nodiscard]] constexpr std::string_view preg_name( const codegen::PReg pr ) noexcept
    {
        if ( pr.rc( ) == codegen::RegClass::Int )
        {
            switch ( pr.hw( ) )
            {
                case 0: return "rax";
                case 1: return "rcx";
                case 2: return "rdx";
                case 3: return "rbx";
                case 4: return "rsp";
                case 5: return "rbp";
                case 6: return "rsi";
                case 7: return "rdi";
                case 8: return "r8";
                case 9: return "r9";
                case 10: return "r10";
                case 11: return "r11";
                case 12: return "r12";
                case 13: return "r13";
                case 14: return "r14";
                case 15: return "r15";
            }
        }
        else
        {
            switch ( pr.hw( ) )
            {
                case 0: return "xmm0";
                case 1: return "xmm1";
                case 2: return "xmm2";
                case 3: return "xmm3";
                case 4: return "xmm4";
                case 5: return "xmm5";
                case 6: return "xmm6";
                case 7: return "xmm7";
                case 8: return "xmm8";
                case 9: return "xmm9";
                case 10: return "xmm10";
                case 11: return "xmm11";
                case 12: return "xmm12";
                case 13: return "xmm13";
                case 14: return "xmm14";
                case 15: return "xmm15";
            }
        }
        return "<?>";
    }
} // namespace xiao::x86_64