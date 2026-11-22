//
// Created by griffith on 19/10/2026.
//

#pragma once

#include <xiao/ir/visitor.hh>
#include <xiao/ir/function.hh>
#include <xiao/isa/x86/inst/inst.hh>
#include <xiao/isa/x86/inst/regs.hh>
#include <xiao/codegen/abi.hh>
#include <xiao/codegen/flags.hh>
#include <xiao/codegen/machenv.hh>
#include <xiao/codegen/vcode.hh>

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <span>
#include <vector>

namespace xiao::x86_64
{
    class IRLowerer : public ir::IRVisitor< IRLowerer >
    {
    public:
        explicit IRLowerer( const MachineEnv& env, const ABISpec& abi, const codegen::Flags& flags = { } ) noexcept
            : m_env { env }, m_abi { abi }, m_flags { flags }
        {
        }

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

            m_pred_count.assign( fn.blocks.size( ), 0 );
            for ( const auto& b : fn.blocks )
                for ( uint32_t s { 0 }; s < b.succs_count; ++s ) ++m_pred_count[ fn.succs[ b.succs_offset + s ].id ];

            m_block_map.resize( fn.blocks.size( ) );
            m_edges.assign( fn.blocks.size( ), { } );
            for ( auto i { 0u }; i < fn.blocks.size( ); ++i )
            {
                const auto& b = fn.blocks[ i ];

                std::vector< VReg > params { };
                for ( auto j { 0u }; j < b.params_count; ++j ) params.push_back( vreg_for( fn.block_params[ b.params_offset + j ] ) );

                m_block_map[ i ] = m_builder.start_block( params );
                reserve_edges( fn, i );
            }

            m_needs_reg.assign( fn.values.size( ), false );
            for ( const auto& inst : fn.insts )
                for ( uint16_t i { 0 }; i < inst.ops_count; ++i )
                    if ( !fold_operand( inst, i ) ) m_needs_reg[ fn.operands[ inst.ops_offset + i ].id ] = true;

            find_fused_compares( fn );

            visit_function( fn );

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

        void on_block_end( ir::IRFunction&, ir::IRBlock& )
        {
            m_builder.end_block( m_term_succs );

            for ( const auto& edge : m_pending_edges )
            {
                m_builder.begin_block( edge.block );
                lower_block_args( edge.args, *edge.target );
                m_builder.push( inst::Jmp { .target = edge.target_id } );

                const BlockId succ[] = { edge.target_id };
                m_builder.end_block( succ );
            }

            m_pending_edges.clear( );
        }

