//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <format>
#include <optional>
#include <string>
#include <unordered_map>
#include <xiao/support/arena.hh>
#include <xiao/ir/function.hh>

namespace xiao::ir
{
    //! @brief A module which owns the arena and interned strings.
    class IRModule
    {
    public:
        explicit IRModule( const std::size_t arena_size = support::Arena::default_slab_size ) : m_arena { arena_size } { }

        IRModule( IRModule&& other ) noexcept
            : m_arena { std::move( other.m_arena ) }, m_functions { std::move( other.m_functions ) },
              m_strings { std::move( other.m_strings ) }, m_lookup { std::move( other.m_lookup ) }
        { rebind_functions( ); }

        IRModule& operator= ( IRModule&& other ) noexcept
        {
            if ( this == &other ) return *this;
            m_arena     = std::move( other.m_arena );
            m_functions = std::move( other.m_functions );
            m_strings   = std::move( other.m_strings );
            m_lookup    = std::move( other.m_lookup );
            rebind_functions( );
            return *this;
        }

        IRModule( const IRModule& )             = delete;
        IRModule& operator= ( const IRModule& ) = delete;

        [[nodiscard]] FunctionRef declare( const std::string_view name, const Type ret, const std::span< const Type > params = { },
                                           const codegen::CallConv cc = codegen::host_call_conv( ) )
        {
            auto*      fn = m_arena.alloc< IRFunction >( );
            const auto id = static_cast< uint32_t >( m_functions.size( ) );
            fn->arena     = &m_arena;
            fn->self      = FunctionRef { id };
            fn->name      = intern( name ).first;
            fn->sig.ret   = ret;
            fn->sig.cc    = cc;
            if ( !params.empty( ) )
            {
                fn->sig.params = m_arena.alloc_array_uninit< Type >( params.size( ) );
                std::ranges::copy( params, fn->sig.params.begin( ) );
            }
            m_functions.push_back( fn );
            return FunctionRef { id };
        }

        [[nodiscard]] FunctionRef declare( const std::string_view name, const Type ret,
                                           const std::initializer_list< const Type > params = { },
                                           const codegen::CallConv                   cc     = codegen::host_call_conv( ) )
        { return declare( name, ret, std::span { params.begin( ), params.size( ) }, cc ); }

        //! @brief Intern a string; returns a StringRef + view pair.
        [[nodiscard]] std::pair< StringRef, std::string_view > intern( const std::string_view sv )
        {
            if ( const auto iter = m_lookup.find( sv ); iter != m_lookup.end( ) )
                return { StringRef { iter->second }, m_strings[ iter->second ] };

            auto       view = m_arena.alloc_str( sv );
            const auto id   = static_cast< uint32_t >( m_strings.size( ) );
            m_strings.push_back( view );
            m_lookup.emplace( view, id );
            return { StringRef { id }, view };
        }

        //! @brief Resolve a StringRef to its string_view.
        [[nodiscard]] std::string_view string( StringRef ref ) const noexcept
        { return m_strings[ ref.id ]; }

        [[nodiscard]] IRFunction* get( const FunctionRef ref ) noexcept
        { return m_functions[ ref.id ]; }

        [[nodiscard]] const IRFunction* get( const FunctionRef ref ) const noexcept
        { return m_functions[ ref.id ]; }

        [[nodiscard]] std::span< IRFunction* const > functions( ) const noexcept
        { return m_functions; }

        [[nodiscard]] support::Arena& arena( ) noexcept
        { return m_arena; }

        template < typename O >
        void fmt( O out ) const
        { fmt( out, []( const ExternalName n ) { return std::format( "u{}:{}", n.ns, n.index ); } ); }

        template < typename O, typename Names >
        void fmt( O out, Names&& names ) const
        {
            for ( auto i { 0uz }; i < m_functions.size( ); ++i )
            {
                if ( i ) std::format_to( out, "\n" );
                fmt_function( out, *m_functions[ i ], names );
            }
        }

