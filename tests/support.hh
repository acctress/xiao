#pragma once

#include <xiao-jit/module.hh>
#include <xiao/codegen/context.hh>
#include <xiao/ir/builder.hh>
#include <xiao/ir/module.hh>
#include <xiao/ir/verifier.hh>
#include <xiao/isa/host.hh>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace xt
{
    using namespace xiao;
    using ir::Type;
    using ir::ValueRef;
    using Args = std::vector< ValueRef >;

    [[nodiscard]] inline std::vector< std::string > verify( ir::IRFunction& fn )
    {
        ir::IRVerifier v;
        v.visit_function( fn );
        std::vector< std::string > out;
        for ( const auto& e : v.errs ) out.push_back( e.msg );
        return out;
    }

    [[nodiscard]] inline std::string print( const ir::IRModule& m )
    {
        std::string out;
        m.fmt( std::back_inserter( out ) );
        return out;
    }

    [[nodiscard]] inline uint64_t mask( const Type ty, const uint64_t v ) noexcept
    {
        const auto w = ir::bit_width( ty );
        if ( w == 0 || w >= 64 ) return v;
        return v & ( ( uint64_t { 1 } << w ) - 1 );
    }

    [[nodiscard]] inline uint64_t sext( const Type ty, const uint64_t v ) noexcept
    {
        const auto w = ir::bit_width( ty );
        if ( w == 0 || w >= 64 ) return v;
        const auto shift = 64 - w;
        return static_cast< uint64_t >( static_cast< int64_t >( v << shift ) >> shift );
    }

    [[nodiscard]] inline const std::vector< int64_t >& edge_values( )
    {
        static const std::vector< int64_t > values {
            0,
            1,
            -1,
            2,
            -2,
            3,
            7,
            -7,
            42,
            127,
            128,
            -128,
            -129,
            255,
            256,
            32767,
            32768,
            -32768,
            65535,
            65536,
            0x7fffffff,
            -0x7fffffffLL - 1,
            0x80000000LL,
            0xffffffffLL,
            0x100000000LL,
            -0x80000001LL,
            0x123456789abcdefLL,
            -0x123456789abcdefLL,
            std::numeric_limits< int64_t >::max( ),
            std::numeric_limits< int64_t >::min( ),
            std::numeric_limits< int64_t >::max( ) - 1,
            std::numeric_limits< int64_t >::min( ) + 1,
        };
        return values;
    }

    class Jit
    {
    public:
        Jit( ) : Jit( jit::JITBuilder { isa::lookup_host( ) } ) { }

        explicit Jit( jit::JITBuilder builder ) : mod { std::move( builder ) } { }

        ir::IRModule   irm;
        jit::JITModule mod;

        [[nodiscard]] module::FuncId import( const std::string_view name, const Type ret, const std::vector< Type >& params )
        {
            const auto id = mod.declare_function( name, module::Linkage::Import,
                                                  module::Signature { .params = params, .ret = ret, .cc = codegen::host_call_conv( ) } );
            REQUIRE( id.has_value( ) );
            return *id;
        }

        [[nodiscard]] module::FuncId declare( const std::string_view name, const Type ret, const std::vector< Type >& params,
                                              const module::Linkage linkage = module::Linkage::Export )
        {
            const auto id = mod.declare_function( name, linkage,
                                                  module::Signature { .params = params, .ret = ret, .cc = codegen::host_call_conv( ) } );
            REQUIRE( id.has_value( ) );
            return *id;
        }

        template < typename Body >
        void build( const module::FuncId id, Body&& body )
        {
            const auto& decl = mod.declarations( ).get( id );
            const auto  ref  = irm.declare( decl.name, decl.sig.ret, std::span< const Type > { decl.sig.params }, decl.sig.cc );
            auto&       fn   = *irm.get( ref );

            {
                ir::IRBuilder b { fn };
                if constexpr ( std::is_invocable_v< Body, ir::IRBuilder&, const Args& > )
                {
                    const auto entry = b.add_block( std::span< const Type > { decl.sig.params } );
                    b.switch_to_block( entry );
                    Args args;
                    for ( uint16_t i { 0 }; i < decl.sig.params.size( ); ++i ) args.push_back( b.param( i ) );
                    body( b, args );
                }
                else
                    body( b );
                b.finalise( );
            }

            const auto errs = verify( fn );
            for ( const auto& e : errs ) UNSCOPED_INFO( e );
            REQUIRE( errs.empty( ) );

            codegen::Context ctx { fn };
            const auto       r = mod.define_function( id, ctx );
            REQUIRE( r.has_value( ) );
            last_code = ctx.compiled( ) ? *ctx.compiled( ) : codegen::CompiledCode { };
        }

        template < typename Body >
        module::FuncId define( const std::string_view name, const Type ret, const std::vector< Type >& params, Body&& body,
                               const module::Linkage linkage = module::Linkage::Export )
        {
            const auto id = declare( name, ret, params, linkage );
            build( id, std::forward< Body >( body ) );
            return id;
        }

        void finalise( )
        {
            const auto r = mod.finalise_definitions( );
            REQUIRE( r.has_value( ) );
        }

        template < typename F >
        [[nodiscard]] F* get( const module::FuncId id ) const
        { return mod.get_finalised_function< F >( id ); }

        codegen::CompiledCode last_code { };
    };

    template < typename F >
    struct Compiled;

    template < typename R, typename... A >
    struct Compiled< R( A... ) >
    {
        std::unique_ptr< Jit > jit;
        R ( *fn )( A... ) { };
        codegen::CompiledCode code { };

        R operator( ) ( A... a ) const
        { return fn( a... ); }
    };

    template < typename F, typename Body >
    [[nodiscard]] Compiled< F > compile( const Type ret, const std::vector< Type >& params, Body&& body )
    {
        Compiled< F > c;
        c.jit         = std::make_unique< Jit >( );
        const auto id = c.jit->define( "f", ret, params, std::forward< Body >( body ) );
        c.jit->finalise( );
        c.fn   = c.jit->template get< F >( id );
        c.code = c.jit->last_code;
        return c;
    }

    struct InterpError : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    struct Interp
    {
        std::function< uint64_t( uint32_t, std::span< const uint64_t > ) > call { };
        std::function< uint64_t( uint32_t ) >                              func_addr { };
        std::function< uint64_t( uint32_t ) >                              symbol_value { };
        uint64_t                                                           step_limit { 10'000'000 };

        [[nodiscard]] uint64_t run( const ir::IRFunction& fn, const std::span< const uint64_t > args ) const
        {
            std::vector< uint64_t > vals( fn.values.size( ), 0 );
            const auto              ty = [ & ]( const ValueRef v )
            {
                return fn.values[ v.id ].type;
            };
            const auto get = [ & ]( const ValueRef v )
            {
                return vals[ v.id ];
            };
            const auto op = [ & ]( const ir::IRInst& i, const uint16_t k )
            {
                return fn.operands[ i.ops_offset + k ];
            };

            const auto& entry = fn.blocks[ 0 ];
            if ( args.size( ) != entry.params_count ) throw InterpError { "argument count" };
            for ( uint16_t i { 0 }; i < entry.params_count; ++i )
            {
                const auto p = fn.block_params[ entry.params_offset + i ];
                vals[ p.id ] = mask( ty( p ), args[ i ] );
            }

            std::vector< uint64_t > slot_at( fn.stack_slots.size( ), 0 );
            uint64_t                frame_size { 0 };
            for ( auto s { 0uz }; s < fn.stack_slots.size( ); ++s )
            {
                const uint64_t align  = uint64_t { 1 } << fn.stack_slots[ s ].align_shift;
                frame_size            = ( frame_size + align - 1 ) & ~( align - 1 );
                slot_at[ s ]          = frame_size;
                frame_size           += fn.stack_slots[ s ].size;
            }
            std::vector< std::byte > frame( frame_size + 16 );
            const auto               frame_base = ( reinterpret_cast< uint64_t >( frame.data( ) ) + 15 ) & ~uint64_t { 15 };

            uint32_t block { 0 };
            uint64_t steps { 0 };
            for ( ;; )
            {
                const auto& b = fn.blocks[ block ];
                for ( uint16_t ii { 0 }; ii < b.insts_count; ++ii )
                {
                    if ( ++steps > step_limit ) throw InterpError { "step limit" };

                    const auto& i   = fn.insts[ b.insts_offset + ii ];
                    const auto  rty = fn.values[ i.result.id ].type;
                    auto        set = [ & ]( const uint64_t v )
                    {
                        vals[ i.result.id ] = mask( rty, v );
                    };

                    using enum ir::IROp;
                    switch ( i.op )
                    {
                        case iconst: set( i.imm ); break;
                        case iadd: set( get( op( i, 0 ) ) + get( op( i, 1 ) ) ); break;
                        case isub: set( get( op( i, 0 ) ) - get( op( i, 1 ) ) ); break;
                        case imul: set( get( op( i, 0 ) ) * get( op( i, 1 ) ) ); break;
                        case band: set( get( op( i, 0 ) ) & get( op( i, 1 ) ) ); break;
                        case bor: set( get( op( i, 0 ) ) | get( op( i, 1 ) ) ); break;
                        case bxor: set( get( op( i, 0 ) ) ^ get( op( i, 1 ) ) ); break;
                        case bnot: set( ~get( op( i, 0 ) ) ); break;
                        case ineg: set( uint64_t { 0 } - get( op( i, 0 ) ) ); break;
                        case ishl: set( get( op( i, 0 ) ) << ( get( op( i, 1 ) ) & 63 ) ); break;
                        case ushr: set( get( op( i, 0 ) ) >> ( get( op( i, 1 ) ) & 63 ) ); break;
                        case sshr:
                            set( static_cast< uint64_t >( static_cast< int64_t >( get( op( i, 0 ) ) ) >> ( get( op( i, 1 ) ) & 63 ) ) );
                            break;
                        case idiv:
                        case srem:
                        {
                            const auto a = static_cast< int64_t >( get( op( i, 0 ) ) );
                            const auto d = static_cast< int64_t >( get( op( i, 1 ) ) );
                            if ( d == 0 || ( d == -1 && a == std::numeric_limits< int64_t >::min( ) ) )
                                throw InterpError { "division trap" };
                            set( static_cast< uint64_t >( i.op == idiv ? a / d : a % d ) );
                            break;
                        }
                        case udiv:
                        case urem:
                        {
                            const auto a = get( op( i, 0 ) );
                            const auto d = get( op( i, 1 ) );
                            if ( d == 0 ) throw InterpError { "division trap" };
                            set( i.op == udiv ? a / d : a % d );
                            break;
                        }
                        case icmp:
                        {
                            const auto a  = get( op( i, 0 ) );
                            const auto c  = get( op( i, 1 ) );
                            const auto sa = static_cast< int64_t >( a );
                            const auto sc = static_cast< int64_t >( c );
                            bool       r { };
                            switch ( i.cc )
                            {
                                case ir::ICmpCC::eq: r = a == c; break;
                                case ir::ICmpCC::ne: r = a != c; break;
                                case ir::ICmpCC::slt: r = sa < sc; break;
                                case ir::ICmpCC::sle: r = sa <= sc; break;
                                case ir::ICmpCC::sgt: r = sa > sc; break;
                                case ir::ICmpCC::sge: r = sa >= sc; break;
                                case ir::ICmpCC::ult: r = a < c; break;
                                case ir::ICmpCC::ule: r = a <= c; break;
                                case ir::ICmpCC::ugt: r = a > c; break;
                                case ir::ICmpCC::uge: r = a >= c; break;
                            }
                            set( r ? 1 : 0 );
                            break;
                        }
                        case select: set( get( op( i, 0 ) ) != 0 ? get( op( i, 1 ) ) : get( op( i, 2 ) ) ); break;
                        case uextend: set( get( op( i, 0 ) ) ); break;
                        case sextend: set( sext( ty( op( i, 0 ) ), get( op( i, 0 ) ) ) ); break;
                        case ireduce: set( get( op( i, 0 ) ) ); break;
                        case load:
                        {
                            uint64_t   v { 0 };
                            const auto addr = get( op( i, 0 ) ) + static_cast< uint64_t >( static_cast< int64_t >( ir::mem_offset( i ) ) );
                            std::memcpy( &v, reinterpret_cast< const void* >( addr ), ir::bit_width( rty ) / 8 );
                            set( v );
                            break;
                        }
                        case store:
                        {
                            const auto v    = get( op( i, 0 ) );
                            const auto addr = get( op( i, 1 ) ) + static_cast< uint64_t >( static_cast< int64_t >( ir::mem_offset( i ) ) );
                            std::memcpy( reinterpret_cast< void* >( addr ), &v, ir::bit_width( ty( op( i, 0 ) ) ) / 8 );
                            break;
                        }
                        case call:
                        {
                            if ( !this->call ) throw InterpError { "no call handler" };
                            std::vector< uint64_t > a;
                            for ( uint16_t k { 0 }; k < i.ops_count; ++k ) a.push_back( get( op( i, k ) ) );
                            set( this->call( static_cast< uint32_t >( i.imm ), a ) );
                            break;
                        }
                        case call_indirect:
                        {
                            using Fn4 = uint64_t ( * )( uint64_t, uint64_t, uint64_t, uint64_t );
                            uint64_t a[ 4 ] { };
                            if ( i.ops_count > 5 ) throw InterpError { "too many indirect args" };
                            for ( uint16_t k { 1 }; k < i.ops_count; ++k ) a[ k - 1 ] = get( op( i, k ) );
                            const auto target = reinterpret_cast< Fn4 >( get( op( i, 0 ) ) );
                            set( target( a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ] ) );
                            break;
                        }
                        case func_addr:
                            if ( !this->func_addr ) throw InterpError { "no func_addr handler" };
                            set( this->func_addr( static_cast< uint32_t >( i.imm ) ) );
                            break;
                        case symbol_value:
                            if ( !this->symbol_value ) throw InterpError { "no symbol_value handler" };
                            set( this->symbol_value( static_cast< uint32_t >( i.imm ) )
                                 + static_cast< uint64_t >( fn.global_values[ i.imm ].offset ) );
                            break;
                        case stack_addr:
                            set( frame_base + slot_at[ ir::slot_of( i ).id ]
                                 + static_cast< uint64_t >( static_cast< int64_t >( ir::slot_offset( i ) ) ) );
                            break;
                        case ret: return i.ops_count ? get( op( i, 0 ) ) : 0;
                        case br:
                        case brif:
                        {
                            uint32_t target { fn.succs[ b.succs_offset ].id };
                            auto [ then_args, else_args ] = ir::branch_args( fn, b, i );
                            auto chosen                   = then_args;
                            if ( i.op == brif && get( op( i, 0 ) ) == 0 )
                            {
                                target = fn.succs[ b.succs_offset + 1 ].id;
                                chosen = else_args;
                            }
                            const auto&             tb = fn.blocks[ target ];
                            std::vector< uint64_t > incoming;
                            for ( const auto a : chosen ) incoming.push_back( get( a ) );
                            for ( uint16_t k { 0 }; k < tb.params_count; ++k )
                            {
                                const auto p = fn.block_params[ tb.params_offset + k ];
                                vals[ p.id ] = mask( ty( p ), incoming[ k ] );
                            }
                            block = target;
                            goto next_block;
                        }
                    }
                }
                throw InterpError { "fell off a block" };
            next_block:;
            }
        }
    };
} // namespace xt
