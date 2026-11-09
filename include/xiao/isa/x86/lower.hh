//
// Created by griffith on 19/10/2026.
//

#pragma once

#include <xiao/ir/visitor.hh>
#include <xiao/ir/function.hh>
#include <xiao/isa/x86/inst.hh>
#include <xiao/isa/x86/machenv.hh>
#include <xiao/isa/x86/reg.hh>
#include <xiao/codegen/abi.hh>
#include <xiao/codegen/machenv.hh>
#include <xiao/codegen/vcode.hh>

#include <algorithm>
#include <cassert>
#include <optional>
#include <span>
#include <vector>

namespace xiao::x86_64
{
    class IRLowerer : public ir::IRVisitor< IRLowerer >
    {
    public:
        explicit IRLowerer( const MachineEnv& env, const ABISpec& abi ) noexcept : m_env { env }, m_abi { abi } { }

        [[nodiscard]] VCode< X86Inst > lower( ir::IRFunction& fn )
        {
            m_fn        = &fn;
            m_vreg_c    = Reg::pinned_count;
            m_cur_block = 0;
            m_value_regs.clear( );
            m_term_succs.clear( );
            m_pending_edges.clear( );
            m_builder = codegen::VCodeBuilder< X86Inst > { };

            std::vector< ir::Type > ret_tys;
            if ( fn.sig.ret != ir::Type::unit ) ret_tys.push_back( fn.sig.ret );

            m_sig = m_abi.compute( fn.sig.cc, fn.sig.params, ret_tys );

            m_value_regs.resize( fn.values.size( ) );
            for ( auto i { 0u }; i < fn.values.size( ); ++i ) m_value_regs[ i ] = alloc_vreg( fn.values[ i ].type );

            m_block_map.resize( fn.blocks.size( ) );
            for ( auto i { 0u }; i < fn.blocks.size( ); ++i )
            {
                const auto& b = fn.blocks[ i ];

                std::vector< VReg > params { };
                for ( auto j { 0u }; j < b.params_count; ++j ) params.push_back( vreg_for( fn.block_params[ b.params_offset + j ] ) );

                m_block_map[ i ] = m_builder.start_block( params );
            }

            m_pred_count.assign( fn.blocks.size( ), 0 );
            for ( const auto& b : fn.blocks )
                for ( uint32_t s { 0 }; s < b.succs_count; ++s ) ++m_pred_count[ fn.succs[ b.succs_offset + s ].id ];

            m_needs_reg.assign( fn.values.size( ), false );
            for ( const auto& inst : fn.insts )
                for ( uint16_t i { 0 }; i < inst.ops_count; ++i )
                    if ( !fold_operand( inst, i ) ) m_needs_reg[ fn.operands[ inst.ops_offset + i ].id ] = true;

            find_fused_compares( fn );

            visit_function( fn );

            for ( const auto& edge : m_pending_edges )
            {
                m_builder.begin_block( edge.block );
                lower_block_args( edge.args, *edge.target );
                m_builder.push( inst::Jmp { .target = edge.target_id } );

                m_term_succs.clear( );
                m_term_succs.push_back( edge.target_id );
                m_builder.end_block( m_term_succs );
            }

            m_fn = nullptr;
            return std::move( m_builder ).finalise( );
        }

        void on_function( ir::IRFunction& ) { }

        void on_function_end( ir::IRFunction& ) { }

        void on_block( ir::IRFunction& fn, ir::IRBlock& b )
        {
            m_term_succs.clear( );

            const bool is_ent = m_cur_block == 0;
            m_builder.begin_block( m_block_map[ m_cur_block++ ] );
            if ( is_ent ) lower_params( fn, b );
        }

        void on_block_end( ir::IRFunction& fn, ir::IRBlock& block )
        { m_builder.end_block( m_term_succs ); }