        template < typename O, typename Names >
        void fmt_function( O out, const IRFunction& fn, Names&& names ) const
        {
            const auto ty     = [ & ]( const ValueRef v ) { return fn.values[ v.id ].type; };
            const auto op_at  = [ & ]( const IRInst& in, const uint16_t k ) { return fn.operands[ in.ops_offset + k ]; };
            const auto def_of = [ & ]( const ValueRef v ) -> const IRInst*
            {
                const auto id = fn.values[ v.id ].inst.id;
                return id < fn.insts.size( ) ? &fn.insts[ id ] : nullptr;
            };
            const auto is_const = [ & ]( const ValueRef v )
            {
                const auto* d = def_of( v );
                return d && d->op == IROp::iconst;
            };
            const auto cc_suffix = [ ]( const codegen::CallConv cc )
            { return cc == codegen::host_call_conv( ) ? std::string { } : std::format( " {}", codegen::to_string( cc ) ); };

            const auto expected = [ & ]( const IRBlock& b, const IRInst& in, const uint16_t k ) -> std::optional< Type >
            {
                const auto other = [ & ]( const uint16_t j ) -> std::optional< Type >
                {
                    if ( is_const( op_at( in, j ) ) ) return std::nullopt;
                    return ty( op_at( in, j ) );
                };

                switch ( in.op )
                {
                    case IROp::iadd:
                    case IROp::isub:
                    case IROp::imul:
                    case IROp::idiv:
                    case IROp::udiv:
                    case IROp::srem:
                    case IROp::urem:
                    case IROp::band:
                    case IROp::bor:
                    case IROp::bxor:
                    case IROp::ishl:
                    case IROp::ushr:
                    case IROp::sshr:
                    case IROp::icmp: return in.ops_count == 2 ? other( static_cast< uint16_t >( 1 - k ) ) : std::nullopt;
                    case IROp::select:
                        if ( k == 0 ) return Type::i64;
                        return other( static_cast< uint16_t >( 3 - k ) );
                    case IROp::store: return k == 0 ? std::optional { ty( op_at( in, 0 ) ) } : std::nullopt;
                    case IROp::ret: return fn.sig.ret;
                    case IROp::call:
                    {
                        const auto& e = fn.ext_funcs[ in.imm ];
                        return k < e.params_count ? std::optional { fn.ext_param_types[ e.params_offset + k ] } : std::nullopt;
                    }
                    case IROp::call_indirect:
                    {
                        const auto& s = fn.signatures[ in.imm ];
                        if ( k == 0 || k > s.params_count ) return std::nullopt;
                        return fn.ext_param_types[ s.params_offset + k - 1 ];
                    }
                    case IROp::br:
                    case IROp::brif:
                    {
                        if ( in.op == IROp::brif && k == 0 ) return Type::i64;
                        const auto  first  = in.op == IROp::brif ? 1u : 0u;
                        const auto* target = &fn.blocks[ fn.succs[ b.succs_offset ].id ];
                        auto        index  = k - first;
                        if ( index >= target->params_count )
                        {
                            index  -= target->params_count;
                            target  = &fn.blocks[ fn.succs[ b.succs_offset + 1 ].id ];
                        }
                        if ( index >= target->params_count ) return std::nullopt;
                        return ty( fn.block_params[ target->params_offset + index ] );
                    }
                    default: return std::nullopt;
                }
            };

            std::vector< uint8_t >  inlined( fn.values.size( ), 0 );
            std::vector< uint8_t >  blocked( fn.values.size( ), 0 );
            std::vector< uint32_t > uses( fn.values.size( ), 0 );
            for ( const auto& b : fn.blocks )
                for ( uint16_t ii { 0 }; ii < b.insts_count; ++ii )
                {
                    const auto& in = fn.insts[ b.insts_offset + ii ];
                    for ( uint16_t k { 0 }; k < in.ops_count; ++k )
                    {
                        const auto v = op_at( in, k );
                        ++uses[ v.id ];
                        if ( !is_const( v ) ) continue;
                        if ( const auto want = expected( b, in, k ); !want || *want != ty( v ) ) blocked[ v.id ] = 1;
                    }
                }

            for ( const auto& in : fn.insts )
            {
                if ( in.op == IROp::symbol_value || in.op == IROp::func_addr ) inlined[ in.result.id ] = 1;
                if ( in.op == IROp::iconst && uses[ in.result.id ] && !blocked[ in.result.id ] ) inlined[ in.result.id ] = 1;
            }

            const auto value = [ & ]( const ValueRef v ) -> std::string
            {
                if ( !inlined[ v.id ] ) return std::format( "%v{}", v.id );
                const auto& d = *def_of( v );
                if ( d.op == IROp::iconst ) return std::format( "{}", static_cast< int64_t >( d.imm ) );
                if ( d.op == IROp::func_addr ) return std::format( "@{}", names( fn.ext_funcs[ d.imm ].name ) );
                const auto& g = fn.global_values[ d.imm ];
                return std::format( "@{}{}", names( g.name ), g.offset ? std::format( "{:+}", g.offset ) : std::string { } );
            };

            const auto list = [ & ]( const std::span< const ValueRef > vs )
            {
                std::string s;
                for ( auto i { 0uz }; i < vs.size( ); ++i ) s += std::format( "{}{}", i ? ", " : "", value( vs[ i ] ) );
                return s;
            };

            const auto target = [ & ]( const uint32_t id, const std::span< const ValueRef > args )
            { return args.empty( ) ? std::format( "block{}", id ) : std::format( "block{}({})", id, list( args ) ); };

            const auto types = [ & ]( const uint32_t offset, const uint16_t count )
            {
                std::string s;
                for ( uint16_t i { 0 }; i < count; ++i ) s += std::format( "{}{}", i ? ", " : "", fn.ext_param_types[ offset + i ] );
                return s;
            };

            const auto offset = [ ]( const int32_t off ) { return off ? std::format( "{:+}", off ) : std::string { }; };

            std::format_to( out, "define @{}(", string( fn.name ) );
            if ( !fn.blocks.empty( ) )
            {
                const auto& entry = fn.blocks[ 0 ];
                for ( uint16_t i { 0 }; i < entry.params_count; ++i )
                {
                    const auto p = fn.block_params[ entry.params_offset + i ];
                    std::format_to( out, "{}%v{}: {}", i ? ", " : "", p.id, ty( p ) );
                }
            }
            else
                for ( auto i { 0uz }; i < fn.sig.params.size( ); ++i ) std::format_to( out, "{}{}", i ? ", " : "", fn.sig.params[ i ] );
            std::format_to( out, ")" );
            if ( fn.sig.ret != Type::unit ) std::format_to( out, " -> {}", fn.sig.ret );
            std::format_to( out, "{} {{\n", cc_suffix( fn.sig.cc ) );

            for ( auto si { 0uz }; si < fn.signatures.size( ); ++si )
            {
                const auto& s = fn.signatures[ si ];
                std::format_to( out, "    sig{} = ({})", si, types( s.params_offset, s.params_count ) );
                if ( s.ret != Type::unit ) std::format_to( out, " -> {}", s.ret );
                std::format_to( out, "{}\n", cc_suffix( s.cc ) );
            }
            for ( auto si { 0uz }; si < fn.stack_slots.size( ); ++si )
            {
                const auto& s = fn.stack_slots[ si ];
                std::format_to( out, "    ss{} = explicit_slot {}, align = {}\n", si, s.size, 1ull << s.align_shift );
            }
            if ( !fn.signatures.empty( ) || !fn.stack_slots.empty( ) ) std::format_to( out, "\n" );

            for ( auto bi { 0uz }; bi < fn.blocks.size( ); ++bi )
            {
                const auto& b = fn.blocks[ bi ];

                if ( bi ) std::format_to( out, "\n" );
                std::format_to( out, "block{}", bi );
                if ( bi && b.params_count )
                {
                    std::format_to( out, "(" );
                    for ( uint16_t pi { 0 }; pi < b.params_count; ++pi )
                    {
                        const auto p = fn.block_params[ b.params_offset + pi ];
                        std::format_to( out, "{}%v{}: {}", pi ? ", " : "", p.id, ty( p ) );
                    }
                    std::format_to( out, ")" );
                }
                std::format_to( out, ":\n" );

                for ( uint16_t ii { 0 }; ii < b.insts_count; ++ii )
                {
                    const auto& in  = fn.insts[ b.insts_offset + ii ];
                    const auto  res = std::format( "%v{} = ", in.result.id );
                    const auto  a   = [ & ]( const uint16_t k ) { return value( op_at( in, k ) ); };

                    if ( inlined[ in.result.id ] && ( in.op == IROp::iconst || in.op == IROp::symbol_value || in.op == IROp::func_addr ) )
                        continue;

                    std::format_to( out, "    " );
                    switch ( in.op )
                    {
                        case IROp::iconst:
                            std::format_to( out, "{}iconst.{} {}\n", res, ty( in.result ), static_cast< int64_t >( in.imm ) );
                            break;
                        case IROp::iadd:
                        case IROp::isub:
                        case IROp::imul:
                        case IROp::idiv:
                        case IROp::udiv:
                        case IROp::srem:
                        case IROp::urem:
                        case IROp::band:
                        case IROp::bor:
                        case IROp::bxor:
                        case IROp::ishl:
                        case IROp::ushr:
                        case IROp::sshr: std::format_to( out, "{}{} {}, {}\n", res, in.op, a( 0 ), a( 1 ) ); break;
                        case IROp::bnot:
                        case IROp::ineg: std::format_to( out, "{}{} {}\n", res, in.op, a( 0 ) ); break;
                        case IROp::uextend:
                        case IROp::sextend:
                        case IROp::ireduce: std::format_to( out, "{}{}.{} {}\n", res, in.op, ty( in.result ), a( 0 ) ); break;
                        case IROp::func_addr:
                        case IROp::symbol_value: break;
                        case IROp::stack_addr:
                            std::format_to( out, "{}stack_addr.{} ss{}{}\n", res, ty( in.result ), slot_of( in ).id,
                                            offset( slot_offset( in ) ) );
                            break;
                        case IROp::select: std::format_to( out, "{}select {}, {}, {}\n", res, a( 0 ), a( 1 ), a( 2 ) ); break;
                        case IROp::icmp: std::format_to( out, "{}icmp {} {}, {}\n", res, in.cc, a( 0 ), a( 1 ) ); break;
                        case IROp::load:
                            std::format_to( out, "{}load.{} {}{}\n", res, ty( in.result ), a( 0 ), offset( mem_offset( in ) ) );
                            break;
                        case IROp::store:
                        {
                            const auto v = op_at( in, 0 );
                            std::format_to( out, "store{} {}, {}{}\n", inlined[ v.id ] ? std::format( ".{}", ty( v ) ) : std::string { },
                                            a( 0 ), a( 1 ), offset( mem_offset( in ) ) );
                            break;
                        }
                        case IROp::call:
                        {
                            const auto& e = fn.ext_funcs[ in.imm ];
                            std::format_to( out, "{}call @{}({})\n", e.ret != Type::unit ? res : std::string { }, names( e.name ),
                                            list( fn.operands.subspan( in.ops_offset, in.ops_count ) ) );
                            break;
                        }
                        case IROp::call_indirect:
                            std::format_to( out, "{}call_indirect sig{}, {}({})\n",
                                            fn.signatures[ in.imm ].ret != Type::unit ? res : std::string { }, in.imm, a( 0 ),
                                            list( fn.operands.subspan( in.ops_offset + 1, in.ops_count - 1u ) ) );
                            break;
                        case IROp::br:
                        {
                            const auto [ then_args, else_args ] = branch_args( fn, b, in );
                            std::format_to( out, "jump {}\n", target( fn.succs[ b.succs_offset ].id, then_args ) );
                            break;
                        }
                        case IROp::brif:
                        {
                            const auto [ then_args, else_args ] = branch_args( fn, b, in );
                            std::format_to( out, "brif {}, {}, {}\n", a( 0 ), target( fn.succs[ b.succs_offset ].id, then_args ),
                                            target( fn.succs[ b.succs_offset + 1 ].id, else_args ) );
                            break;
                        }
                        case IROp::ret:
                            if ( in.ops_count > 0 ) std::format_to( out, "return {}\n", a( 0 ) );
                            else
                                std::format_to( out, "return\n" );
                            break;
                    }
                }
            }

            std::format_to( out, "}}\n" );
        }

    private:
        void rebind_functions( ) noexcept
        {
            for ( auto* fn : m_functions ) fn->arena = &m_arena;
        }

        support::Arena                                   m_arena;
        std::vector< IRFunction* >                       m_functions;
        std::vector< std::string_view >                  m_strings;
        std::unordered_map< std::string_view, uint32_t > m_lookup;
    };
} // namespace xiao::ir
