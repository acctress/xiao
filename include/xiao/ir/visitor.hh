//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <xiao/ir/module.hh>

namespace xiao::ir {
    //! @brief CRTP base for mutable IR visitors.
    //! Derive from IRVisitor to implement any IR traversal for static dispatch
    //! only. Only override the hooks you need, or they default to no ops.
    //! @tparam Derived The deriving visitor type
    template <typename Derived>
    class IRVisitor {
    public:
        //! @brief Visit all functions in a module
        void visit_module(IRModule& m) {
            for (auto* fn : m.functions()) visit_function(*fn);
        }

        //! @brief Visit a function and all of its blocks
        void visit_function(IRFunction& fn) {
            self().on_function(fn);
            for (auto& b : fn.blocks) visit_block(fn, b);
            self().on_function_end(fn);
        }

        //! @brief Visit a block and all of its instructions
        void visit_block(IRFunction& fn, IRBlock& b) {
            self().on_block(fn, b);
            for (auto& i : fn.insts.subspan(b.insts_offset, b.insts_count)) visit_inst(fn, b, i);
            self().on_block_end(fn, b);
        }

        //! @brief Visit an instructions and all of its operands
        void visit_inst(IRFunction& fn, IRBlock& b, IRInst& in) {
            self().on_inst(fn, b, in);
            for (auto i { 0u }; i < in.ops_count; ++i)
                self().on_operand(fn, b, in, fn.operands[in.ops_offset + i]);
        }

        //! @brief Hook called on entry to each function.
        void on_function(IRFunction&)                               {}
        //! @brief Hook called on exit of each function.
        void on_function_end(IRFunction&)                           {}
        //! @brief Hook called on entry to each block.
        void on_block(IRFunction&, IRBlock&)                        {}
        //! @brief Hook called on exit of each block.
        void on_block_end(IRFunction&, IRBlock&)                    {}
        //! @brief Hook called for each instruction before its operands.
        void on_inst(IRFunction&, IRBlock&, IRInst&)                {}
        //! @brief Hook called for each operand of an instruction.
        void on_operand(IRFunction&, IRBlock&, IRInst&, ValueRef)   {}

    private:
        Derived& self() { return static_cast<Derived&>(*this); }
    };
}