        void on_inst( ir::IRFunction&, ir::IRBlock& block, ir::IRInst& inst )
        {
            using enum ir::IROp;
            switch ( inst.op )
            {
                case iconst: lower_iconst( inst ); break;
                case iadd: lower_binop( inst, AluOp::Add ); break;
                case isub: lower_binop( inst, AluOp::Sub ); break;
                case imul: lower_binop( inst, AluOp::Mul ); break;
                case ret: lower_ret( inst ); break;
                case icmp: lower_icmp( inst ); break;
                case br: lower_br( block, inst ); break;
                case brif: lower_brif( block, inst ); break;
                case idiv: lower_div( inst, true, false ); break;
                case udiv: lower_div( inst, false, false ); break;
                case srem: lower_div( inst, true, true ); break;
                case urem: lower_div( inst, false, true ); break;
                case call: lower_call( inst ); break;
                case load: lower_load( inst ); break;
                case store: lower_store( inst ); break;
                case band: lower_binop( inst, AluOp::And ); break;
                case bor: lower_binop( inst, AluOp::Or ); break;
                case bxor: lower_binop( inst, AluOp::Xor ); break;
                case bnot: lower_unary( inst, UnaryOp::Not ); break;
                case ineg: lower_unary( inst, UnaryOp::Neg ); break;
                case ishl: lower_shift( inst, ShiftOp::Shl ); break;
                case ushr: lower_shift( inst, ShiftOp::Shr ); break;
                case sshr: lower_shift( inst, ShiftOp::Sar ); break;
                case select: lower_select( inst ); break;
                case uextend: lower_extend( inst, false ); break;
                case sextend: lower_extend( inst, true ); break;
                case ireduce: lower_ireduce( inst ); break;
                case symbol_value: lower_symbol_value( inst ); break;
                case func_addr: lower_func_addr( inst ); break;
                case call_indirect: lower_call_indirect( inst ); break;
            }
        }

    private:
        struct PendingEdge
        {
            BlockId                         block { };
            std::span< const ir::ValueRef > args { };
            const ir::IRBlock*              target { };
            BlockId                         target_id { };
        };

        ir::IRFunction*            m_fn { nullptr };
        const MachineEnv&          m_env;
        const ABISpec&             m_abi;
        ABISig                     m_sig { };
        VCodeBuilder< X86Inst >    m_builder { };
        std::vector< VReg >        m_value_regs { };
        std::vector< BlockId >     m_block_map { };
        std::vector< BlockId >     m_term_succs { };
        std::vector< PendingEdge > m_pending_edges { };
        std::vector< bool >        m_needs_reg { };
        std::vector< bool >        m_fused { };
        std::vector< uint32_t >    m_pred_count { };
        uint32_t                   m_vreg_c { };
        uint32_t                   m_cur_block { };

        [[nodiscard]] VReg alloc_vreg( const ir::Type type ) noexcept
        {
            const auto rc = ( type == ir::Type::f64 ) ? RegClass::Float : RegClass::Int;
            const auto vr = VReg::make( m_vreg_c++, rc );
            m_builder.register_vreg( vr );
            return vr;
        }

        [[nodiscard]] VReg vreg_for( const ir::ValueRef ref ) const noexcept
        {
            assert( ref.id < m_value_regs.size( ) );
            return m_value_regs[ ref.id ];
        }

        [[nodiscard]] Reg reg_for( const ir::ValueRef ref ) const noexcept
        { return Reg::from_vreg( vreg_for( ref ) ); }

        [[nodiscard]] ir::ValueRef operand( const ir::IRInst& inst, const uint16_t i ) const noexcept
        { return m_fn->operands[ inst.ops_offset + i ]; }

        [[nodiscard]] ir::Type type_of( const ir::ValueRef ref ) const noexcept
        { return m_fn->values[ ref.id ].type; }

        [[nodiscard]] static OperandSize size_of( const ir::Type ty ) noexcept
        {
            switch ( ir::bit_width( ty ) )
            {
                case 8: return OperandSize::S8;
                case 16: return OperandSize::S16;
                case 32: return OperandSize::S32;
                default: return OperandSize::S64;
            }
        }

