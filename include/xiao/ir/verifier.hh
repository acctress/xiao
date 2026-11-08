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

            const auto& l = fn.insts[ block.insts_offset + block.insts_count - 1 ];

            if ( const bool is_term = l.op == IROp::ret || l.op == IROp::br || l.op == IROp::brif; !is_term )
            {
                errs.push_back( { std::format( "block does not end with a terminator: {}", l ) } );
                return;
            }

            if ( l.op == IROp::br || l.op == IROp::brif ) verify_branch( fn, block, l );
        }

        void on_inst( IRFunction& fn, IRBlock&, IRInst& inst )
        {
            switch ( inst.op )
            {
                case IROp::call: verify_call( fn, inst ); break;
                case IROp::load: verify_load( fn, inst ); break;
                case IROp::store: verify_store( fn, inst ); break;
                default: break;
            }
        }

    private:
        [[nodiscard]] static bool is_64bit_scalar( const Type ty ) noexcept
        { return ty == Type::i64 || ty == Type::ptr; }

        void verify_load( const IRFunction& fn, const IRInst& inst )
        {
            if ( inst.ops_count != 1 )
            {
                errs.push_back( { std::format( "load: {} operands, expected 1", inst.ops_count ) } );
                return;
            }

            if ( const auto addr = fn.values[ fn.operands[ inst.ops_offset ].id ].type; addr != Type::ptr )
                errs.push_back( { std::format( "load: address is {}, expected ptr", addr ) } );

            if ( const auto ty = fn.values[ inst.result.id ].type; !is_64bit_scalar( ty ) )
                errs.push_back( { std::format( "load.{}: only 64-bit loads supported YET", ty ) } );
        }

        void verify_store( const IRFunction& fn, const IRInst& inst )
        {
            if ( inst.ops_count != 2 )
            {
                errs.push_back( { std::format( "store: {} operands, expected 2", inst.ops_count ) } );
                return;
            }

            if ( const auto addr = fn.values[ fn.operands[ inst.ops_offset + 1 ].id ].type; addr != Type::ptr )
                errs.push_back( { std::format( "store: address is {}, expected ptr", addr ) } );

            if ( const auto ty = fn.values[ fn.operands[ inst.ops_offset ].id ].type; !is_64bit_scalar( ty ) )
                errs.push_back( { std::format( "store.{}: only 64-bit stores supported YET", ty ) } );
        }

        void verify_call( const IRFunction& fn, const IRInst& inst )
        {
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

        void verify_branch( const IRFunction& fn, const IRBlock& block, const IRInst& term )
        {
            const bool is_br = term.op == IROp::br;
            const auto name  = is_br ? std::string_view { "br" } : std::string_view { "brif" };

            if ( const std::size_t want_succ = is_br ? 1 : 2; block.succs_count != want_succ )
            {
                errs.push_back( { std::format( "{}: {} successors, expected {}", name, block.succs_count, want_succ ) } );
                return;
            }

            std::size_t want_ops = is_br ? 0 : 1;
            for ( uint8_t s = 0; s < block.succs_count; ++s )
            {
                const auto [ id ] = fn.succs[ block.succs_offset + s ];
                if ( id >= fn.blocks.size( ) )
                {
                    errs.push_back( { std::format( "{} to b{}, but the function has {} blocks", name, id, fn.blocks.size( ) ) } );
                    return;
                }

                if ( id == 0 ) errs.push_back( { std::format( "{} to the entry block b0", name ) } );
                want_ops += fn.blocks[ id ].params_count;
            }

            if ( term.ops_count != want_ops )
            {
                if ( is_br )
                {
                    const auto target = fn.succs[ block.succs_offset ].id;
                    errs.push_back( { std::format( "br to b{}: {} args, block takes {}", target, term.ops_count, want_ops ) } );
                }
                else
                {
                    const auto& then_b = fn.blocks[ fn.succs[ block.succs_offset ].id ];
                    const auto& else_b = fn.blocks[ fn.succs[ block.succs_offset + 1 ].id ];
                    errs.push_back( { std::format( "brif: {} operands, expected 1 condition + {} + {} args", term.ops_count,
                                                   then_b.params_count, else_b.params_count ) } );
                }
                return;
            }

            const auto [ then_args, else_args ] = branch_args( fn, block, term );
            verify_args( fn, name, then_args, fn.succs[ block.succs_offset ] );
            if ( !is_br ) verify_args( fn, name, else_args, fn.succs[ block.succs_offset + 1 ] );
        }

        void verify_args( const IRFunction& fn, const std::string_view name, const std::span< const ValueRef > args, const BlockRef target )
        {
            const auto& tb = fn.blocks[ target.id ];
            for ( auto i { 0uz }; i < args.size( ); ++i )
            {
                const auto got = fn.values[ args[ i ].id ].type;
                if ( const auto want = fn.values[ fn.block_params[ tb.params_offset + i ].id ].type; got != want )
                    errs.push_back( { std::format( "{} to b{}: arg {} is {}, block param is {}", name, target.id, i, got, want ) } );
            }
        }
    };
} // namespace xiao::ir