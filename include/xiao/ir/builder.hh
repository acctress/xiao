//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <xiao/ir/function.hh>
#include <xiao/support/arena.hh>
#include <algorithm>
#include <vector>

#include <print>

namespace xiao::ir
{
    template < typename T >
    static std::span< T > copy_to_arena( support::Arena& arena, const std::vector< T >& src )
    {
        if ( src.empty( ) ) return { };
        auto dst = arena.alloc_array_uninit< T >( src.size( ) );
        std::ranges::copy( src, dst.begin( ) );
        return dst;
    }

    class IRBuilder
    {
    public:
        explicit IRBuilder( IRFunction& fn ) : m_fn { fn } { }

        [[nodiscard]] BlockRef add_block( std::span< const Type > param_types = { } )
        {
            const auto block_id = static_cast< uint32_t >( m_blocks.size( ) );

            IRBlock b { };
            b.params_count  = static_cast< uint16_t >( param_types.size( ) );
            b.params_offset = static_cast< uint32_t >( m_block_params.size( ) );
            b.succs_offset  = static_cast< uint32_t >( m_succs.size( ) );
            b.succs_count   = 0;
            b.insts_offset  = 0;
            b.insts_count   = 0;

            m_block_param_offsets.push_back( static_cast< uint32_t >( m_block_params.size( ) ) );
            for ( const auto ty : param_types )
            {
                const auto vref = push_value( ty, InstRef { ~0u } );
                m_block_params.push_back( vref );
            }

            m_blocks.push_back( b );
            m_block_inst_starts.push_back( ~0u );
            m_block_inst_ends.push_back( ~0u );

            return BlockRef { block_id };
        }

        [[nodiscard]] BlockRef add_block( std::initializer_list< const Type > param_types = { } )
        {
            const auto block_id = static_cast< uint32_t >( m_blocks.size( ) );

            IRBlock b { };
            b.params_count  = static_cast< uint16_t >( param_types.size( ) );
            b.params_offset = static_cast< uint32_t >( m_block_params.size( ) );
            b.succs_offset  = static_cast< uint32_t >( m_succs.size( ) );
            b.succs_count   = 0;
            b.insts_offset  = 0;
            b.insts_count   = 0;

            m_block_param_offsets.push_back( static_cast< uint32_t >( m_block_params.size( ) ) );
            for ( const auto ty : param_types )
            {
                const auto vref = push_value( ty, InstRef { ~0u } );
                m_block_params.push_back( vref );
            }

            m_blocks.push_back( b );

            m_block_inst_starts.push_back( ~0u );
            m_block_inst_ends.push_back( ~0u );

            return BlockRef { block_id };
        }

        [[nodiscard]] FuncRef import_function( const ExternalName name, const std::span< const Type > params, const Type ret,
                                               const codegen::CallConv cc, const bool colocated )
        {
            const auto id = static_cast< uint32_t >( m_ext_funcs.size( ) );
            m_ext_funcs.push_back( ExtFunc { .name          = name,
                                             .ret           = ret,
                                             .params_offset = static_cast< uint32_t >( m_ext_param_types.size( ) ),
                                             .params_count  = static_cast< uint16_t >( params.size( ) ),
                                             .cc            = cc,
                                             .colocated     = colocated } );

            m_ext_param_types.insert( m_ext_param_types.end( ), params.begin( ), params.end( ) );
            return FuncRef { id };
        }

        [[nodiscard]] FuncRef import_function( const ExternalName name, const std::initializer_list< const Type > params, const Type ret,
                                               const codegen::CallConv cc, const bool colocated )
        { return import_function( name, std::span { params.begin( ), params.size( ) }, ret, cc, colocated ); }

        [[nodiscard]] ValueRef block_param( const BlockRef block, const uint16_t n ) const
        { return m_block_params[ m_block_param_offsets[ block.id ] + n ]; }

        //! @brief Get the n-th parameter of the entry block.
        [[nodiscard]] ValueRef param( const uint16_t n ) const
        { return block_param( BlockRef { 0 }, n ); }

        void switch_to_block( const BlockRef block )
        {
            if ( m_block_inst_starts[ m_cur.id ] != ~0u )
                assert( m_block_inst_ends[ m_cur.id ] != ~0u && "[IRBuilder::switch_to_block] current block not terminated" );

            m_cur = block;
            if ( m_block_inst_starts[ block.id ] == ~0u ) m_block_inst_starts[ block.id ] = static_cast< uint32_t >( m_insts.size( ) );
        }

        [[nodiscard]] ValueRef iconst( const Type ty, const uint64_t imm )
        { return emit( IROp::iconst, ty, { }, imm ); }

        [[nodiscard]] ValueRef iadd( const ValueRef a, const ValueRef b )
        { return emit_binop( IROp::iadd, a, b ); }

        [[nodiscard]] ValueRef isub( const ValueRef a, const ValueRef b )
        { return emit_binop( IROp::isub, a, b ); }

        [[nodiscard]] ValueRef imul( const ValueRef a, const ValueRef b )
        { return emit_binop( IROp::imul, a, b ); }

        [[nodiscard]] ValueRef idiv( const ValueRef a, const ValueRef b )
        { return emit_binop( IROp::idiv, a, b ); }

        [[nodiscard]] ValueRef icmp( const ICmpCC cc, const ValueRef a, const ValueRef b )
        {
            const ValueRef ops[] = { a, b };
            return emit( IROp::icmp, Type::i8, ops, 0, cc );
        }

        void br( const BlockRef target, const std::span< const ValueRef > args = { } )
        { emit_terminator( IROp::br, target, { }, ValueRef { ~0u }, args, { } ); }

