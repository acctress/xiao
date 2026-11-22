//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <xiao/ir/dominators.hh>
#include <xiao/ir/visitor.hh>
#include <string_view>
#include <optional>
#include <vector>

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

        static constexpr uint8_t max_slot_align_shift = 4;

        void on_function( IRFunction& fn )
        {
            index_function( fn );
            verify_stack_slots( fn );

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

            if ( !is_terminator( l.op ) )
            {
                errs.push_back( { std::format( "block does not end with a terminator: {}", l ) } );
                return;
            }

            if ( l.op == IROp::br || l.op == IROp::brif ) verify_branch( fn, block, l );
        }

        void on_inst( IRFunction& fn, IRBlock& block, IRInst& inst )
        {
            for ( uint16_t i { }; i < inst.ops_count; ++i )
                if ( const auto [ id ] = fn.operands[ inst.ops_offset + i ]; id >= fn.values.size( ) )
                {
                    errs.push_back(
                        { std::format( "{}: operand {} is v{}, but the function has {} values", inst.op, i, id, fn.values.size( ) ) } );
                    return;
                }

            if ( is_terminator( inst.op ) && &inst != &fn.insts[ block.insts_offset + block.insts_count - 1 ] )
                errs.push_back( { std::format( "{}: terminator before the end of the block", inst.op ) } );

            const auto bfr = errs.size( );
            switch ( inst.op )
            {
                case IROp::call: verify_call( fn, inst ); break;
                case IROp::load: verify_load( fn, inst ); break;
                case IROp::store: verify_store( fn, inst ); break;
                case IROp::iconst:
                    if ( const auto ty = fn.values[ inst.result.id ].type; !is_int( ty ) )
                        errs.push_back( { std::format( "iconst: type {} is not an integer type", ty ) } );
                    break;
                case IROp::iadd:
                case IROp::isub:
                case IROp::imul:
                case IROp::band:
                case IROp::bor:
                case IROp::bxor: verify_binary( fn, inst, false ); break;
                case IROp::idiv:
                case IROp::udiv:
                case IROp::srem:
                case IROp::urem: verify_binary( fn, inst, true ); break;
                case IROp::icmp: verify_binary( fn, inst, false ); break;
                case IROp::ishl:
                case IROp::ushr:
                case IROp::sshr: verify_shift( fn, inst ); break;
                case IROp::bnot:
                case IROp::ineg: verify_unary( fn, inst ); break;
                case IROp::uextend:
                case IROp::sextend:
                case IROp::ireduce: verify_convert( fn, inst ); break;
                case IROp::select: verify_select( fn, inst ); break;
                case IROp::symbol_value: verify_symbol_value( fn, inst ); break;
                case IROp::func_addr: verify_func_addr( fn, inst ); break;
                case IROp::call_indirect: verify_call_indirect( fn, inst ); break;
                case IROp::stack_addr: verify_stack_addr( fn, inst ); break;
                case IROp::ret: verify_ret( fn, inst ); break;
                default: break;
            }

            if ( errs.size( ) == bfr ) verify_dominance( fn, static_cast< uint32_t >( &block - fn.blocks.data( ) ), inst );
        }

    private:
        std::optional< DomTree > m_dom { };
        std::vector< uint32_t >  m_inst_block { };
        std::vector< uint32_t >  m_param_block { };

        [[nodiscard]] static Type operand_type( const IRFunction& fn, const IRInst& inst, const uint16_t i )
        { return fn.values[ fn.operands[ inst.ops_offset + i ].id ].type; }

        [[nodiscard]] static bool is_terminator( const IROp op ) noexcept
        { return op == IROp::ret || op == IROp::br || op == IROp::brif; }

        [[nodiscard]] bool expect_operands( const IRInst& inst, const uint16_t n )
        {
            if ( inst.ops_count == n ) return true;
            errs.push_back( { std::format( "{}: {} operands, expected {}", inst.op, inst.ops_count, n ) } );
            return false;
        }

        void index_function( const IRFunction& fn )
        {
            m_dom.emplace( fn );
            m_inst_block.assign( fn.insts.size( ), DomTree::NONE );
            m_param_block.assign( fn.values.size( ), DomTree::NONE );

            for ( uint32_t bi { }; bi < fn.blocks.size( ); ++bi )
            {
                const auto& block = fn.blocks[ bi ];

                for ( uint32_t i { }; i < block.insts_count; ++i ) m_inst_block[ block.insts_offset + i ] = bi;
                for ( uint16_t p { }; p < block.params_count; ++p )
                    if ( const auto value = fn.block_params[ block.params_offset + p ].id; value < m_param_block.size( ) )
                        m_param_block[ value ] = bi;
            }
        }

        void verify_binary( const IRFunction& fn, const IRInst& inst, const bool needs_64bit )
        {
            if ( !expect_operands( inst, 2 ) ) return;

            const auto a = operand_type( fn, inst, 0 );
            const auto b = operand_type( fn, inst, 1 );

            if ( !is_int( a ) || !is_int( b ) )
            {
                errs.push_back( { std::format( "{}: operands are {} and {}, expected integers", inst.op, a, b ) } );
                return;
            }

            if ( a != b && !( a == Type::ptr && b == Type::i64 ) )
                errs.push_back( { std::format( "{}: operand types {} and {} don't match", inst.op, a, b ) } );

            if ( needs_64bit && ( bit_width( a ) != 64 || bit_width( b ) != 64 ) )
                errs.push_back( { std::format( "{}: operands are {} and {}, extend narrow values to i64 first", inst.op, a, b ) } );
        }

        void verify_shift( const IRFunction& fn, const IRInst& inst )
        {
            if ( !expect_operands( inst, 2 ) ) return;

            const auto value = operand_type( fn, inst, 0 );
            const auto count = operand_type( fn, inst, 1 );

            if ( !is_int( value ) || !is_int( count ) )
                errs.push_back( { std::format( "{}: operands are {} and {}, expected integers", inst.op, value, count ) } );
            else if ( inst.op != IROp::ishl && bit_width( value ) != 64 )
                errs.push_back( { std::format( "{}: value is {}, extend narrow values to i64 first", inst.op, value ) } );
        }

        void verify_unary( const IRFunction& fn, const IRInst& inst )
        {
            if ( !expect_operands( inst, 1 ) ) return;

            if ( const auto a = operand_type( fn, inst, 0 ); !is_int( a ) )
                errs.push_back( { std::format( "{}: operand is {}, expected an integer", inst.op, a ) } );
        }

        void verify_convert( const IRFunction& fn, const IRInst& inst )
        {
            if ( !expect_operands( inst, 1 ) ) return;

            const auto from = operand_type( fn, inst, 0 );
            const auto to   = fn.values[ inst.result.id ].type;

            if ( !is_int( from ) || !is_int( to ) )
            {
                errs.push_back( { std::format( "{}: {} to {}, expected integers", inst.op, from, to ) } );
                return;
            }

            const bool widening = inst.op != IROp::ireduce;
            if ( widening ? bit_width( to ) <= bit_width( from ) : bit_width( to ) >= bit_width( from ) )
                errs.push_back( { std::format( "{}.{}: from {} is not {}", inst.op, to, from, widening ? "narrower" : "wider" ) } );
        }

        void verify_select( const IRFunction& fn, const IRInst& inst )
        {
            if ( !expect_operands( inst, 3 ) ) return;

            const auto c = operand_type( fn, inst, 0 );
            const auto a = operand_type( fn, inst, 1 );
            const auto b = operand_type( fn, inst, 2 );

            if ( !is_int( c ) ) errs.push_back( { std::format( "select: condition is {}, expected an integer", c ) } );
            if ( a != b ) errs.push_back( { std::format( "select: values are {} and {}, expected the same type", a, b ) } );
        }

        void verify_func_addr( const IRFunction& fn, const IRInst& inst )
        {
            if ( !expect_operands( inst, 0 ) ) return;

            if ( inst.imm >= fn.ext_funcs.size( ) )
                errs.push_back( { std::format( "func_addr: fn{} but only {} imports", inst.imm, fn.ext_funcs.size( ) ) } );

            if ( const auto ty = fn.values[ inst.result.id ].type; ty != Type::ptr )
                errs.push_back( { std::format( "func_addr.{}: result must be ptr", ty ) } );
        }

        void verify_call_indirect( const IRFunction& fn, const IRInst& inst )
        {
            if ( inst.imm >= fn.signatures.size( ) )
            {
                errs.push_back( { std::format( "call_indirect through sig{} but only {} signatures", inst.imm, fn.signatures.size( ) ) } );
                return;
            }

            const auto& sig = fn.signatures[ inst.imm ];
            verify_call_conv( fn, "call_indirect", sig.cc );

            if ( inst.ops_count != sig.params_count + 1 )
            {
                errs.push_back(
                    { std::format( "call_indirect sig{}: {} args, expected {}", inst.imm, inst.ops_count - 1, sig.params_count ) } );
                return;
            }

            if ( const auto callee = operand_type( fn, inst, 0 ); callee != Type::ptr )
                errs.push_back( { std::format( "call_indirect: callee is {}, expected ptr", callee ) } );

            for ( uint16_t i = 0; i < sig.params_count; ++i )
            {
                const auto got = operand_type( fn, inst, static_cast< uint16_t >( i + 1 ) );
                if ( const auto want = fn.ext_param_types[ sig.params_offset + i ]; got != want )
                    errs.push_back( { std::format( "call_indirect sig{}: arg {} is {}, expected {}", inst.imm, i, got, want ) } );
            }
        }

        void verify_stack_slots( const IRFunction& fn )
        {
            for ( auto i { 0uz }; i < fn.stack_slots.size( ); ++i )
            {
                const auto& [ size, align_shift ] = fn.stack_slots[ i ];
                if ( size == 0 ) errs.push_back( { std::format( "ss{}: size is 0", i ) } );
                if ( align_shift > max_slot_align_shift )
                    errs.push_back( { std::format( "ss{}: align = {} is over the maximum of {}", i, 1ull << align_shift,
                                                   1u << max_slot_align_shift ) } );
            }
        }

        void verify_stack_addr( const IRFunction& fn, const IRInst& inst )
        {
            if ( !expect_operands( inst, 0 ) ) return;

            const auto [ id ] = slot_of( inst );
            const bool known  = id < fn.stack_slots.size( );
            if ( !known ) errs.push_back( { std::format( "stack_addr: ss{} but only {} stack slots", id, fn.stack_slots.size( ) ) } );

            if ( const auto ty = fn.values[ inst.result.id ].type; ty != Type::ptr )
                errs.push_back( { std::format( "stack_addr.{}: result must be ptr", ty ) } );

            if ( !known ) return;

            const auto size = fn.stack_slots[ id ].size;
            if ( const auto off = slot_offset( inst ); off < 0 || static_cast< uint32_t >( off ) > size )
                errs.push_back( { std::format( "stack_addr: ss{}{:+} is outside the slot's {} bytes", id, off, size ) } );
        }

        void verify_symbol_value( const IRFunction& fn, const IRInst& inst )
        {
            if ( !expect_operands( inst, 0 ) ) return;

            if ( inst.imm >= fn.global_values.size( ) )
                errs.push_back( { std::format( "symbol_value: gv{} but only {} global values", inst.imm, fn.global_values.size( ) ) } );

            if ( const auto ty = fn.values[ inst.result.id ].type; ty != Type::ptr )
                errs.push_back( { std::format( "symbol_value.{}: result must be ptr", ty ) } );
        }

        void verify_load( const IRFunction& fn, const IRInst& inst )
        {
            if ( inst.ops_count != 1 )
            {
                errs.push_back( { std::format( "load: {} operands, expected 1", inst.ops_count ) } );
                return;
            }

            if ( const auto addr = fn.values[ fn.operands[ inst.ops_offset ].id ].type; addr != Type::ptr )
                errs.push_back( { std::format( "load: address is {}, expected ptr", addr ) } );

            if ( const auto ty = fn.values[ inst.result.id ].type; !is_int( ty ) )
                errs.push_back( { std::format( "load.{}: only integer loads supported YET", ty ) } );
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

            if ( const auto ty = fn.values[ fn.operands[ inst.ops_offset ].id ].type; !is_int( ty ) )
                errs.push_back( { std::format( "store.{}: only integer stores supported YET", ty ) } );
        }

        void verify_call_conv( const IRFunction& fn, const std::string_view what, const codegen::CallConv callee )
        {
            if ( fn.sig.cc == codegen::CallConv::WindowsFastcall && callee == codegen::CallConv::SystemV )
                errs.push_back( { std::format( "{}: windows_fastcall calling system_v not supported yet", what ) } );
        }

        void verify_call( const IRFunction& fn, const IRInst& inst )
        {
            if ( inst.imm >= fn.ext_funcs.size( ) )
            {
                errs.push_back( { std::format( "call to fn{} but only {} imports", inst.imm, fn.ext_funcs.size( ) ) } );
                return;
            }

            const auto& ext = fn.ext_funcs[ inst.imm ];
            verify_call_conv( fn, "call", ext.cc );

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

            if ( !is_br )
                if ( const auto c = operand_type( fn, term, 0 ); !is_int( c ) )
                    errs.push_back( { std::format( "brif: condition is {}, expect an integer", c ) } );

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

        void verify_ret( const IRFunction& fn, const IRInst& inst )
        {
            const auto need = fn.sig.ret;
            if ( !expect_operands( inst, need == Type::unit ? 0 : 1 ) ) return;
            if ( need == Type::unit ) return;

            if ( const auto got = operand_type( fn, inst, 0 ); got != need )
                errs.push_back( { std::format( "ret: value type is {}, the function's signature wants {}", got, need ) } );
        }

        void verify_dominance( const IRFunction& fn, const uint32_t u_block, const IRInst& inst )
        {
            if ( !m_dom->is_reachable( u_block ) ) return;

            const auto use = static_cast< uint32_t >( &inst - fn.insts.data( ) );
            for ( uint16_t i { }; i < inst.ops_count; ++i )
            {
                const auto value = fn.operands[ inst.ops_offset + i ].id;
                const auto def   = fn.values[ value ].inst.id;

                if ( def == ~0u )
                {
                    if ( const auto pb = m_param_block[ value ]; pb != DomTree::NONE && !m_dom->block_dominates( pb, u_block ) )
                        errs.push_back(
                            { std::format( "{}: uses v{}, a param of b{} which does not dominate b{}", inst.op, value, pb, u_block ) } );

                    continue;
                }

                if ( def == use )
                {
                    errs.push_back( { std::format( "{}: v{} uses itself", inst.op, value ) } );
                    continue;
                }

                const auto db = def < m_inst_block.size( ) ? m_inst_block[ def ] : DomTree::NONE;
                const bool ok = db == u_block ? def < use : db != DomTree::NONE && m_dom->block_dominates( db, u_block );

                if ( !ok ) errs.push_back( { std::format( "{}: uses v{} from a definition that does not dominate it", inst.op, value ) } );
            }
        }
    };
} // namespace xiao::ir