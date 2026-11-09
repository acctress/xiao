//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <xiao/codegen/call_conv.hh>
#include <xiao/support/arena.hh>
#include <xiao/ir/extname.hh>
#include <xiao/ir/block.hh>
#include <xiao/ir/inst.hh>
#include <xiao/ir/types.hh>
#include <xiao/ir/refs.hh>
#include <span>
#include <variant>

namespace xiao::ir
{
    //! @brief Function signature
    //! @returns The return type and a flat parameter type list.
    struct IRSig
    {
        Type              ret { };
        std::span< Type > params { };
        codegen::CallConv cc { codegen::host_call_conv( ) };
    };

    //! @brief A function this function calls
    struct ExtFunc
    {
        ExternalName      name { };
        Type              ret { };
        uint32_t          params_offset { };
        uint16_t          params_count { };
        codegen::CallConv cc { };
        bool              colocated { };
    };

    struct ExtSig
    {
        Type              ret { };
        uint32_t          params_offset { };
        uint16_t          params_count { };
        codegen::CallConv cc { };
    };

    struct GlobalValueData
    {
        ExternalName name { };
        int64_t      offset { };
        bool         colocated { };
    };

    //! @brief Owns flat pools for one IR function,
    //! @note All are views into the owning IRModule's arena
    struct IRFunction
    {
        support::Arena*              arena { };
        StringRef                    name { };
        IRSig                        sig { };
        FunctionRef                  self { };
        std::span< IRBlock >         blocks { };
        std::span< IRInst >          insts { };
        std::span< ValueRef >        operands { };
        std::span< BlockRef >        succs { };
        std::span< Value >           values { };
        std::span< ValueRef >        block_params { };
        std::span< ExtFunc >         ext_funcs { };
        std::span< Type >            ext_param_types { };
        std::span< GlobalValueData > global_values { };
        std::span< ExtSig >          signatures { };
    };

    struct BranchArgs
    {
        std::span< const ValueRef > then_args { };
        std::span< const ValueRef > else_args { };
    };

    [[nodiscard]] inline BranchArgs branch_args( const IRFunction& fn, const IRBlock& block, const IRInst& term )
    {
        const auto ops = fn.operands.subspan( term.ops_offset, term.ops_count );
        switch ( term.op )
        {
            case IROp::br: return { .then_args = ops, .else_args = { } };
            case IROp::brif:
            {
                const auto& then_b = fn.blocks[ fn.succs[ block.succs_offset ].id ];
                const auto  rest   = ops.subspan( 1 );
                return { .then_args = rest.first( then_b.params_count ), .else_args = rest.subspan( then_b.params_count ) };
            }
            default: return { };
        }
    }
} // namespace xiao::ir