        void brif( const ValueRef cond, const BlockRef then_block, const std::span< const ValueRef > then_args, const BlockRef else_block,
                   const std::span< const ValueRef > else_args )
        { emit_terminator( IROp::brif, then_block, else_block, cond, then_args, else_args ); }

        void ret( const ValueRef val = ValueRef { ~0u } )
        {
            if ( val.id != ~0u ) emit( IROp::ret, Type::unit, std::span { &val, 1 } );
            else
                emit( IROp::ret, Type::unit, { } );
            m_block_inst_ends[ m_cur.id ] = static_cast< uint32_t >( m_insts.size( ) );
        }

        [[nodiscard]] ValueRef call( const FuncRef callee, const std::span< const ValueRef > args )
        { return emit( IROp::call, m_ext_funcs[ callee.id ].ret, args, callee.id ); }

        [[nodiscard]] ValueRef load( const Type ty, const ValueRef addr, const int32_t offset = 0 )
        {
            const ValueRef ops[] = { addr };
            return emit( IROp::load, ty, ops, static_cast< uint64_t >( static_cast< int64_t >( offset ) ) );
        }

        void store( const ValueRef value, const ValueRef addr, const int32_t offset = 0 )
        {
            const ValueRef ops[] = { value, addr };
            emit( IROp::store, Type::unit, ops, static_cast< uint64_t >( static_cast< int64_t >( offset ) ) );
        }

        void finalise( )
        {
            auto& arena = *m_fn.arena;
            for ( std::size_t i = 0; i < m_blocks.size( ); ++i )
            {
                const auto start           = m_block_inst_starts[ i ] == ~0u ? 0u : m_block_inst_starts[ i ];
                const auto end             = m_block_inst_ends[ i ] == ~0u ? start : m_block_inst_ends[ i ];
                m_blocks[ i ].insts_offset = start;
                m_blocks[ i ].insts_count  = static_cast< uint16_t >( end - start );
            }

            m_fn.blocks          = copy_to_arena< IRBlock >( arena, m_blocks );
            m_fn.insts           = copy_to_arena< IRInst >( arena, m_insts );
            m_fn.operands        = copy_to_arena< ValueRef >( arena, m_operands );
            m_fn.succs           = copy_to_arena< BlockRef >( arena, m_succs );
            m_fn.values          = copy_to_arena< Value >( arena, m_values );
            m_fn.block_params    = copy_to_arena< ValueRef >( arena, m_block_params );
            m_fn.ext_funcs       = copy_to_arena< ExtFunc >( arena, m_ext_funcs );
            m_fn.ext_param_types = copy_to_arena< Type >( arena, m_ext_param_types );
        }

    private:
        IRFunction& m_fn;
        BlockRef    m_cur { };

        std::vector< IRBlock >  m_blocks;
        std::vector< IRInst >   m_insts;
        std::vector< ValueRef > m_operands;
        std::vector< BlockRef > m_succs;
        std::vector< Value >    m_values;
        std::vector< ValueRef > m_block_params;
        std::vector< uint32_t > m_block_inst_starts;
        std::vector< uint32_t > m_block_param_offsets;
        std::vector< uint32_t > m_block_inst_ends;
        std::vector< ExtFunc >  m_ext_funcs;
        std::vector< Type >     m_ext_param_types;

        [[nodiscard]] ValueRef push_value( const Type ty, const InstRef inst )
        {
            const auto id = static_cast< uint32_t >( m_values.size( ) );
            m_values.push_back( Value { .type = ty, .inst = inst } );
            return ValueRef { id };
        }

        ValueRef emit( const IROp op, const Type ty, const std::span< const ValueRef > ops, const uint64_t imm = 0, const ICmpCC cc = { } )
        {
            const auto inst_id = static_cast< uint32_t >( m_insts.size( ) );
            const auto vref    = push_value( ty, InstRef { inst_id } );

            IRInst inst { };
            inst.op         = op;
            inst.cc         = cc;
            inst.result     = vref;
            inst.ops_offset = static_cast< uint32_t >( m_operands.size( ) );
            inst.ops_count  = static_cast< uint16_t >( ops.size( ) );
            inst.imm        = imm;

            for ( const auto op_ref : ops ) m_operands.push_back( op_ref );
            m_insts.push_back( inst );
            return vref;
        }

        [[nodiscard]] ValueRef emit_binop( const IROp op, const ValueRef a, const ValueRef b )
        {
            const ValueRef ops[] = { a, b };
            return emit( op, m_values[ a.id ].type, ops );
        }

        void emit_terminator( const IROp op, const BlockRef primary, const BlockRef secondary, const ValueRef cond,
                              const std::span< const ValueRef > primary_args, const std::span< const ValueRef > secondary_args )
        {
            auto& blk        = m_blocks[ m_cur.id ];
            blk.succs_offset = static_cast< uint32_t >( m_succs.size( ) );

            std::vector< ValueRef > ops;
            if ( cond.id != ~0u ) ops.push_back( cond );
            for ( const auto a : primary_args ) ops.push_back( a );
            for ( const auto a : secondary_args ) ops.push_back( a );

            m_succs.push_back( primary );
            if ( op == IROp::brif )
            {
                m_succs.push_back( secondary );
                blk.succs_count = 2;
            }
            else
            {
                blk.succs_count = 1;
            }

            emit( op, Type::unit, ops );
            m_block_inst_ends[ m_cur.id ] = static_cast< uint32_t >( m_insts.size( ) );
        }
    };
} // namespace xiao::ir
