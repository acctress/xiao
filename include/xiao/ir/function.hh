//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <xiao/codegen/call_conv.hh>
#include <xiao/support/arena.hh>
#include <xiao/ir/block.hh>
#include <xiao/ir/inst.hh>
#include <xiao/ir/types.hh>
#include <xiao/ir/refs.hh>
#include <span>

namespace xiao::ir
{
    //! @brief Function signature
    //! @returns The return type and a flat parameter type list.
    struct IRSig
    {
        Type              ret { };
        uint32_t          params_offset { };
        uint16_t          params_count { };
        codegen::CallConv cc { codegen::host_call_conv( ) };
    };

    //! @brief Owns flat pools for one IR function,
    //! @note All are views into the owning IRModule's arena
    struct IRFunction
    {
        support::Arena*       arena { };
        StringRef             name { };
        IRSig                 sig { };
        FunctionRef           self { };
        std::span< IRBlock >  blocks { };
        std::span< IRInst >   insts { };
        std::span< ValueRef > operands { };
        std::span< BlockRef > succs { };
        std::span< Value >    values { };
        std::span< ValueRef > block_params { };
    };
} // namespace xiao::ir