        void lower_params( const ir::IRFunction& fn, const ir::IRBlock& entry )
        {
            assert( entry.params_count == fn.sig.params.size( ) && "[xiao::lower::lower_params] entry block params must match the sig" );

            for ( uint16_t i { 0 }; i < entry.params_count; ++i )
            {
                const auto& loc = m_sig.params[ i ];
                assert( loc.is_reg( ) && "[xiao::lower::lower_params] stack passed params not support YET" );

                const auto val = fn.block_params[ entry.params_offset + i ];
                m_builder.push( inst::Mov { .dst = reg_for( val ), .src = Reg::from_preg( loc.reg( ) ) } );
            }
        }

        [[nodiscard]] std::optional< int64_t > const_value( const ir::ValueRef ref ) const noexcept
        {
            const auto def = m_fn->values[ ref.id ].inst;
            if ( def.id >= m_fn->insts.size( ) || m_fn->insts[ def.id ].op != ir::IROp::iconst ) return std::nullopt;
            return static_cast< int64_t >( m_fn->insts[ def.id ].imm );
        }

        [[nodiscard]] std::optional< int32_t > const_imm32( const ir::ValueRef ref ) const noexcept
        {
            const auto v = const_value( ref );
            if ( !v || *v < INT32_MIN || *v > INT32_MAX ) return std::nullopt;
            return static_cast< int32_t >( *v );
        }

        [[nodiscard]] std::optional< int32_t > fold_operand( const ir::IRInst& inst, const uint16_t i ) const noexcept
        {
            using enum ir::IROp;
            if ( inst.ops_count != 2 ) return std::nullopt;

            const auto lhs = m_fn->operands[ inst.ops_offset ];
            const auto rhs = m_fn->operands[ inst.ops_offset + 1 ];

            switch ( inst.op )
            {
                case iadd:
                case imul:
                case band:
                case bor:
                case bxor:
                case icmp:
                    if ( i == 1 ) return const_imm32( rhs );
                    if ( !const_imm32( rhs ) ) return const_imm32( lhs );
                    return std::nullopt;
                case isub: return i == 1 ? const_imm32( rhs ) : std::nullopt;
                case ishl:
                case ushr:
                case sshr:
                    if ( const auto v = const_value( rhs ); i == 1 && v ) return static_cast< int32_t >( *v & 63 );
                    return std::nullopt;
                default: return std::nullopt;
            }
        }

        [[nodiscard]] static ir::ICmpCC swap_cc( const ir::ICmpCC cc ) noexcept
        {
            using enum ir::ICmpCC;
            switch ( cc )
            {
                case slt: return sgt;
                case sle: return sge;
                case sgt: return slt;
                case sge: return sle;
                case ult: return ugt;
                case ule: return uge;
                case ugt: return ult;
                case uge: return ule;
                default: return cc;
            }
        }

        void lower_iconst( const ir::IRInst& inst )
        {
            if ( !m_needs_reg[ inst.result.id ] ) return;

            m_builder.push( inst::MovImm {
                .dst = reg_for( inst.result ),
                .imm = static_cast< int64_t >( inst.imm ),
            } );
        }

        void lower_binop( const ir::IRInst& inst, AluOp op )
        {
            const auto dst = reg_for( inst.result );

            if ( const auto imm = fold_operand( inst, 1 ) )
            {
                m_builder.push( inst::Mov { .dst = dst, .src = reg_for( operand( inst, 0 ) ) } );
                m_builder.push( inst::AluImm { .op = op, .dst = dst, .imm = *imm } );
                return;
            }

            if ( const auto imm = fold_operand( inst, 0 ) )
            {
                m_builder.push( inst::Mov { .dst = dst, .src = reg_for( operand( inst, 1 ) ) } );
                m_builder.push( inst::AluImm { .op = op, .dst = dst, .imm = *imm } );
                return;
            }

            m_builder.push( inst::Mov { .dst = dst, .src = reg_for( operand( inst, 0 ) ) } );
            m_builder.push( inst::Alu { .op = op, .dst = dst, .src = reg_for( operand( inst, 1 ) ) } );
        }

