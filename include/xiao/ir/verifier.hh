//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <xiao/ir/visitor.hh>

namespace xiao::ir
{
    //! @brief Validates invariants structural of finalised IR.
    struct IRVerifier : IRVisitor< IRVerifier >
    {
        struct Err
        {
            std::string msg;
        };

        std::vector< Err > errs;

        [[nodiscard]] bool ok( ) const
        { return errs.empty( ); }

        void on_function( IRFunction& fn )
        {
            if ( fn.blocks.empty( ) ) return;

            const auto& entry = fn.blocks[ 0 ];
            if ( entry.params_count != fn.sig.params.size( ) )
            {
                errs.push_back(
                    { std::format( "entry block has {} params, signature has {}", entry.params_count, fn.sig.params.size( ) ) } );
                return;
            }

            for ( uint16_t i = 0; i < entry.params_count; ++i )
            {
                const auto got = fn.values[ fn.block_params[ entry.params_offset + i ].id ].type;
                if ( const auto want = fn.sig.params[ i ]; got != want )
                    errs.push_back( { std::format( "entry block param {} is {}, signature says {}", i, got, want ) } );
            }
        }

        void on_block_end( IRFunction& fn, IRBlock& block )
        {
            if ( block.insts_count == 0 )
            {
                errs.push_back( { "block is empty, no terminator." } );
                return;
            }

            const auto& l       = fn.insts[ block.insts_offset + block.insts_count - 1 ];
            const bool  is_term = l.op == IROp::ret || l.op == IROp::br || l.op == IROp::brif;

            if ( !is_term ) errs.push_back( { std::format( "block does not end with a terminator: {}", l ) } );
        }

        void on_inst( IRFunction& fn, IRBlock&, IRInst& inst )
        {
            if ( inst.op != IROp::call ) return;

            if ( inst.imm >= fn.ext_funcs.size( ) )
            {
                errs.push_back( { std::format( "call to fn{} but only {} imports", inst.imm, fn.ext_funcs.size( ) ) } );
                return;
            }

            const auto& ext = fn.ext_funcs[ inst.imm ];
            if ( inst.ops_count != ext.params_count )
            {
                errs.push_back( { std::format( "call fn{}: {} args, expected {}", inst.imm, inst.ops_count, ext.params_count ) } );
                return;
            }

            for ( uint16_t i = 0; i < inst.ops_count; ++i )
            {
                const auto got = fn.values[ fn.operands[ inst.ops_offset + i ].id ].type;
                if ( const auto want = fn.ext_param_types[ ext.params_offset + i ]; got != want )
                    errs.push_back( { std::format( "call fn{}: arg {} is {}, expected {}", inst.imm, i, got, want ) } );
            }
        }
    };
} // namespace xiao::ir