        void on_inst( ir::IRFunction&, ir::IRBlock& block, ir::IRInst& inst )
        {
            using enum ir::IROp;
            switch ( inst.op )
            {
                case iconst: lower_iconst( inst ); break;
                case iadd: lower_alu< inst::addq_mr, inst::addq_mi8, inst::addq_mi32 >( inst ); break;
                case isub: lower_alu< inst::subq_mr, inst::subq_mi8, inst::subq_mi32 >( inst ); break;
                case imul: lower_imul( inst ); break;
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
                case band: lower_alu< inst::andq_mr, inst::andq_mi8, inst::andq_mi32 >( inst ); break;
                case bor: lower_alu< inst::orq_mr, inst::orq_mi8, inst::orq_mi32 >( inst ); break;
                case bxor: lower_alu< inst::xorq_mr, inst::xorq_mi8, inst::xorq_mi32 >( inst ); break;
                case bnot: lower_unary< inst::notq_m >( inst ); break;
                case ineg: lower_unary< inst::negq_m >( inst ); break;
                case ishl:
                    lower_shift< Sized< inst::shlb_mc, inst::shlw_mc, inst::shll_mc, inst::shlq_mc >,
                                 Sized< inst::shlb_mi8, inst::shlw_mi8, inst::shll_mi8, inst::shlq_mi8 > >( inst );
                    break;
                case ushr:
                    lower_shift< Sized< inst::shrb_mc, inst::shrw_mc, inst::shrl_mc, inst::shrq_mc >,
                                 Sized< inst::shrb_mi8, inst::shrw_mi8, inst::shrl_mi8, inst::shrq_mi8 > >( inst );
                    break;
                case sshr:
                    lower_shift< Sized< inst::sarb_mc, inst::sarw_mc, inst::sarl_mc, inst::sarq_mc >,
                                 Sized< inst::sarb_mi8, inst::sarw_mi8, inst::sarl_mi8, inst::sarq_mi8 > >( inst );
                    break;
                case select: lower_select( inst ); break;
                case uextend: lower_extend( inst, false ); break;
                case sextend: lower_extend( inst, true ); break;
                case ireduce: lower_ireduce( inst ); break;
                case symbol_value: lower_symbol_value( inst ); break;
                case func_addr: lower_func_addr( inst ); break;
                case call_indirect: lower_call_indirect( inst ); break;
                case stack_addr: lower_stack_addr( inst ); break;
                case rotl:
                    lower_shift< Sized< inst::rolb_mc, inst::rolw_mc, inst::roll_mc, inst::rolq_mc >,
                                 Sized< inst::rolb_mi8, inst::rolw_mi8, inst::roll_mi8, inst::rolq_mi8 > >( inst );
                    break;
                case rotr:
                    lower_shift< Sized< inst::rorb_mc, inst::rorw_mc, inst::rorl_mc, inst::rorq_mc >,
                                 Sized< inst::rorb_mi8, inst::rorw_mi8, inst::rorl_mi8, inst::rorq_mi8 > >( inst );
                    break;
                case bswap: lower_bswap( inst ); break;
                case clz: lower_clz( inst ); break;
                case ctz: lower_ctz( inst ); break;
                case popcnt: lower_popcnt( inst ); break;
                case umulhi: lower_mulhi( inst, false ); break;
                case smulhi: lower_mulhi( inst, true ); break;
                case trap: m_builder.push( inst::Trap { .code = ir::trap_code( inst ) } ); break;
                case trapz: lower_trap_if( inst, CC::E ); break;
                case trapnz: lower_trap_if( inst, CC::NE ); break;
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

        template < typename B, typename W, typename L, typename Q >
        struct Sized
        {
            using b = B;
            using w = W;
            using l = L;
            using q = Q;
        };

        ir::IRFunction*                                          m_fn { nullptr };
        const MachineEnv&                                        m_env;
        const ABISpec&                                           m_abi;
        codegen::Flags                                           m_flags;
        ABISig                                                   m_sig { };
        VCodeBuilder< X86Inst >                                  m_builder { };
        std::vector< VReg >                                      m_value_regs { };
        std::vector< BlockId >                                   m_block_map { };
        std::vector< BlockId >                                   m_term_succs { };
        std::vector< PendingEdge >                               m_pending_edges { };
        std::vector< std::array< std::optional< BlockId >, 2 > > m_edges { };
        std::vector< bool >                                      m_needs_reg { };
        std::vector< bool >                                      m_fused { };
        std::vector< uint32_t >                                  m_pred_count { };
        uint32_t                                                 m_vreg_c { };
        uint32_t                                                 m_cur_block { };

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

        void mov( const Reg dst, const Reg src )
        { m_builder.push( inst::movq_mr { GprMem { dst }, src } ); }

        template < typename Imm8, typename Imm32 >
        void push_imm( const GprMem& dst, const int32_t imm )
        {
            if ( fits_i8( imm ) ) m_builder.push( Imm8 { dst, static_cast< int8_t >( imm ) } );
            else
                m_builder.push( Imm32 { dst, static_cast< decltype( Imm32::imm ) >( imm ) } ); //! let this function take narrow imms
        }

        template < typename S, typename... Args >
        void push_sized( const ir::Type ty, const Args&... args )
        {
            switch ( ir::bit_width( ty ) )
            {
                case 8: m_builder.push( typename S::b { args... } ); break;
                case 16: m_builder.push( typename S::w { args... } ); break;
                case 32: m_builder.push( typename S::l { args... } ); break;
                default: m_builder.push( typename S::q { args... } ); break;
            }
        }

        void push_test( const Reg r, const ir::Type ty )
        {
            switch ( ir::bit_width( ty ) )
            {
                case 8: m_builder.push( inst::testb_mr { GprMem { r }, r } ); break;
                case 16: m_builder.push( inst::testw_mr { GprMem { r }, r } ); break;
                case 32: m_builder.push( inst::testl_mr { GprMem { r }, r } ); break;
                default: m_builder.push( inst::testq_mr { GprMem { r }, r } ); break;
            }
        }

        void push_load( const Reg dst, const GprMem& mem, const ir::Type ty )
        {
            switch ( ir::bit_width( ty ) )
            {
                case 8: m_builder.push( inst::movzxq_rm8 { dst, mem } ); break;
                case 16: m_builder.push( inst::movzxq_rm16 { dst, mem } ); break;
                case 32: m_builder.push( inst::movl_rm { dst, mem } ); break;
                default: m_builder.push( inst::movq_rm { dst, mem } ); break;
            }
        }

        void lower_params( const ir::IRFunction& fn, const ir::IRBlock& entry )
        {
            assert( entry.params_count == fn.sig.params.size( ) && "[xiao::lower::lower_params] entry block params must match the sig" );

            for ( uint16_t i { 0 }; i < entry.params_count; ++i )
            {
                const auto& loc = m_sig.params[ i ];
                const auto  val = fn.block_params[ entry.params_offset + i ];

                if ( loc.is_reg( ) )
                {
                    mov( reg_for( val ), Reg::from_preg( loc.reg( ) ) );
                    continue;
                }

                push_load( reg_for( val ), GprMem { incoming_arg( loc.stack_offset( ) ) }, loc.ty );
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

        [[nodiscard]] static int64_t sext_to( const ir::Type ty, const int64_t value ) noexcept
        {
            const auto w = ir::bit_width( ty );
            if ( w >= 64 ) return value;

            const auto shft = 64 - w;
            return static_cast< int64_t >( static_cast< uint64_t >( value ) << shft ) >> shft;
        }

        [[nodiscard]] std::optional< int32_t > cmp_imm( const ir::Type ty, const ir::ValueRef ref ) const noexcept
        {
            if ( ir::bit_width( ty ) == 64 ) return const_imm32( ref );

            const auto value = const_value( ref );
            if ( !value ) return std::nullopt;

            return static_cast< int32_t >( sext_to( ty, *value ) );
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
                    if ( i == 1 ) return const_imm32( rhs );
                    if ( !const_imm32( rhs ) ) return const_imm32( lhs );
                    return std::nullopt;
                case icmp:
                {
                    const auto ty = type_of( lhs );
                    if ( i == 1 ) return cmp_imm( ty, rhs );
                    if ( !cmp_imm( ty, rhs ) ) return cmp_imm( ty, lhs );
                    return std::nullopt;
                }
                case isub: return i == 1 ? const_imm32( rhs ) : std::nullopt;
                case ishl:
                case ushr:
                case sshr:
                case rotl:
                case rotr:
                    if ( const auto v = const_value( rhs ); i == 1 && v )
                        return static_cast< int32_t >( *v & ir::bit_width( type_of( lhs ) ) - 1 );
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

        template < typename RegForm, typename Imm8Form, typename Imm32Form >
        void lower_alu( const ir::IRInst& inst )
        {
            const auto     dst      = reg_for( inst.result );
            const auto     imm_rhs  = fold_operand( inst, 1 );
            const auto     imm_lhs  = imm_rhs ? std::nullopt : fold_operand( inst, 0 );
            const uint16_t reg_side = imm_lhs ? 1 : 0;

            mov( dst, reg_for( operand( inst, reg_side ) ) );
            if ( const auto imm { imm_rhs ? imm_rhs : imm_lhs } ) push_imm< Imm8Form, Imm32Form >( GprMem { dst }, *imm );
            else
                m_builder.push( RegForm { GprMem { dst }, reg_for( operand( inst, 1 ) ) } );
        }

        void lower_imul( const ir::IRInst& inst )
        {
            const auto dst     = reg_for( inst.result );
            const auto imm_rhs = fold_operand( inst, 1 );
            const auto imm_lhs = imm_rhs ? std::nullopt : fold_operand( inst, 0 );

            if ( const auto imm { imm_rhs ? imm_rhs : imm_lhs } )
            {
                const GprMem src { reg_for( operand( inst, imm_lhs ? 1 : 0 ) ) };
                if ( fits_i8( *imm ) ) m_builder.push( inst::imulq_rmi8 { dst, src, static_cast< int8_t >( *imm ) } );
                else
                    m_builder.push( inst::imulq_rmi32 { dst, src, *imm } );
                return;
            }

            mov( dst, reg_for( operand( inst, 0 ) ) );
            m_builder.push( inst::imulq_rm { dst, GprMem { reg_for( operand( inst, 1 ) ) } } );
        }

        void lower_ret( const ir::IRInst& inst )
        {
            if ( inst.ops_count > 0 )
            {
                assert( !m_sig.rets.empty( ) && m_sig.rets[ 0 ].is_reg( ) );

                const auto src      = reg_for( m_fn->operands[ inst.ops_offset ] );
                const auto ret_preg = m_sig.rets[ 0 ].reg( );

                mov( Reg::from_preg( ret_preg ), src );
            }

            m_builder.push( inst::ret { } );
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

                const auto [ id ] = fn.operands[ term.ops_offset ];
                const auto def    = fn.values[ id ].inst.id;
                if ( def < b.insts_offset || def >= b.insts_offset + b.insts_count ) continue;
                if ( fn.insts[ def ].op != ir::IROp::icmp || uses[ id ] != 1 ) continue;

                m_fused[ id ] = true;
            }
        }

        void push_cmp_imm( const Reg r, const ir::Type ty, const int32_t imm )
        {
            const GprMem dst { r };
            switch ( ir::bit_width( ty ) )
            {
                case 8: m_builder.push( inst::cmpb_mi8 { dst, static_cast< int8_t >( imm ) } ); break;
                case 16: push_imm< inst::cmpw_mi8, inst::cmpw_mi16 >( dst, imm ); break;
                case 32: push_imm< inst::cmpl_mi8, inst::cmpl_mi32 >( dst, imm ); break;
                default: push_imm< inst::cmpq_mi8, inst::cmpq_mi32 >( dst, imm ); break;
            }
        }

        void push_cmp( const Reg a, const Reg b, const ir::Type ty )
        {
            switch ( ir::bit_width( ty ) )
            {
                case 8: m_builder.push( inst::cmpb_mr { GprMem { a }, b } ); break;
                case 16: m_builder.push( inst::cmpw_mr { GprMem { a }, b } ); break;
                case 32: m_builder.push( inst::cmpl_mr { GprMem { a }, b } ); break;
                default: m_builder.push( inst::cmpq_mr { GprMem { a }, b } ); break;
            }
        }

        CC emit_cmp( const ir::IRInst& inst )
        {
            const auto ty = type_of( operand( inst, 0 ) );

            if ( const auto imm = fold_operand( inst, 1 ) )
            {
                push_cmp_imm( reg_for( operand( inst, 0 ) ), ty, *imm );
                return icmp_cc( inst.cc );
            }

            if ( const auto imm = fold_operand( inst, 0 ) )
            {
                push_cmp_imm( reg_for( operand( inst, 1 ) ), ty, *imm );
                return icmp_cc( swap_cc( inst.cc ) );
            }

            push_cmp( reg_for( operand( inst, 0 ) ), reg_for( operand( inst, 1 ) ), ty );
            return icmp_cc( inst.cc );
        }

        void lower_icmp( const ir::IRInst& inst )
        {
            if ( m_fused[ inst.result.id ] ) return;

            const auto dst = reg_for( inst.result );
            const auto cc  = emit_cmp( inst );
            m_builder.push( inst::setcc_m8 { GprMem { dst }, cc } );
            m_builder.push( inst::movzxq_rm8 { dst, GprMem { dst } } );
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

        void reserve_edges( const ir::IRFunction& fn, const uint32_t i )
        {
            const auto& b = fn.blocks[ i ];
            if ( b.insts_count == 0 ) return;

            const auto& term = fn.insts[ b.insts_offset + b.insts_count - 1 ];
            if ( term.op != ir::IROp::brif ) return;

            const auto [ then_args, else_args ] = ir::branch_args( fn, b, term );
            for ( const uint32_t side : { 1u, 0u } )
            {
                const auto  target = fn.succs[ b.succs_offset + side ].id;
                const auto& params = fn.blocks[ target ];
                const auto  args   = side ? else_args : then_args;

                bool copies { false };
                for ( uint16_t j { 0 }; j < args.size( ); ++j ) copies |= args[ j ].id != fn.block_params[ params.params_offset + j ].id;

                if ( copies || m_pred_count[ target ] > 1 ) m_edges[ i ][ side ] = m_builder.start_block( );
            }
        }

        void lower_brif( ir::IRBlock& block, const ir::IRInst& inst )
        {
            const auto [ then_args, else_args ] = ir::branch_args( *m_fn, block, inst );
            const auto  cond                    = m_fn->operands[ inst.ops_offset ];
            const auto& edges                   = m_edges[ m_cur_block - 1 ];

            std::array< BlockId, 2 > targets;
            for ( const uint32_t side : { 1u, 0u } )
            {
                const auto id   = m_fn->succs[ block.succs_offset + side ].id;
                targets[ side ] = edges[ side ].value_or( m_block_map[ id ] );
                if ( edges[ side ] )
                    m_pending_edges.push_back( { .block     = *edges[ side ],
                                                 .args      = side ? else_args : then_args,
                                                 .target    = &m_fn->blocks[ id ],
                                                 .target_id = m_block_map[ id ] } );
            }

            auto cc = CC::NE;
            if ( m_fused[ cond.id ] ) cc = emit_cmp( m_fn->insts[ m_fn->values[ cond.id ].inst.id ] );
            else
                push_test( reg_for( cond ), type_of( cond ) );
            m_builder.push( inst::Jcc { .cc = cc, .taken = targets[ 0 ], .not_taken = targets[ 1 ] } );

            m_term_succs.push_back( targets[ 0 ] );
            m_term_succs.push_back( targets[ 1 ] );
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

        void lower_stack_addr( const ir::IRInst& inst )
        {
            const auto slot = static_cast< int32_t >( ir::slot_of( inst ).id );
            m_builder.push( inst::leaq_rm { .dst = reg_for( inst.result ), .src = stack_slot( slot, ir::slot_offset( inst ) ) } );
        }

        void lower_load( const ir::IRInst& inst )
        {
            const auto   dst = reg_for( inst.result );
            const GprMem mem {
                AddressMode { .base = reg_for( operand( inst, 0 ) ), .disp = ir::mem_offset( inst ) }
            };

            push_load( dst, mem, type_of( inst.result ) );
        }

        void lower_store( const ir::IRInst& inst )
        {
            const auto   src = reg_for( operand( inst, 0 ) );
            const GprMem mem {
                AddressMode { .base = reg_for( operand( inst, 1 ) ), .disp = ir::mem_offset( inst ) }
            };

            switch ( ir::bit_width( type_of( operand( inst, 0 ) ) ) )
            {
                case 8: m_builder.push( inst::movb_mr { mem, src } ); break;
                case 16: m_builder.push( inst::movw_mr { mem, src } ); break;
                case 32: m_builder.push( inst::movl_mr { mem, src } ); break;
                default: m_builder.push( inst::movq_mr { mem, src } ); break;
            }
        }

        void lower_div( const ir::IRInst& inst, const bool is_signed, const bool want_rem )
        {
            const auto lhs = reg_for( operand( inst, 0 ) );
            const auto rhs = reg_for( operand( inst, 1 ) );
            const auto res = reg_for( inst.result );

            constexpr auto rax = Reg::from_preg( RAX );
            constexpr auto rdx = Reg::from_preg( RDX );

            mov( rax, lhs );
            if ( is_signed )
            {
                m_builder.push( inst::cqo { rax, rdx } );
                m_builder.push( inst::idivq_m { GprMem { rhs }, rax, rdx } );
            }
            else
            {
                m_builder.push( inst::MovImm { .dst = rdx, .imm = 0 } );
                m_builder.push( inst::divq_m { GprMem { rhs }, rax, rdx } );
            }
            mov( res, want_rem ? rdx : rax );
        }

        template < typename Op >
        void lower_unary( const ir::IRInst& inst )
        {
            const auto dst = reg_for( inst.result );
            mov( dst, reg_for( operand( inst, 0 ) ) );
            m_builder.push( Op { GprMem { dst } } );
        }

        template < typename ByCl, typename ByImm >
        void lower_shift( const ir::IRInst& inst )
        {
            constexpr auto rcx = Reg::from_preg( RCX );

            const auto ty  = type_of( inst.result );
            const auto dst = reg_for( inst.result );
            mov( dst, reg_for( operand( inst, 0 ) ) );

            if ( const auto imm = fold_operand( inst, 1 ) )
            {
                push_sized< ByImm >( ty, GprMem { dst }, static_cast< int8_t >( *imm ) );
                return;
            }

            mov( rcx, reg_for( operand( inst, 1 ) ) );
            if ( const auto bits = ir::bit_width( ty ); bits < 32 )
                m_builder.push( inst::andl_mi8 { GprMem { rcx }, static_cast< int8_t >( bits - 1 ) } );

            push_sized< ByCl >( ty, GprMem { dst }, rcx );
        }

        [[nodiscard]] Reg temp( ) noexcept
        { return Reg::from_vreg( alloc_vreg( ir::Type::i64 ) ); }

        void extend_to_64( const Reg dst, const Reg src, const ir::Type ty, const bool is_signed )
        {
            const GprMem from { src };
            switch ( ir::bit_width( ty ) )
            {
                case 8:
                    if ( is_signed ) m_builder.push( inst::movsxq_rm8 { dst, from } );
                    else
                        m_builder.push( inst::movzxq_rm8 { dst, from } );
                    break;
                case 16:
                    if ( is_signed ) m_builder.push( inst::movsxq_rm16 { dst, from } );
                    else
                        m_builder.push( inst::movzxq_rm16 { dst, from } );
                    break;
                case 32:
                    if ( is_signed ) m_builder.push( inst::movsxdq_rm32 { dst, from } );
                    else
                        m_builder.push( inst::movl_rm { dst, from } );
                    break;
                default: mov( dst, src ); break;
            }
        }

        void lower_bswap( const ir::IRInst& inst )
        {
            const auto dst = reg_for( inst.result );
            mov( dst, reg_for( operand( inst, 0 ) ) );

            switch ( ir::bit_width( type_of( inst.result ) ) )
            {
                case 16: m_builder.push( inst::rolw_mi8 { GprMem { dst }, 8 } ); break;
                case 32: m_builder.push( inst::bswapl_r { dst } ); break;
                default: m_builder.push( inst::bswapq_r { dst } ); break;
            }
        }

        void lower_clz( const ir::IRInst& inst )
        {
            const auto ty   = type_of( inst.result );
            const auto bits = ir::bit_width( ty );
            const auto dst  = reg_for( inst.result );
            const auto src  = reg_for( operand( inst, 0 ) );

            if ( m_flags.has_lzcnt )
            {
                if ( bits == 8 )
                {
                    const auto wide = temp( );
                    extend_to_64( wide, src, ty, false );
                    m_builder.push( inst::lzcntl_rm { dst, GprMem { wide } } );
                    m_builder.push( inst::subl_mi8 { GprMem { dst }, 24 } );
                    return;
                }

                push_sized< Sized< inst::lzcntw_rm, inst::lzcntw_rm, inst::lzcntl_rm, inst::lzcntq_rm > >( ty, dst, GprMem { src } );
                return;
            }

            auto wide = src;
            if ( bits < 64 )
            {
                wide = temp( );
                extend_to_64( wide, src, ty, false );
            }

            const auto idx = temp( );
            m_builder.push( inst::MovImm { .dst = dst, .imm = -1 } );
            m_builder.push( inst::bsrq_rm { idx, GprMem { wide } } );
            m_builder.push( inst::cmovq_rm { idx, GprMem { dst }, CC::E } );
            m_builder.push( inst::MovImm { .dst = dst, .imm = bits - 1 } );
            m_builder.push( inst::subq_mr { GprMem { dst }, idx } );
        }

        void lower_ctz( const ir::IRInst& inst )
        {
            const auto ty   = type_of( inst.result );
            const auto bits = ir::bit_width( ty );
            const auto dst  = reg_for( inst.result );
            const auto src  = reg_for( operand( inst, 0 ) );

            if ( m_flags.has_bmi1 && bits > 8 )
            {
                push_sized< Sized< inst::tzcntw_rm, inst::tzcntw_rm, inst::tzcntl_rm, inst::tzcntq_rm > >( ty, dst, GprMem { src } );
                return;
            }

            if ( bits < 64 )
            {
                const auto wide = temp( );
                extend_to_64( wide, src, ty, false );
                m_builder.push( inst::btsq_mi8 { GprMem { wide }, static_cast< int8_t >( bits ) } );
                m_builder.push( inst::bsfq_rm { dst, GprMem { wide } } );
                return;
            }

            const auto none = temp( );
            m_builder.push( inst::MovImm { .dst = none, .imm = 64 } );
            m_builder.push( inst::bsfq_rm { dst, GprMem { src } } );
            m_builder.push( inst::cmovq_rm { dst, GprMem { none }, CC::E } );
        }

        void lower_popcnt( const ir::IRInst& inst )
        {
            const auto ty   = type_of( inst.result );
            const auto bits = ir::bit_width( ty );
            const auto dst  = reg_for( inst.result );
            const auto src  = reg_for( operand( inst, 0 ) );

            if ( m_flags.has_popcnt )
            {
                if ( bits == 8 )
                {
                    const auto wide = temp( );
                    extend_to_64( wide, src, ty, false );
                    m_builder.push( inst::popcntl_rm { dst, GprMem { wide } } );
                    return;
                }

                push_sized< Sized< inst::popcntw_rm, inst::popcntw_rm, inst::popcntl_rm, inst::popcntq_rm > >( ty, dst, GprMem { src } );
                return;
            }

            const auto t = temp( );
            const auto k = temp( );
            extend_to_64( dst, src, ty, false );

            mov( t, dst );
            m_builder.push( inst::shrq_mi8 { GprMem { t }, 1 } );
            m_builder.push( inst::MovImm { .dst = k, .imm = static_cast< int64_t >( 0x5555555555555555ull ) } );
            m_builder.push( inst::andq_mr { GprMem { t }, k } );
            m_builder.push( inst::subq_mr { GprMem { dst }, t } );

            m_builder.push( inst::MovImm { .dst = k, .imm = static_cast< int64_t >( 0x3333333333333333ull ) } );
            mov( t, dst );
            m_builder.push( inst::shrq_mi8 { GprMem { t }, 2 } );
            m_builder.push( inst::andq_mr { GprMem { t }, k } );
            m_builder.push( inst::andq_mr { GprMem { dst }, k } );
            m_builder.push( inst::addq_mr { GprMem { dst }, t } );

            mov( t, dst );
            m_builder.push( inst::shrq_mi8 { GprMem { t }, 4 } );
            m_builder.push( inst::addq_mr { GprMem { dst }, t } );
            m_builder.push( inst::MovImm { .dst = k, .imm = static_cast< int64_t >( 0x0F0F0F0F0F0F0F0Full ) } );
            m_builder.push( inst::andq_mr { GprMem { dst }, k } );

            m_builder.push( inst::MovImm { .dst = k, .imm = static_cast< int64_t >( 0x0101010101010101ull ) } );
            m_builder.push( inst::imulq_rm { dst, GprMem { k } } );
            m_builder.push( inst::shrq_mi8 { GprMem { dst }, 56 } );
        }

        void lower_mulhi( const ir::IRInst& inst, const bool is_signed )
        {
            const auto ty   = type_of( inst.result );
            const auto bits = ir::bit_width( ty );
            const auto dst  = reg_for( inst.result );
            const auto lhs  = reg_for( operand( inst, 0 ) );
            const auto rhs  = reg_for( operand( inst, 1 ) );

            if ( bits == 64 )
            {
                constexpr auto rax = Reg::from_preg( RAX );
                constexpr auto rdx = Reg::from_preg( RDX );

                mov( rax, lhs );
                m_builder.push( inst::MovImm { .dst = rdx, .imm = 0 } );
                if ( is_signed ) m_builder.push( inst::imulq_m { GprMem { rhs }, rax, rdx } );
                else
                    m_builder.push( inst::mulq_m { GprMem { rhs }, rax, rdx } );
                mov( dst, rdx );
                return;
            }

            const auto t = temp( );
            extend_to_64( dst, lhs, ty, is_signed );
            extend_to_64( t, rhs, ty, is_signed );
            m_builder.push( inst::imulq_rm { dst, GprMem { t } } );
            if ( is_signed ) m_builder.push( inst::sarq_mi8 { GprMem { dst }, static_cast< int8_t >( bits ) } );
            else
                m_builder.push( inst::shrq_mi8 { GprMem { dst }, static_cast< int8_t >( bits ) } );
        }

        void lower_trap_if( const ir::IRInst& inst, const CC cc )
        {
            push_test( reg_for( operand( inst, 0 ) ), type_of( operand( inst, 0 ) ) );
            m_builder.push( inst::TrapIf { .cc = cc, .code = ir::trap_code( inst ) } );
        }

        void lower_select( const ir::IRInst& inst )
        {
            const auto dst = reg_for( inst.result );
            mov( dst, reg_for( operand( inst, 2 ) ) );
            push_test( reg_for( operand( inst, 0 ) ), type_of( operand( inst, 0 ) ) );
            m_builder.push( inst::cmovq_rm { dst, GprMem { reg_for( operand( inst, 1 ) ) }, CC::NE } );
        }

        void lower_extend( const ir::IRInst& inst, const bool is_signed )
        {
            const auto   dst = reg_for( inst.result );
            const auto   reg = reg_for( operand( inst, 0 ) );
            const GprMem src { reg };

            switch ( ir::bit_width( type_of( operand( inst, 0 ) ) ) )
            {
                case 8:
                    if ( is_signed ) m_builder.push( inst::movsxq_rm8 { dst, src } );
                    else
                        m_builder.push( inst::movzxq_rm8 { dst, src } );
                    break;
                case 16:
                    if ( is_signed ) m_builder.push( inst::movsxq_rm16 { dst, src } );
                    else
                        m_builder.push( inst::movzxq_rm16 { dst, src } );
                    break;
                case 32:
                    if ( is_signed ) m_builder.push( inst::movsxdq_rm32 { dst, src } );
                    else
                        m_builder.push( inst::movl_rm { dst, src } );
                    break;
                default: mov( dst, reg ); break;
            }
        }

        void lower_ireduce( const ir::IRInst& inst )
        { mov( reg_for( inst.result ), reg_for( operand( inst, 0 ) ) ); }

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
                for ( const auto& c : copies ) mov( Reg::from_vreg( c.dst ), Reg::from_vreg( c.src ) );
                return;
            }

            std::vector< VReg > temps;
            temps.reserve( copies.size( ) );
            for ( const auto& c : copies )
            {
                temps.push_back( alloc_vreg( c.ty ) );
                mov( Reg::from_vreg( temps.back( ) ), Reg::from_vreg( c.src ) );
            }

            for ( std::size_t i { 0 }; i < copies.size( ); ++i ) mov( Reg::from_vreg( copies[ i ].dst ), Reg::from_vreg( temps[ i ] ) );
        }

        struct CallTarget
        {
            std::optional< ir::ExternalName > name { };
            Reg                               reg { };
        };

        void emit_call( const ir::IRInst& inst, const std::span< const ir::Type > params, const ir::Type ret, const CallConv cc,
                        const uint16_t first_arg, const CallTarget target )
        {
            std::vector< ir::Type > rets;
            if ( ret != ir::Type::unit ) rets.push_back( ret );

            const auto csig = m_abi.compute( cc, params, rets );

            const auto arg = [ & ]( const std::size_t idx )
            {
                return reg_for( m_fn->operands[ inst.ops_offset + first_arg + idx ] );
            };

            for ( auto i { 0uz }; i < params.size( ); ++i )
                if ( const auto& loc = csig.params[ i ]; loc.is_stack( ) )
                    m_builder.push( inst::movq_mr { GprMem { outgoing_arg( loc.stack_offset( ) ) }, arg( i ) } );

            std::vector< PReg > args;
            args.reserve( params.size( ) );
            for ( auto i { 0uz }; i < params.size( ); ++i )
            {
                const auto& loc = csig.params[ i ];
                if ( !loc.is_reg( ) ) continue;
                args.push_back( loc.reg( ) );
                mov( Reg::from_preg( loc.reg( ) ), arg( i ) );
            }

            const auto ret_preg = rets.empty( ) ? PReg::invalid( ) : csig.rets[ 0 ].reg( );

            std::vector< PReg > clobbers;
            for ( const auto callee_env = env_for( cc ); const auto& cls : callee_env.preferred )
                clobbers.insert( clobbers.end( ), cls.begin( ), cls.end( ) );

            support::Box info {
                inst::CallInfo { .args            = std::move( args ),
                                .ret             = ret_preg,
                                .clobbers        = std::move( clobbers ),
                                .cc              = cc,
                                .stack_arg_bytes = csig.stack_arg_bytes }
            };

            if ( target.name ) m_builder.push( inst::Call { .name = *target.name, .info = std::move( info ) } );
            else
                m_builder.push( inst::CallInd { .target = target.reg, .info = std::move( info ) } );

            if ( ret_preg.valid( ) ) mov( reg_for( inst.result ), Reg::from_preg( ret_preg ) );
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