        void lower_ret( const ir::IRInst& inst )
        {
            if ( inst.ops_count > 0 )
            {
                assert( !m_sig.rets.empty( ) && m_sig.rets[ 0 ].is_reg( ) );

                const auto src      = reg_for( m_fn->operands[ inst.ops_offset ] );
                const auto ret_preg = m_sig.rets[ 0 ].reg( );

                m_builder.push( inst::Mov {
                    .dst = Reg::from_preg( ret_preg ),
                    .src = src,
                } );
            }

            m_builder.push( inst::Ret { } );
        }

        void find_fused_compares( const ir::IRFunction& fn )
        {
            std::vector< uint32_t > uses( fn.values.size( ), 0 );
            for ( const auto& inst : fn.insts )
                for ( uint16_t i { 0 }; i < inst.ops_count; ++i ) ++uses[ fn.operands[ inst.ops_offset + i ].id ];

            m_fused.assign( fn.values.size( ), false );
            for ( const auto& b : fn.blocks )
            {
                if ( b.insts_count == 0 ) continue;

                const auto& term = fn.insts[ b.insts_offset + b.insts_count - 1 ];
                if ( term.op != ir::IROp::brif ) continue;

                const auto cond = fn.operands[ term.ops_offset ];
                const auto def  = fn.values[ cond.id ].inst.id;
                if ( def < b.insts_offset || def >= b.insts_offset + b.insts_count ) continue;
                if ( fn.insts[ def ].op != ir::IROp::icmp || uses[ cond.id ] != 1 ) continue;

                m_fused[ cond.id ] = true;
            }
        }

        CC emit_cmp( const ir::IRInst& inst )
        {
            if ( const auto imm = fold_operand( inst, 1 ) )
            {
                m_builder.push( inst::AluImm { .op = AluOp::Cmp, .dst = reg_for( operand( inst, 0 ) ), .imm = *imm } );
                return icmp_cc( inst.cc );
            }

            if ( const auto imm = fold_operand( inst, 0 ) )
            {
                m_builder.push( inst::AluImm { .op = AluOp::Cmp, .dst = reg_for( operand( inst, 1 ) ), .imm = *imm } );
                return icmp_cc( swap_cc( inst.cc ) );
            }

            m_builder.push( inst::Alu { .op = AluOp::Cmp, .dst = reg_for( operand( inst, 0 ) ), .src = reg_for( operand( inst, 1 ) ) } );
            return icmp_cc( inst.cc );
        }

        void lower_icmp( const ir::IRInst& inst )
        {
            if ( m_fused[ inst.result.id ] ) return;

            const auto dst = reg_for( inst.result );
            const auto cc  = emit_cmp( inst );
            m_builder.push( inst::SetCC { .cc = cc, .dst = dst } );
            m_builder.push( inst::Movzx { .dst = dst, .src = dst, .size = OperandSize::S8 } );
        }

        void lower_br( ir::IRBlock& block, const ir::IRInst& inst )
        {
            const auto [ id ]  = m_fn->succs[ block.succs_offset ];
            const auto& target = m_fn->blocks[ id ];
            const auto  ref    = m_block_map[ id ];

            lower_block_args( ir::branch_args( *m_fn, block, inst ).then_args, target );
            m_builder.push( inst::Jmp { .target = ref } );
            m_term_succs.push_back( ref );
        }

        void lower_brif( ir::IRBlock& block, const ir::IRInst& inst )
        {
            const auto [ a_id ]                 = m_fn->succs[ block.succs_offset ];
            const auto [ b_id ]                 = m_fn->succs[ block.succs_offset + 1 ];
            const auto& then_b                  = m_fn->blocks[ a_id ];
            const auto& else_b                  = m_fn->blocks[ b_id ];
            const auto [ then_args, else_args ] = ir::branch_args( *m_fn, block, inst );
            const auto cond                     = m_fn->operands[ inst.ops_offset ];

            const auto then_id = m_block_map[ a_id ];
            const auto else_id = m_block_map[ b_id ];

            auto jcc_target = then_id;
            if ( !then_args.empty( ) || m_pred_count[ a_id ] > 1 )
            {
                jcc_target = m_builder.start_block( );
                m_pending_edges.push_back( { .block = jcc_target, .args = then_args, .target = &then_b, .target_id = then_id } );
            }

            auto cc = CC::NE;
            if ( m_fused[ cond.id ] ) cc = emit_cmp( m_fn->insts[ m_fn->values[ cond.id ].inst.id ] );
            else
                m_builder.push( inst::TestReg { .reg = reg_for( cond ), .size = size_of( type_of( cond ) ) } );
            m_builder.push( inst::Jcc { .cc = cc, .target = jcc_target } );
            lower_block_args( else_args, else_b );
            m_builder.push( inst::Jmp { .target = else_id } );

            m_term_succs.push_back( jcc_target );
            m_term_succs.push_back( else_id );
        }

        void lower_func_addr( const ir::IRInst& inst )
        {
            const auto& ext = m_fn->ext_funcs[ inst.imm ];
            m_builder.push( inst::LoadExtName {
                .dst      = reg_for( inst.result ),
                .name     = ext.name,
                .distance = ext.colocated ? RelocDistance::Near : RelocDistance::Far,
            } );
        }

        void lower_symbol_value( const ir::IRInst& inst )
        {
            const auto& gv = m_fn->global_values[ inst.imm ];
            m_builder.push( inst::LoadExtName {
                .dst      = reg_for( inst.result ),
                .name     = gv.name,
                .offset   = gv.offset,
                .distance = gv.colocated ? RelocDistance::Near : RelocDistance::Far,
            } );
        }

        void lower_load( const ir::IRInst& inst )
        {
            m_builder.push( inst::MovLoad {
                .dst  = reg_for( inst.result ),
                .mem  = { .base = reg_for( operand( inst, 0 ) ), .disp = ir::mem_offset( inst ) },
                .size = size_of( type_of( inst.result ) ),
            } );
        }

        void lower_store( const ir::IRInst& inst )
        {
            m_builder.push( inst::MovStore {
                .mem  = { .base = reg_for( operand( inst, 1 ) ), .disp = ir::mem_offset( inst ) },
                .src  = reg_for( operand( inst, 0 ) ),
                .size = size_of( type_of( operand( inst, 0 ) ) ),
            } );
        }

        void lower_div( const ir::IRInst& inst, const bool is_signed, const bool want_rem )
        {
            const auto lhs = reg_for( operand( inst, 0 ) );
            const auto rhs = reg_for( operand( inst, 1 ) );
            const auto res = reg_for( inst.result );

            constexpr auto rax = Reg::from_preg( RAX );
            constexpr auto rdx = Reg::from_preg( RDX );

            m_builder.push( inst::Mov { .dst = rax, .src = lhs } );
            if ( is_signed )
            {
                m_builder.push( inst::Cdq { .rax = rax, .rdx = rdx } );
                m_builder.push( inst::Idiv { .src = rhs, .rax = rax, .rdx = rdx } );
            }
            else
            {
                m_builder.push( inst::MovImm { .dst = rdx, .imm = 0 } );
                m_builder.push( inst::Div { .src = rhs, .rax = rax, .rdx = rdx } );
            }
            m_builder.push( inst::Mov { .dst = res, .src = want_rem ? rdx : rax } );
        }

        void lower_unary( const ir::IRInst& inst, const UnaryOp op )
        {
            const auto dst = reg_for( inst.result );
            m_builder.push( inst::Mov { .dst = dst, .src = reg_for( operand( inst, 0 ) ) } );
            m_builder.push( inst::Unary { .op = op, .dst = dst } );
        }

        void lower_shift( const ir::IRInst& inst, const ShiftOp op )
        {
            constexpr auto rcx = Reg::from_preg( RCX );

            const auto dst = reg_for( inst.result );
            m_builder.push( inst::Mov { .dst = dst, .src = reg_for( operand( inst, 0 ) ) } );

            if ( const auto imm = fold_operand( inst, 1 ) )
            {
                m_builder.push( inst::ShiftImm { .op = op, .dst = dst, .imm = static_cast< uint8_t >( *imm ) } );
                return;
            }

            m_builder.push( inst::Mov { .dst = rcx, .src = reg_for( operand( inst, 1 ) ) } );
            m_builder.push( inst::Shift { .op = op, .dst = dst, .count = rcx } );
        }

        void lower_select( const ir::IRInst& inst )
        {
            const auto dst = reg_for( inst.result );
            m_builder.push( inst::Mov { .dst = dst, .src = reg_for( operand( inst, 2 ) ) } );
            m_builder.push( inst::TestReg { .reg = reg_for( operand( inst, 0 ) ), .size = size_of( type_of( operand( inst, 0 ) ) ) } );
            m_builder.push( inst::CMov { .cc = CC::NE, .dst = dst, .src = reg_for( operand( inst, 1 ) ) } );
        }

        void lower_extend( const ir::IRInst& inst, const bool is_signed )
        {
            const auto dst  = reg_for( inst.result );
            const auto src  = reg_for( operand( inst, 0 ) );
            const auto size = size_of( type_of( operand( inst, 0 ) ) );

            if ( is_signed ) m_builder.push( inst::Movsx { .dst = dst, .src = src, .size = size } );
            else
                m_builder.push( inst::Movzx { .dst = dst, .src = src, .size = size } );
        }

        void lower_ireduce( const ir::IRInst& inst )
        { m_builder.push( inst::Mov { .dst = reg_for( inst.result ), .src = reg_for( operand( inst, 0 ) ) } ); }

        void lower_block_args( const std::span< const ir::ValueRef > args, const ir::IRBlock& target )
        {
            assert( args.size( ) == target.params_count );

            struct Copy
            {
                VReg     dst { };
                VReg     src { };
                ir::Type ty { };
            };

            std::vector< Copy > copies;
            copies.reserve( args.size( ) );
            for ( uint16_t i { 0 }; i < target.params_count; ++i )
            {
                const auto dst = vreg_for( m_fn->block_params[ target.params_offset + i ] );
                if ( const auto src = vreg_for( args[ i ] ); dst != src )
                    copies.push_back( { .dst = dst, .src = src, .ty = m_fn->values[ args[ i ].id ].type } );
            }

            const auto overlaps = std::ranges::any_of( copies,
                                                       [ & ]( const Copy& c )
                                                       {
                                                           return std::ranges::any_of( copies,
                                                                                       [ & ]( const Copy& d )
                                                                                       {
                                                                                           return c.src == d.dst;
                                                                                       } );
                                                       } );

            if ( !overlaps )
            {
                for ( const auto& c : copies )
                    m_builder.push( inst::Mov { .dst = Reg::from_vreg( c.dst ), .src = Reg::from_vreg( c.src ) } );
                return;
            }

            std::vector< VReg > temps;
            temps.reserve( copies.size( ) );
            for ( const auto& c : copies )
            {
                temps.push_back( alloc_vreg( c.ty ) );
                m_builder.push( inst::Mov { .dst = Reg::from_vreg( temps.back( ) ), .src = Reg::from_vreg( c.src ) } );
            }

            for ( std::size_t i { 0 }; i < copies.size( ); ++i )
                m_builder.push( inst::Mov { .dst = Reg::from_vreg( copies[ i ].dst ), .src = Reg::from_vreg( temps[ i ] ) } );
        }

        struct CallTarget
        {
            std::optional< ir::ExternalName > name { };
            Reg                               reg { };
        };

        void emit_call( const ir::IRInst& inst, const std::span< const ir::Type > params, const ir::Type ret, const codegen::CallConv cc,
                        const uint16_t first_arg, const CallTarget target )
        {
            std::vector< ir::Type > rets;
            if ( ret != ir::Type::unit ) rets.push_back( ret );

            const auto csig = m_abi.compute( cc, params, rets );

            std::vector< PReg > args;
            args.reserve( params.size( ) );
            for ( uint16_t i = 0; i < params.size( ); ++i )
            {
                assert( csig.params[ i ].is_reg( ) && "[xiao::lower::emit_call] stack passed args not supported YET" );
                args.push_back( csig.params[ i ].reg( ) );
                m_builder.push( inst::Mov { .dst = Reg::from_preg( args.back( ) ),
                                            .src = reg_for( m_fn->operands[ inst.ops_offset + first_arg + i ] ) } );
            }

            const auto ret_preg = rets.empty( ) ? PReg::invalid( ) : csig.rets[ 0 ].reg( );

            std::vector< PReg > clobbers;
            for ( const auto callee_env = env_for( cc ); const auto& cls : callee_env.preferred )
                clobbers.insert( clobbers.end( ), cls.begin( ), cls.end( ) );

            if ( target.name )
                m_builder.push(
                    inst::Call { .name = *target.name, .args = std::move( args ), .ret = ret_preg, .clobbers = std::move( clobbers ) } );
            else
                m_builder.push(
                    inst::CallInd { .target = target.reg, .args = std::move( args ), .ret = ret_preg, .clobbers = std::move( clobbers ) } );

            if ( ret_preg.valid( ) ) m_builder.push( inst::Mov { .dst = reg_for( inst.result ), .src = Reg::from_preg( ret_preg ) } );
        }

        void lower_call( const ir::IRInst& inst )
        {
            const auto& [ name, ret, params_offset, params_count, cc, colocated ] = m_fn->ext_funcs[ inst.imm ];
            const auto params = m_fn->ext_param_types.subspan( params_offset, params_count );

            if ( colocated )
            {
                emit_call( inst, params, ret, cc, 0, CallTarget { .name = name } );
                return;
            }

            const auto target = Reg::from_vreg( alloc_vreg( ir::Type::i64 ) );
            m_builder.push( inst::LoadExtName { .dst = target, .name = name } );
            emit_call( inst, params, ret, cc, 0, CallTarget { .reg = target } );
        }

        void lower_call_indirect( const ir::IRInst& inst )
        {
            const auto& sig    = m_fn->signatures[ inst.imm ];
            const auto  params = m_fn->ext_param_types.subspan( sig.params_offset, sig.params_count );
            emit_call( inst, params, sig.ret, sig.cc, 1, CallTarget { .reg = reg_for( m_fn->operands[ inst.ops_offset ] ) } );
        }

        [[nodiscard]] static constexpr CC icmp_cc( const ir::ICmpCC cc ) noexcept
        {
            switch ( cc )
            {
                case ir::ICmpCC::eq: return CC::E;
                case ir::ICmpCC::ne: return CC::NE;
                case ir::ICmpCC::slt: return CC::L;
                case ir::ICmpCC::sle: return CC::LE;
                case ir::ICmpCC::sgt: return CC::G;
                case ir::ICmpCC::sge: return CC::GE;
                case ir::ICmpCC::ult: return CC::B;
                case ir::ICmpCC::ule: return CC::BE;
                case ir::ICmpCC::ugt: return CC::A;
                case ir::ICmpCC::uge: return CC::AE;
            }
        }
    };
} // namespace xiao::x86_64
