#include "support.hh"

#include <catch2/generators/catch_generators.hpp>

#include <cstdint>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace xt;

namespace
{
    constexpr auto cc = codegen::host_call_conv( );

    using Errs = std::vector< std::string >;

    struct Fn
    {
        ir::IRModule    irm;
        ir::IRFunction* fn { };

        Fn( const Type ret, const std::vector< Type >& params ) :
            fn { irm.get( irm.declare( "f", ret, std::span< const Type > { params } ) ) }
        {
        }

        template < typename Body >
        Fn& build( Body&& body )
        {
            ir::IRBuilder b { *fn };
            body( b );
            b.finalise( );
            return *this;
        }

        [[nodiscard]] Errs errors( ) const
        { return verify( *fn ); }

        [[nodiscard]] ir::IRInst& inst( const ValueRef v ) const
        { return fn->insts[ fn->values[ v.id ].inst.id ]; }
    };

    template < typename Body >
    [[nodiscard]] Errs errors_of( const Type ret, const std::vector< Type >& params, Body&& body )
    {
        Fn f { ret, params };
        f.build(
            [ & ]( ir::IRBuilder& b )
            {
                b.switch_to_block( b.add_block( std::span< const Type > { params } ) );
                body( b );
            } );
        return f.errors( );
    }

    struct Recorder : ir::IRVisitor< Recorder >
    {
        std::vector< std::string > log;

        void on_function( ir::IRFunction& fn )
        { log.push_back( std::format( "fn {}", fn.self.id ) ); }

        void on_function_end( ir::IRFunction& fn )
        { log.push_back( std::format( "fn_end {}", fn.self.id ) ); }

        void on_block( ir::IRFunction&, ir::IRBlock& b )
        { log.push_back( std::format( "block {}", b.insts_offset ) ); }

        void on_block_end( ir::IRFunction&, ir::IRBlock& )
        { log.push_back( "block_end" ); }

        void on_inst( ir::IRFunction&, ir::IRBlock&, ir::IRInst& i )
        { log.push_back( std::format( "inst {}", i.op ) ); }

        void on_operand( ir::IRFunction&, ir::IRBlock&, ir::IRInst&, const ValueRef v )
        { log.push_back( std::format( "op v{}", v.id ) ); }
    };

    struct InstCounter : ir::IRVisitor< InstCounter >
    {
        uint32_t insts { };

        void on_inst( ir::IRFunction&, ir::IRBlock&, ir::IRInst& )
        { ++insts; }
    };

    struct ConstRewriter : ir::IRVisitor< ConstRewriter >
    {
        void on_inst( ir::IRFunction&, ir::IRBlock&, ir::IRInst& i )
        {
            if ( i.op == ir::IROp::iconst ) i.imm = 99;
        }
    };
} // namespace

TEST_CASE( "verifier accepts a function using every op", "[verifier]" )
{
    Fn f {
        Type::i64,
        { Type::i64, Type::i64, Type::ptr, Type::i32, Type::i8 }
    };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto callee = b.import_function( { ir::extns::function, 0 }, { Type::i64, Type::ptr }, Type::i64, cc, true );
            const auto sig    = b.import_signature( { Type::i64, Type::ptr }, Type::i64, cc );
            const auto gv     = b.create_global_value( {
                    .name      = { ir::extns::data, 0 },
                    .offset    = 8,
                    .colocated = true
            } );

            const auto entry = b.add_block( { Type::i64, Type::i64, Type::ptr, Type::i32, Type::i8 } );
            const auto then  = b.add_block( { Type::i64 } );
            const auto other = b.add_block( { Type::i64, Type::i32 } );
            const auto join  = b.add_block( { Type::i64 } );

            b.switch_to_block( entry );
            const auto x   = b.param( 0 );
            const auto y   = b.param( 1 );
            const auto p   = b.param( 2 );
            const auto w   = b.param( 3 );
            const auto c8  = b.param( 4 );
            auto       acc = b.iadd( x, y );
            acc            = b.isub( acc, b.iconst( Type::i64, 3 ) );
            acc            = b.imul( acc, x );
            acc            = b.idiv( acc, y );
            acc            = b.udiv( acc, y );
            acc            = b.srem( acc, y );
            acc            = b.urem( acc, y );
            acc            = b.band( acc, x );
            acc            = b.bor( acc, x );
            acc            = b.bxor( acc, x );
            acc            = b.ishl( acc, c8 );
            acc            = b.ushr( acc, c8 );
            acc            = b.sshr( acc, y );
            acc            = b.bnot( acc );
            acc            = b.ineg( acc );
            const auto n32 = b.iadd( w, b.iconst( Type::i32, 1 ) );
            const auto e64 = b.uextend( Type::i64, n32 );
            const auto s64 = b.sextend( Type::i64, c8 );
            const auto r16 = b.ireduce( Type::i16, e64 );
            const auto l   = b.load( Type::i32, p, 4 );
            b.store( r16, p, -2 );
            b.store( acc, p );
            const auto q   = b.iadd( p, x );
            const auto cmp = b.icmp( ir::ICmpCC::slt, acc, s64 );
            const auto sel = b.select( cmp, acc, x );
            const auto r   = b.call( callee, Args { sel, q } );
            const auto fa  = b.func_addr( Type::ptr, callee );
            const auto ri  = b.call_indirect( sig, fa, Args { r, b.symbol_value( Type::ptr, gv ) } );
            b.brif( cmp, then, Args { ri }, other, Args { x, l } );

            b.switch_to_block( then );
            b.br( join, Args { b.block_param( then, 0 ) } );

            b.switch_to_block( other );
            b.br( join, Args { b.iadd( b.block_param( other, 0 ), b.sextend( Type::i64, b.block_param( other, 1 ) ) ) } );

            b.switch_to_block( join );
            b.ret( b.block_param( join, 0 ) );
        } );
    const auto errs = f.errors( );
    for ( const auto& e : errs ) UNSCOPED_INFO( e );
    CHECK( errs.empty( ) );
}

TEST_CASE( "verifier accepts a function with no blocks", "[verifier]" )
{
    Fn f {
        Type::i64,
        { Type::i64, Type::i32 }
    };
    f.build( []( ir::IRBuilder& ) { } );
    CHECK( f.errors( ).empty( ) );
}

TEST_CASE( "verifier starts ok and reports ok after a clean function", "[verifier]" )
{
    ir::IRVerifier v;
    CHECK( v.ok( ) );
    Fn f { Type::unit, { } };
    f.build(
        []( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { } ) );
            b.ret( );
        } );
    v.visit_function( *f.fn );
    CHECK( v.ok( ) );
}

TEST_CASE( "verifier rejects an entry block with the wrong param count", "[verifier]" )
{
    Fn f {
        Type::i64,
        { Type::i64, Type::i64 }
    };
    f.build(
        []( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::i64 } ) );
            b.ret( b.param( 0 ) );
        } );
    CHECK( f.errors( ) == Errs { "entry block has 1 params, signature has 2" } );
}

TEST_CASE( "verifier rejects an entry block with params when the signature has none", "[verifier]" )
{
    Fn f { Type::unit, { } };
    f.build(
        []( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::i64, Type::ptr } ) );
            b.ret( );
        } );
    CHECK( f.errors( ) == Errs { "entry block has 2 params, signature has 0" } );
}

TEST_CASE( "verifier rejects entry block param types that differ from the signature", "[verifier]" )
{
    Fn f {
        Type::unit,
        { Type::i64, Type::i32, Type::ptr }
    };
    f.build(
        []( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::i64, Type::i64, Type::i8 } ) );
            b.ret( );
        } );
    CHECK( f.errors( ) == Errs { "entry block param 1 is i64, signature says i32", "entry block param 2 is i8, signature says ptr" } );
}

TEST_CASE( "verifier rejects an empty block", "[verifier]" )
{
    Fn f { Type::unit, { } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e = b.add_block( { } );
            ( void ) b.add_block( { } );
            b.switch_to_block( e );
            b.ret( );
        } );
    CHECK( f.errors( ) == Errs { "block is empty, no terminator." } );
}

TEST_CASE( "verifier rejects a block ending in a non terminator", "[verifier]" )
{
    SECTION( "iconst" )
    {
        Fn f { Type::i64, { } };
        f.build(
            []( ir::IRBuilder& b )
            {
                b.switch_to_block( b.add_block( { } ) );
                b.ret( b.iconst( Type::i64, 7 ) );
            } );
        f.fn->blocks[ 0 ].insts_count = 1;
        CHECK( f.errors( ) == Errs { "block does not end with a terminator: v0 = iconst i64 #7" } );
    }

    SECTION( "iadd" )
    {
        Fn f {
            Type::i64,
            { Type::i64, Type::i64 }
        };
        f.build(
            []( ir::IRBuilder& b )
            {
                b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
                b.ret( b.iadd( b.param( 0 ), b.param( 1 ) ) );
            } );
        f.fn->blocks[ 0 ].insts_count = 1;
        CHECK( f.errors( ) == Errs { "block does not end with a terminator: v2 = iadd [ops 0..2]" } );
    }

    SECTION( "icmp" )
    {
        Fn f {
            Type::i8,
            { Type::i64, Type::i64 }
        };
        f.build(
            []( ir::IRBuilder& b )
            {
                b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
                b.ret( b.icmp( ir::ICmpCC::ult, b.param( 0 ), b.param( 1 ) ) );
            } );
        f.fn->blocks[ 0 ].insts_count = 1;
        CHECK( f.errors( ) == Errs { "block does not end with a terminator: v2 = icmp ult v0 v1" } );
    }
}

TEST_CASE( "verifier rejects a call to an unknown import", "[verifier]" )
{
    Fn       f { Type::i64, { Type::i64 } };
    ValueRef r { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            const auto callee = b.import_function( { ir::extns::function, 0 }, { Type::i64 }, Type::i64, cc, true );
            b.switch_to_block( b.add_block( { Type::i64 } ) );
            r = b.call( callee, Args { b.param( 0 ) } );
            b.ret( r );
        } );
    f.inst( r ).imm = 5;
    CHECK( f.errors( ) == Errs { "call to fn5 but only 1 imports" } );
}

TEST_CASE( "verifier rejects a call with the wrong argument count", "[verifier]" )
{
    const auto errs = errors_of( Type::i64, { Type::i64 },
                                 []( ir::IRBuilder& b )
                                 {
                                     const auto f
                                         = b.import_function( { ir::extns::function, 0 }, { Type::i64, Type::i64 }, Type::i64, cc, true );
                                     b.ret( b.call( f, Args { b.param( 0 ) } ) );
                                 } );
    CHECK( errs == Errs { "call fn0: 1 args, expected 2" } );
}

TEST_CASE( "verifier rejects a call with too many arguments", "[verifier]" )
{
    const auto errs
        = errors_of( Type::unit, { Type::i64 },
                     []( ir::IRBuilder& b )
                     {
                         const auto f = b.import_function( { ir::extns::function, 0 }, std::span< const Type > { }, Type::unit, cc, true );
                         ( void ) b.call( f, Args { b.param( 0 ), b.param( 0 ) } );
                         b.ret( );
                     } );
    CHECK( errs == Errs { "call fn0: 2 args, expected 0" } );
}

TEST_CASE( "verifier rejects call arguments of the wrong type", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { Type::i64, Type::i32 },
                                 []( ir::IRBuilder& b )
                                 {
                                     const auto f
                                         = b.import_function( { ir::extns::function, 0 }, { Type::i32, Type::ptr }, Type::unit, cc, true );
                                     ( void ) b.call( f, Args { b.param( 0 ), b.param( 1 ) } );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "call fn0: arg 0 is i64, expected i32", "call fn0: arg 1 is i32, expected ptr" } );
}

TEST_CASE( "verifier rejects a load with the wrong operand count", "[verifier]" )
{
    Fn       f { Type::i64, { Type::ptr } };
    ValueRef l { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::ptr } ) );
            l = b.load( Type::i64, b.param( 0 ) );
            b.ret( l );
        } );
    f.inst( l ).ops_count = 2;
    CHECK( f.errors( ) == Errs { "load: 2 operands, expected 1" } );
}

TEST_CASE( "verifier rejects a load through a non pointer", "[verifier]" )
{
    const Type addr = GENERATE( Type::i64, Type::i32, Type::i8 );
    const auto errs = errors_of( Type::i64, { addr },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.ret( b.load( Type::i64, b.param( 0 ) ) );
                                 } );
    CHECK( errs == Errs { std::format( "load: address is {}, expected ptr", addr ) } );
}

TEST_CASE( "verifier rejects a load of a non integer type", "[verifier]" )
{
    const Type ty   = GENERATE( Type::f32, Type::f64, Type::unit );
    const auto errs = errors_of( Type::unit, { Type::ptr },
                                 [ & ]( ir::IRBuilder& b )
                                 {
                                     ( void ) b.load( ty, b.param( 0 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { std::format( "load.{}: only integer loads supported YET", ty ) } );
}

TEST_CASE( "verifier accepts loads of every integer type", "[verifier]" )
{
    const Type ty   = GENERATE( Type::i8, Type::i16, Type::i32, Type::i64, Type::ptr );
    const auto errs = errors_of( Type::unit, { Type::ptr },
                                 [ & ]( ir::IRBuilder& b )
                                 {
                                     b.store( b.load( ty, b.param( 0 ), -16 ), b.param( 0 ), 16 );
                                     b.ret( );
                                 } );
    CHECK( errs.empty( ) );
}

TEST_CASE( "verifier rejects a store with the wrong operand count", "[verifier]" )
{
    Fn f {
        Type::unit,
        { Type::i64, Type::ptr }
    };
    f.build(
        []( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::i64, Type::ptr } ) );
            b.store( b.param( 0 ), b.param( 1 ) );
            b.ret( );
        } );
    f.fn->insts[ 0 ].ops_count = 1;
    CHECK( f.errors( ) == Errs { "store: 1 operands, expected 2" } );
}

TEST_CASE( "verifier rejects a store through a non pointer", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { Type::i64, Type::i64 },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.store( b.param( 0 ), b.param( 1 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "store: address is i64, expected ptr" } );
}

TEST_CASE( "verifier rejects a store of a non integer value", "[verifier]" )
{
    const Type ty   = GENERATE( Type::f32, Type::f64 );
    const auto errs = errors_of( Type::unit, { ty, Type::ptr },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.store( b.param( 0 ), b.param( 1 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { std::format( "store.{}: only integer stores supported YET", ty ) } );
}

TEST_CASE( "verifier rejects a store with both a bad address and a bad value", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { Type::f64, Type::i32 },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.store( b.param( 0 ), b.param( 1 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "store: address is i32, expected ptr", "store.f64: only integer stores supported YET" } );
}

TEST_CASE( "verifier rejects iconst of a non integer type", "[verifier]" )
{
    const Type ty   = GENERATE( Type::f32, Type::f64, Type::unit );
    const auto errs = errors_of( Type::unit, { },
                                 [ & ]( ir::IRBuilder& b )
                                 {
                                     ( void ) b.iconst( ty, 0 );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { std::format( "iconst: type {} is not an integer type", ty ) } );
}

TEST_CASE( "verifier accepts iconst of every integer type", "[verifier]" )
{
    const Type ty   = GENERATE( Type::i8, Type::i16, Type::i32, Type::i64, Type::ptr );
    const auto errs = errors_of( Type::unit, { },
                                 [ & ]( ir::IRBuilder& b )
                                 {
                                     ( void ) b.iconst( ty, ~uint64_t { 0 } );
                                     b.ret( );
                                 } );
    CHECK( errs.empty( ) );
}

TEST_CASE( "verifier rejects binary ops with the wrong operand count", "[verifier]" )
{
    Fn f {
        Type::i64,
        { Type::i64, Type::i64 }
    };
    ValueRef r { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
            r = b.iadd( b.param( 0 ), b.param( 1 ) );
            b.ret( r );
        } );
    f.inst( r ).ops_count = 1;
    CHECK( f.errors( ) == Errs { "iadd: 1 operands, expected 2" } );
    f.inst( r ).ops_count = 3;
    CHECK( f.errors( ) == Errs { "iadd: 3 operands, expected 2" } );
}

TEST_CASE( "verifier rejects binary ops on non integers", "[verifier]" )
{
    using Bin                   = ValueRef ( ir::IRBuilder::* )( ValueRef, ValueRef );
    const auto [ method, name ] = GENERATE( values< std::pair< Bin, std::string > >( {
        {&ir::IRBuilder::iadd, "iadd"},
        {&ir::IRBuilder::isub, "isub"},
        {&ir::IRBuilder::imul, "imul"},
        {&ir::IRBuilder::band, "band"},
        { &ir::IRBuilder::bor,  "bor"},
        {&ir::IRBuilder::bxor, "bxor"},
        {&ir::IRBuilder::idiv, "idiv"},
        {&ir::IRBuilder::udiv, "udiv"},
        {&ir::IRBuilder::srem, "srem"},
        {&ir::IRBuilder::urem, "urem"},
    } ) );
    const auto errs             = errors_of( Type::unit, { Type::f64, Type::i64 },
                                             [ & ]( ir::IRBuilder& b )
                                             {
                                     ( void ) ( b.*method )( b.param( 0 ), b.param( 1 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { name + ": operands are f64 and i64, expected integers" } );
}

TEST_CASE( "verifier rejects mismatched binary operand types", "[verifier]" )
{
    using Bin                   = ValueRef ( ir::IRBuilder::* )( ValueRef, ValueRef );
    const auto [ method, name ] = GENERATE( values< std::pair< Bin, std::string > >( {
        {&ir::IRBuilder::iadd, "iadd"},
        {&ir::IRBuilder::isub, "isub"},
        {&ir::IRBuilder::imul, "imul"},
        {&ir::IRBuilder::band, "band"},
        { &ir::IRBuilder::bor,  "bor"},
        {&ir::IRBuilder::bxor, "bxor"},
    } ) );
    const auto [ a, c ]         = GENERATE( values< std::pair< Type, Type > >( {
        {Type::i32, Type::i64},
        { Type::i8, Type::i16},
        {Type::i64, Type::ptr},
        {Type::i64, Type::i32},
    } ) );
    const auto errs             = errors_of( Type::unit, { a, c },
                                             [ & ]( ir::IRBuilder& b )
                                             {
                                     ( void ) ( b.*method )( b.param( 0 ), b.param( 1 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { std::format( "{}: operand types {} and {} don't match", name, a, c ) } );
}

TEST_CASE( "verifier accepts pointer plus i64 but not the reverse", "[verifier]" )
{
    const auto ok = errors_of( Type::ptr, { Type::ptr, Type::i64 },
                               []( ir::IRBuilder& b )
                               {
                                   b.ret( b.iadd( b.param( 0 ), b.param( 1 ) ) );
                               } );
    CHECK( ok.empty( ) );

    const auto both = errors_of( Type::ptr, { Type::ptr, Type::ptr },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.ret( b.isub( b.param( 0 ), b.param( 1 ) ) );
                                 } );
    CHECK( both.empty( ) );

    const auto bad = errors_of( Type::i64, { Type::i64, Type::ptr },
                                []( ir::IRBuilder& b )
                                {
                                    b.ret( b.iadd( b.param( 0 ), b.param( 1 ) ) );
                                } );
    CHECK( bad == Errs { "iadd: operand types i64 and ptr don't match" } );
}

TEST_CASE( "verifier accepts narrow arithmetic and bitwise ops", "[verifier]" )
{
    const Type ty   = GENERATE( Type::i8, Type::i16, Type::i32, Type::i64 );
    const auto errs = errors_of( ty, { ty, ty },
                                 []( ir::IRBuilder& b )
                                 {
                                     const auto x = b.param( 0 );
                                     const auto y = b.param( 1 );
                                     b.ret( b.bxor( b.bor( b.band( b.imul( b.isub( b.iadd( x, y ), y ), x ), y ), x ), y ) );
                                 } );
    CHECK( errs.empty( ) );
}

TEST_CASE( "verifier rejects narrow division, remainder and comparison", "[verifier]" )
{
    using Bin                   = ValueRef ( ir::IRBuilder::* )( ValueRef, ValueRef );
    const auto [ method, name ] = GENERATE( values< std::pair< Bin, std::string > >( {
        {&ir::IRBuilder::idiv, "idiv"},
        {&ir::IRBuilder::udiv, "udiv"},
        {&ir::IRBuilder::srem, "srem"},
        {&ir::IRBuilder::urem, "urem"},
    } ) );
    const Type ty               = GENERATE( Type::i8, Type::i16, Type::i32 );
    const auto errs             = errors_of( Type::unit, { ty, ty },
                                             [ & ]( ir::IRBuilder& b )
                                             {
                                     ( void ) ( b.*method )( b.param( 0 ), b.param( 1 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { std::format( "{}: operands are {} and {}, extend narrow values to i64 first", name, ty, ty ) } );
}

TEST_CASE( "verifier rejects narrow icmp operands", "[verifier]" )
{
    const Type ty   = GENERATE( Type::i8, Type::i16, Type::i32 );
    const auto errs = errors_of( Type::i8, { ty, ty },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.ret( b.icmp( ir::ICmpCC::eq, b.param( 0 ), b.param( 1 ) ) );
                                 } );
    CHECK( errs == Errs { std::format( "icmp: operands are {} and {}, extend narrow values to i64 first", ty, ty ) } );
}

TEST_CASE( "verifier reports both mismatch and narrowness for a mixed division", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { Type::i32, Type::i64 },
                                 []( ir::IRBuilder& b )
                                 {
                                     ( void ) b.idiv( b.param( 0 ), b.param( 1 ) );
                                     b.ret( );
                                 } );
    CHECK( errs
           == Errs { "idiv: operand types i32 and i64 don't match", "idiv: operands are i32 and i64, extend narrow values to i64 first" } );
}

TEST_CASE( "verifier accepts 64 bit division, comparison and pointer comparison", "[verifier]" )
{
    const auto errs = errors_of( Type::i8, { Type::i64, Type::i64, Type::ptr },
                                 []( ir::IRBuilder& b )
                                 {
                                     const auto x = b.param( 0 );
                                     const auto y = b.param( 1 );
                                     const auto p = b.param( 2 );
                                     ( void ) b.urem( b.srem( b.udiv( b.idiv( x, y ), y ), y ), y );
                                     ( void ) b.icmp( ir::ICmpCC::eq, p, p );
                                     ( void ) b.icmp( ir::ICmpCC::ult, p, x );
                                     ( void ) b.udiv( p, x );
                                     b.ret( b.icmp( ir::ICmpCC::sge, x, y ) );
                                 } );
    CHECK( errs.empty( ) );
}

TEST_CASE( "verifier rejects shifts with the wrong operand count", "[verifier]" )
{
    Fn f {
        Type::i64,
        { Type::i64, Type::i64 }
    };
    ValueRef r { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
            r = b.ishl( b.param( 0 ), b.param( 1 ) );
            b.ret( r );
        } );
    f.inst( r ).ops_count = 1;
    CHECK( f.errors( ) == Errs { "ishl: 1 operands, expected 2" } );
}

TEST_CASE( "verifier rejects shifts on non integers", "[verifier]" )
{
    const auto value = errors_of( Type::unit, { Type::f64, Type::i64 },
                                  []( ir::IRBuilder& b )
                                  {
                                      ( void ) b.ishl( b.param( 0 ), b.param( 1 ) );
                                      b.ret( );
                                  } );
    CHECK( value == Errs { "ishl: operands are f64 and i64, expected integers" } );

    const auto count = errors_of( Type::unit, { Type::i64, Type::f32 },
                                  []( ir::IRBuilder& b )
                                  {
                                      ( void ) b.ushr( b.param( 0 ), b.param( 1 ) );
                                      b.ret( );
                                  } );
    CHECK( count == Errs { "ushr: operands are i64 and f32, expected integers" } );
}

TEST_CASE( "verifier rejects narrow right shifts", "[verifier]" )
{
    const Type ty   = GENERATE( Type::i8, Type::i16, Type::i32 );
    const auto errs = errors_of( Type::unit, { ty, Type::i64 },
                                 []( ir::IRBuilder& b )
                                 {
                                     ( void ) b.ushr( b.param( 0 ), b.param( 1 ) );
                                     ( void ) b.sshr( b.param( 0 ), b.param( 1 ) );
                                     b.ret( );
                                 } );
    CHECK( errs
           == Errs { std::format( "ushr: value is {}, extend narrow values to i64 first", ty ),
                     std::format( "sshr: value is {}, extend narrow values to i64 first", ty ) } );
}

TEST_CASE( "verifier accepts narrow left shifts and any integer shift count", "[verifier]" )
{
    const Type value = GENERATE( Type::i8, Type::i16, Type::i32, Type::i64 );
    const Type count = GENERATE( Type::i8, Type::i16, Type::i32, Type::i64 );
    const auto errs  = errors_of( Type::unit, { value, count, Type::i64, Type::ptr },
                                  []( ir::IRBuilder& b )
                                  {
                                     ( void ) b.ishl( b.param( 0 ), b.param( 1 ) );
                                     ( void ) b.ushr( b.param( 2 ), b.param( 1 ) );
                                     ( void ) b.sshr( b.param( 3 ), b.param( 1 ) );
                                     b.ret( );
                                 } );
    CHECK( errs.empty( ) );
}

TEST_CASE( "verifier rejects unary ops on non integers", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { Type::f64 },
                                 []( ir::IRBuilder& b )
                                 {
                                     ( void ) b.bnot( b.param( 0 ) );
                                     ( void ) b.ineg( b.param( 0 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "bnot: operand is f64, expected an integer", "ineg: operand is f64, expected an integer" } );
}

TEST_CASE( "verifier rejects unary ops with the wrong operand count", "[verifier]" )
{
    Fn       f { Type::i64, { Type::i64 } };
    ValueRef r { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::i64 } ) );
            r = b.ineg( b.param( 0 ) );
            b.ret( r );
        } );
    f.inst( r ).ops_count = 2;
    CHECK( f.errors( ) == Errs { "ineg: 2 operands, expected 1" } );
    f.inst( r ).op = ir::IROp::bnot;
    CHECK( f.errors( ) == Errs { "bnot: 2 operands, expected 1" } );
}

TEST_CASE( "verifier accepts unary ops on every integer type", "[verifier]" )
{
    const Type ty   = GENERATE( Type::i8, Type::i16, Type::i32, Type::i64, Type::ptr );
    const auto errs = errors_of( ty, { ty },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.ret( b.ineg( b.bnot( b.param( 0 ) ) ) );
                                 } );
    CHECK( errs.empty( ) );
}

TEST_CASE( "verifier rejects conversions involving non integers", "[verifier]" )
{
    const auto to_float = errors_of( Type::unit, { Type::i32 },
                                     []( ir::IRBuilder& b )
                                     {
                                         ( void ) b.uextend( Type::f64, b.param( 0 ) );
                                         b.ret( );
                                     } );
    CHECK( to_float == Errs { "uextend: i32 to f64, expected integers" } );

    const auto from_float = errors_of( Type::unit, { Type::f64 },
                                       []( ir::IRBuilder& b )
                                       {
                                           ( void ) b.ireduce( Type::i32, b.param( 0 ) );
                                           b.ret( );
                                       } );
    CHECK( from_float == Errs { "ireduce: f64 to i32, expected integers" } );

    const auto to_unit = errors_of( Type::unit, { Type::i8 },
                                    []( ir::IRBuilder& b )
                                    {
                                        ( void ) b.sextend( Type::unit, b.param( 0 ) );
                                        b.ret( );
                                    } );
    CHECK( to_unit == Errs { "sextend: i8 to unit, expected integers" } );
}

TEST_CASE( "verifier rejects extensions that do not widen", "[verifier]" )
{
    const auto [ to, from ] = GENERATE( values< std::pair< Type, Type > >( {
        {Type::i32, Type::i64},
        {Type::i64, Type::i64},
        { Type::i8,  Type::i8},
        {Type::ptr, Type::i64},
        {Type::i64, Type::ptr},
        {Type::i16, Type::i32},
    } ) );
    const auto errs         = errors_of( Type::unit, { from },
                                         [ & ]( ir::IRBuilder& b )
                                         {
                                     ( void ) b.uextend( to, b.param( 0 ) );
                                     ( void ) b.sextend( to, b.param( 0 ) );
                                     b.ret( );
                                 } );
    CHECK( errs
           == Errs { std::format( "uextend.{}: from {} is not narrower", to, from ),
                     std::format( "sextend.{}: from {} is not narrower", to, from ) } );
}

TEST_CASE( "verifier rejects reductions that do not narrow", "[verifier]" )
{
    const auto [ to, from ] = GENERATE( values< std::pair< Type, Type > >( {
        {Type::i64, Type::i32},
        {Type::i32, Type::i32},
        {Type::ptr, Type::i64},
        {Type::i16,  Type::i8},
    } ) );
    const auto errs         = errors_of( Type::unit, { from },
                                         [ & ]( ir::IRBuilder& b )
                                         {
                                     ( void ) b.ireduce( to, b.param( 0 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { std::format( "ireduce.{}: from {} is not wider", to, from ) } );
}

TEST_CASE( "verifier accepts every widening and narrowing pair", "[verifier]" )
{
    const auto [ narrow, wide ] = GENERATE( values< std::pair< Type, Type > >( {
        { Type::i8, Type::i16},
        { Type::i8, Type::i32},
        { Type::i8, Type::i64},
        { Type::i8, Type::ptr},
        {Type::i16, Type::i32},
        {Type::i16, Type::i64},
        {Type::i32, Type::i64},
        {Type::i32, Type::ptr},
    } ) );
    const auto errs             = errors_of( Type::unit, { narrow, wide },
                                             [ & ]( ir::IRBuilder& b )
                                             {
                                     ( void ) b.uextend( wide, b.param( 0 ) );
                                     ( void ) b.sextend( wide, b.param( 0 ) );
                                     ( void ) b.ireduce( narrow, b.param( 1 ) );
                                     b.ret( );
                                 } );
    CHECK( errs.empty( ) );
}

TEST_CASE( "verifier rejects conversions with the wrong operand count", "[verifier]" )
{
    Fn       f { Type::i64, { Type::i32 } };
    ValueRef r { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::i32 } ) );
            r = b.sextend( Type::i64, b.param( 0 ) );
            b.ret( r );
        } );
    f.inst( r ).ops_count = 2;
    CHECK( f.errors( ) == Errs { "sextend: 2 operands, expected 1" } );
}

TEST_CASE( "verifier rejects select with the wrong operand count", "[verifier]" )
{
    Fn f {
        Type::i64,
        { Type::i8, Type::i64, Type::i64 }
    };
    ValueRef r { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::i8, Type::i64, Type::i64 } ) );
            r = b.select( b.param( 0 ), b.param( 1 ), b.param( 2 ) );
            b.ret( r );
        } );
    f.inst( r ).ops_count = 2;
    CHECK( f.errors( ) == Errs { "select: 2 operands, expected 3" } );
}

TEST_CASE( "verifier rejects select with a non integer condition", "[verifier]" )
{
    const auto errs = errors_of( Type::i64, { Type::f64, Type::i64 },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.ret( b.select( b.param( 0 ), b.param( 1 ), b.param( 1 ) ) );
                                 } );
    CHECK( errs == Errs { "select: condition is f64, expected an integer" } );
}

TEST_CASE( "verifier rejects select between different types", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { Type::i8, Type::i64, Type::i32 },
                                 []( ir::IRBuilder& b )
                                 {
                                     ( void ) b.select( b.param( 0 ), b.param( 1 ), b.param( 2 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "select: values are i64 and i32, expected the same type" } );
}

TEST_CASE( "verifier reports both select errors together", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { Type::f32, Type::ptr, Type::i64 },
                                 []( ir::IRBuilder& b )
                                 {
                                     ( void ) b.select( b.param( 0 ), b.param( 1 ), b.param( 2 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "select: condition is f32, expected an integer", "select: values are ptr and i64, expected the same type" } );
}

TEST_CASE( "verifier accepts select with any integer condition", "[verifier]" )
{
    const Type cond = GENERATE( Type::i8, Type::i16, Type::i32, Type::i64, Type::ptr );
    const auto errs = errors_of( Type::ptr, { cond, Type::ptr, Type::ptr },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.ret( b.select( b.param( 0 ), b.param( 1 ), b.param( 2 ) ) );
                                 } );
    CHECK( errs.empty( ) );
}

TEST_CASE( "verifier rejects func_addr of an unknown import", "[verifier]" )
{
    const auto errs = errors_of( Type::ptr, { },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.ret( b.func_addr( Type::ptr, ir::FuncRef { 3 } ) );
                                 } );
    CHECK( errs == Errs { "func_addr: fn3 but only 0 imports" } );
}

TEST_CASE( "verifier rejects func_addr with a non pointer result", "[verifier]" )
{
    const Type ty = GENERATE( Type::i64, Type::i32, Type::f64 );
    const auto errs
        = errors_of( Type::unit, { },
                     [ & ]( ir::IRBuilder& b )
                     {
                         const auto f = b.import_function( { ir::extns::function, 0 }, std::span< const Type > { }, Type::unit, cc, true );
                         ( void ) b.func_addr( ty, f );
                         b.ret( );
                     } );
    CHECK( errs == Errs { std::format( "func_addr.{}: result must be ptr", ty ) } );
}

TEST_CASE( "verifier reports both func_addr errors together", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { },
                                 []( ir::IRBuilder& b )
                                 {
                                     ( void ) b.func_addr( Type::i64, ir::FuncRef { 1 } );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "func_addr: fn1 but only 0 imports", "func_addr.i64: result must be ptr" } );
}

TEST_CASE( "verifier rejects func_addr with operands", "[verifier]" )
{
    Fn       f { Type::ptr, { } };
    ValueRef r { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            const auto callee = b.import_function( { ir::extns::function, 0 }, std::span< const Type > { }, Type::unit, cc, true );
            b.switch_to_block( b.add_block( { } ) );
            r = b.func_addr( Type::ptr, callee );
            b.ret( r );
        } );
    f.inst( r ).ops_count = 1;
    CHECK( f.errors( ) == Errs { "func_addr: 1 operands, expected 0" } );
}

TEST_CASE( "verifier rejects call_indirect through an unknown signature", "[verifier]" )
{
    Fn f { Type::unit, { Type::ptr } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto s = b.import_signature( std::span< const Type > { }, Type::unit, cc );
            b.switch_to_block( b.add_block( { Type::ptr } ) );
            b.call_indirect( s, b.param( 0 ), { } );
            b.ret( );
        } );
    f.fn->insts[ 0 ].imm = 4;
    CHECK( f.errors( ) == Errs { "call_indirect through sig4 but only 1 signatures" } );
}

TEST_CASE( "verifier rejects call_indirect with the wrong argument count", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { Type::ptr, Type::i64 },
                                 []( ir::IRBuilder& b )
                                 {
                                     const auto s = b.import_signature( { Type::i64, Type::i64 }, Type::unit, cc );
                                     b.call_indirect( s, b.param( 0 ), Args { b.param( 1 ) } );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "call_indirect sig0: 1 args, expected 2" } );
}

TEST_CASE( "verifier rejects call_indirect through a non pointer callee", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { Type::i64 },
                                 []( ir::IRBuilder& b )
                                 {
                                     const auto s = b.import_signature( std::span< const Type > { }, Type::unit, cc );
                                     b.call_indirect( s, b.param( 0 ), { } );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "call_indirect: callee is i64, expected ptr" } );
}

TEST_CASE( "verifier rejects call_indirect arguments of the wrong type", "[verifier]" )
{
    const auto errs = errors_of( Type::i64, { Type::ptr, Type::i64, Type::i32 },
                                 []( ir::IRBuilder& b )
                                 {
                                     const auto s = b.import_signature( { Type::ptr, Type::i64 }, Type::i64, cc );
                                     b.ret( b.call_indirect( s, b.param( 0 ), Args { b.param( 1 ), b.param( 2 ) } ) );
                                 } );
    CHECK( errs == Errs { "call_indirect sig0: arg 0 is i64, expected ptr", "call_indirect sig0: arg 1 is i32, expected i64" } );
}

TEST_CASE( "verifier rejects symbol_value of an unknown global", "[verifier]" )
{
    const auto errs = errors_of( Type::ptr, { },
                                 []( ir::IRBuilder& b )
                                 {
                                     b.ret( b.symbol_value( Type::ptr, ir::GlobalValueRef { 2 } ) );
                                 } );
    CHECK( errs == Errs { "symbol_value: gv2 but only 0 global values" } );
}

TEST_CASE( "verifier rejects symbol_value with a non pointer result", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { },
                                 []( ir::IRBuilder& b )
                                 {
                                     const auto g = b.create_global_value( {
                                         .name = { ir::extns::data, 0 }
                                     } );
                                     ( void ) b.symbol_value( Type::i64, g );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "symbol_value.i64: result must be ptr" } );
}

TEST_CASE( "verifier rejects symbol_value with operands", "[verifier]" )
{
    Fn       f { Type::ptr, { } };
    ValueRef r { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            const auto g = b.create_global_value( {
                .name = { ir::extns::data, 0 }
            } );
            b.switch_to_block( b.add_block( { } ) );
            r = b.symbol_value( Type::ptr, g );
            b.ret( r );
        } );
    f.inst( r ).ops_count = 1;
    CHECK( f.errors( ) == Errs { "symbol_value: 1 operands, expected 0" } );
}

TEST_CASE( "verifier accepts stack_addr anywhere in the slot up to one past the end", "[verifier]" )
{
    const int32_t offset = GENERATE( 0, 4, 15, 16 );
    const auto    errs   = errors_of( Type::ptr, { },
                                      [ & ]( ir::IRBuilder& b )
                                      {
                                     const auto ss = b.create_stack_slot( 16, 8 );
                                     b.ret( b.stack_addr( Type::ptr, ss, offset ) );
                                 } );
    CHECK( errs.empty( ) );
}

TEST_CASE( "verifier rejects stack_addr of an unknown slot", "[verifier]" )
{
    const auto errs = errors_of( Type::ptr, { },
                                 []( ir::IRBuilder& b )
                                 {
                                     ( void ) b.create_stack_slot( 8, 8 );
                                     b.ret( b.stack_addr( Type::ptr, ir::StackSlotRef { 2 } ) );
                                 } );
    CHECK( errs == Errs { "stack_addr: ss2 but only 1 stack slots" } );
}

TEST_CASE( "verifier rejects stack_addr with a non pointer result", "[verifier]" )
{
    const Type ty   = GENERATE( Type::i64, Type::i32, Type::i8 );
    const auto errs = errors_of( Type::unit, { },
                                 [ & ]( ir::IRBuilder& b )
                                 {
                                     ( void ) b.stack_addr( ty, b.create_stack_slot( 8, 8 ) );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { std::format( "stack_addr.{}: result must be ptr", ty ) } );
}

TEST_CASE( "verifier reports both stack_addr errors together", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { },
                                 []( ir::IRBuilder& b )
                                 {
                                     ( void ) b.stack_addr( Type::i64, ir::StackSlotRef { 0 } );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "stack_addr: ss0 but only 0 stack slots", "stack_addr.i64: result must be ptr" } );
}

TEST_CASE( "verifier rejects stack_addr offsets outside the slot", "[verifier]" )
{
    const auto [ offset, text ] = GENERATE( std::pair { -1, "ss0-1" }, std::pair { 17, "ss0+17" },
                                            std::pair { std::numeric_limits< int32_t >::min( ), "ss0-2147483648" } );
    const auto errs             = errors_of( Type::ptr, { },
                                             [ & ]( ir::IRBuilder& b )
                                             {
                                     b.ret( b.stack_addr( Type::ptr, b.create_stack_slot( 16, 8 ), offset ) );
                                 } );
    CHECK( errs == Errs { std::format( "stack_addr: {} is outside the slot's 16 bytes", text ) } );
}

TEST_CASE( "verifier rejects stack_addr with operands", "[verifier]" )
{
    Fn       f { Type::ptr, { } };
    ValueRef r { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            const auto ss = b.create_stack_slot( 8, 8 );
            b.switch_to_block( b.add_block( { } ) );
            r = b.stack_addr( Type::ptr, ss );
            b.ret( r );
        } );
    f.inst( r ).ops_count = 1;
    CHECK( f.errors( ) == Errs { "stack_addr: 1 operands, expected 0" } );
}

TEST_CASE( "verifier rejects empty and over aligned stack slots", "[verifier]" )
{
    const auto errs = errors_of( Type::unit, { },
                                 []( ir::IRBuilder& b )
                                 {
                                     ( void ) b.create_stack_slot( 0, 8 );
                                     ( void ) b.create_stack_slot( 64, 16 );
                                     ( void ) b.create_stack_slot( 64, 32 );
                                     b.ret( );
                                 } );
    CHECK( errs == Errs { "ss0: size is 0", "ss2: align = 32 is over the maximum of 16" } );
}

TEST_CASE( "verifier checks stack slots of a function without blocks", "[verifier]" )
{
    Fn f { Type::unit, { } };
    f.build(
        []( ir::IRBuilder& b )
        {
            ( void ) b.create_stack_slot( 0, 1 );
        } );
    CHECK( f.errors( ) == Errs { "ss0: size is 0" } );
}

TEST_CASE( "verifier rejects a branch to the entry block", "[verifier]" )
{
    SECTION( "br" )
    {
        Fn f { Type::unit, { } };
        f.build(
            []( ir::IRBuilder& b )
            {
                const auto e = b.add_block( { } );
                const auto n = b.add_block( { } );
                b.switch_to_block( e );
                b.br( n );
                b.switch_to_block( n );
                b.br( e );
            } );
        CHECK( f.errors( ) == Errs { "br to the entry block b0" } );
    }

    SECTION( "brif" )
    {
        Fn f { Type::unit, { Type::i8 } };
        f.build(
            []( ir::IRBuilder& b )
            {
                const auto e = b.add_block( { Type::i8 } );
                const auto n = b.add_block( { } );
                b.switch_to_block( e );
                b.br( n );
                b.switch_to_block( n );
                const auto c = b.iconst( Type::i8, 0 );
                b.brif( c, e, Args { c }, n, Args { } );
            } );
        CHECK( f.errors( ) == Errs { "brif to the entry block b0" } );
    }
}

TEST_CASE( "verifier rejects br with the wrong argument count", "[verifier]" )
{
    Fn f { Type::unit, { Type::i64 } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e = b.add_block( { Type::i64 } );
            const auto t = b.add_block( { Type::i64 } );
            b.switch_to_block( e );
            b.br( t, Args { b.param( 0 ), b.param( 0 ) } );
            b.switch_to_block( t );
            b.ret( );
        } );
    CHECK( f.errors( ) == Errs { "br to b1: 2 args, block takes 1" } );
}

TEST_CASE( "verifier rejects br with missing arguments", "[verifier]" )
{
    Fn f { Type::unit, { } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e = b.add_block( { } );
            const auto t = b.add_block( { Type::i64, Type::i32 } );
            b.switch_to_block( e );
            b.br( t );
            b.switch_to_block( t );
            b.ret( );
        } );
    CHECK( f.errors( ) == Errs { "br to b1: 0 args, block takes 2" } );
}

TEST_CASE( "verifier rejects brif with the wrong argument count", "[verifier]" )
{
    Fn f { Type::unit, { Type::i8 } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e  = b.add_block( { Type::i8 } );
            const auto t  = b.add_block( { Type::i8 } );
            const auto el = b.add_block( { Type::i8, Type::i8 } );
            b.switch_to_block( e );
            b.brif( b.param( 0 ), t, Args { }, el, Args { b.param( 0 ) } );
            b.switch_to_block( t );
            b.ret( );
            b.switch_to_block( el );
            b.ret( );
        } );
    CHECK( f.errors( ) == Errs { "brif: 2 operands, expected 1 condition + 1 + 2 args" } );
}

TEST_CASE( "verifier rejects br arguments of the wrong type", "[verifier]" )
{
    Fn f {
        Type::unit,
        { Type::i64, Type::ptr }
    };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e = b.add_block( { Type::i64, Type::ptr } );
            const auto t = b.add_block( { Type::i32, Type::ptr, Type::i64 } );
            b.switch_to_block( e );
            b.br( t, Args { b.param( 0 ), b.param( 1 ), b.param( 1 ) } );
            b.switch_to_block( t );
            b.ret( );
        } );
    CHECK( f.errors( ) == Errs { "br to b1: arg 0 is i64, block param is i32", "br to b1: arg 2 is ptr, block param is i64" } );
}

TEST_CASE( "verifier rejects brif arguments of the wrong type on either edge", "[verifier]" )
{
    Fn f {
        Type::unit,
        { Type::i8, Type::i64 }
    };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e  = b.add_block( { Type::i8, Type::i64 } );
            const auto t  = b.add_block( { Type::i32 } );
            const auto el = b.add_block( { Type::i64 } );
            b.switch_to_block( e );
            b.brif( b.param( 0 ), t, Args { b.param( 1 ) }, el, Args { b.param( 0 ) } );
            b.switch_to_block( t );
            b.ret( );
            b.switch_to_block( el );
            b.ret( );
        } );
    CHECK( f.errors( ) == Errs { "brif to b1: arg 0 is i64, block param is i32", "brif to b2: arg 0 is i8, block param is i64" } );
}

TEST_CASE( "verifier rejects a brif condition that is not an integer", "[verifier]" )
{
    Fn f { Type::i64, { Type::f64 } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e   = b.add_block( { Type::f64 } );
            const auto yes = b.add_block( { } );
            const auto no  = b.add_block( { } );
            b.switch_to_block( e );
            b.brif( b.param( 0 ), yes, Args { }, no, Args { } );
            b.switch_to_block( yes );
            b.ret( b.iconst( Type::i64, 1 ) );
            b.switch_to_block( no );
            b.ret( b.iconst( Type::i64, 2 ) );
        } );
    CHECK( f.errors( ) == Errs { "brif: condition is f64, expect an integer" } );
}

TEST_CASE( "verifier rejects a branch with the wrong successor count", "[verifier]" )
{
    SECTION( "br with two successors" )
    {
        Fn f { Type::unit, { } };
        f.build(
            []( ir::IRBuilder& b )
            {
                const auto e = b.add_block( { } );
                const auto t = b.add_block( { } );
                b.switch_to_block( e );
                b.br( t );
                b.switch_to_block( t );
                b.ret( );
            } );
        f.fn->blocks[ 0 ].succs_count = 2;
        CHECK( f.errors( ) == Errs { "br: 2 successors, expected 1" } );
    }

    SECTION( "brif with one successor" )
    {
        Fn f { Type::unit, { Type::i8 } };
        f.build(
            []( ir::IRBuilder& b )
            {
                const auto e = b.add_block( { Type::i8 } );
                const auto t = b.add_block( { } );
                b.switch_to_block( e );
                b.brif( b.param( 0 ), t, Args { }, t, Args { } );
                b.switch_to_block( t );
                b.ret( );
            } );
        f.fn->blocks[ 0 ].succs_count = 1;
        CHECK( f.errors( ) == Errs { "brif: 1 successors, expected 2" } );
    }
}

TEST_CASE( "verifier rejects a branch to a block that does not exist", "[verifier]" )
{
    Fn f { Type::unit, { } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e = b.add_block( { } );
            const auto t = b.add_block( { } );
            b.switch_to_block( e );
            b.br( t );
            b.switch_to_block( t );
            b.ret( );
        } );
    f.fn->succs[ 0 ].id = 9;
    CHECK( f.errors( ) == Errs { "br to b9, but the function has 2 blocks" } );
}

TEST_CASE( "verifier accepts brif to the same block on both edges", "[verifier]" )
{
    Fn f { Type::i64, { Type::i8 } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e = b.add_block( { Type::i8 } );
            const auto t = b.add_block( { Type::i64 } );
            b.switch_to_block( e );
            b.brif( b.param( 0 ), t, Args { b.iconst( Type::i64, 1 ) }, t, Args { b.iconst( Type::i64, 2 ) } );
            b.switch_to_block( t );
            b.ret( b.block_param( t, 0 ) );
        } );
    CHECK( f.errors( ).empty( ) );
}

TEST_CASE( "verifier accepts a self loop on a non entry block", "[verifier]" )
{
    Fn f { Type::i64, { Type::i64 } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e    = b.add_block( { Type::i64 } );
            const auto loop = b.add_block( { Type::i64 } );
            const auto exit = b.add_block( { } );
            b.switch_to_block( e );
            b.br( loop, Args { b.param( 0 ) } );
            b.switch_to_block( loop );
            const auto n = b.isub( b.block_param( loop, 0 ), b.iconst( Type::i64, 1 ) );
            b.brif( n, loop, Args { n }, exit, Args { } );
            b.switch_to_block( exit );
            b.ret( b.iconst( Type::i64, 0 ) );
        } );
    CHECK( f.errors( ).empty( ) );
}

TEST_CASE( "verifier collects errors from several blocks and instructions", "[verifier]" )
{
    Fn f {
        Type::unit,
        { Type::i32, Type::i64 }
    };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e = b.add_block( { Type::i32, Type::i64 } );
            const auto t = b.add_block( { Type::i32 } );
            ( void ) b.add_block( { } );
            b.switch_to_block( e );
            ( void ) b.iadd( b.param( 0 ), b.param( 1 ) );
            b.br( t, Args { b.param( 1 ) } );
            b.switch_to_block( t );
            ( void ) b.udiv( b.block_param( t, 0 ), b.block_param( t, 0 ) );
            b.ret( );
        } );
    CHECK( f.errors( )
           == Errs { "iadd: operand types i32 and i64 don't match", "br to b1: arg 0 is i64, block param is i32",
                     "udiv: operands are i32 and i32, extend narrow values to i64 first", "block is empty, no terminator." } );
}

TEST_CASE( "verifier visit_module accumulates errors across functions", "[verifier]" )
{
    ir::IRModule irm;
    const auto   good = irm.declare( "good", Type::i64, { Type::i64 } );
    const auto   bad  = irm.declare( "bad", Type::i64, { Type::i64 } );
    const auto   none = irm.declare( "none", Type::unit, { Type::ptr } );
    {
        ir::IRBuilder b { *irm.get( good ) };
        b.switch_to_block( b.add_block( { Type::i64 } ) );
        b.ret( b.param( 0 ) );
        b.finalise( );
    }
    {
        ir::IRBuilder b { *irm.get( bad ) };
        b.switch_to_block( b.add_block( { Type::i32 } ) );
        ( void ) b.iconst( Type::f64, 0 );
        b.ret( );
        b.finalise( );
    }
    {
        ir::IRBuilder b { *irm.get( none ) };
        b.finalise( );
    }

    ir::IRVerifier v;
    v.visit_module( irm );
    CHECK_FALSE( v.ok( ) );
    REQUIRE( v.errs.size( ) == 3 );
    CHECK( v.errs[ 0 ].msg == "entry block param 0 is i32, signature says i64" );
    CHECK( v.errs[ 1 ].msg == "iconst: type f64 is not an integer type" );
    CHECK( v.errs[ 2 ].msg == "ret: 0 operands, expected 1" );
}

TEST_CASE( "visitor hooks fire in order for every block, instruction and operand", "[visitor]" )
{
    Fn f { Type::i64, { Type::i64 } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto e = b.add_block( { Type::i64 } );
            const auto t = b.add_block( { Type::i64 } );
            b.switch_to_block( e );
            const auto k = b.iconst( Type::i64, 1 );
            b.br( t, Args { b.iadd( b.param( 0 ), k ) } );
            b.switch_to_block( t );
            b.ret( b.block_param( t, 0 ) );
        } );

    Recorder r;
    r.visit_function( *f.fn );
    CHECK( r.log
           == std::vector< std::string > { "fn 0", "block 0", "inst iconst", "inst iadd", "op v0", "op v2", "inst br", "op v3", "block_end",
                                           "block 3", "inst ret", "op v1", "block_end", "fn_end 0" } );
}

TEST_CASE( "visitor visits a function without blocks", "[visitor]" )
{
    Fn f { Type::unit, { } };
    f.build( []( ir::IRBuilder& ) { } );
    Recorder r;
    r.visit_function( *f.fn );
    CHECK( r.log == std::vector< std::string > { "fn 0", "fn_end 0" } );
}

TEST_CASE( "visitor visit_module walks functions in declaration order", "[visitor]" )
{
    ir::IRModule irm;
    for ( int i { 0 }; i < 3; ++i )
    {
        const auto    ref = irm.declare( "f" + std::to_string( i ), Type::unit, std::span< const Type > { } );
        ir::IRBuilder b { *irm.get( ref ) };
        b.switch_to_block( b.add_block( { } ) );
        b.ret( );
        b.finalise( );
    }
    Recorder r;
    r.visit_module( irm );
    CHECK( r.log
           == std::vector< std::string > { "fn 0", "block 0", "inst ret", "block_end", "fn_end 0", "fn 1", "block 0", "inst ret",
                                           "block_end", "fn_end 1", "fn 2", "block 0", "inst ret", "block_end", "fn_end 2" } );
}

TEST_CASE( "visitor only overriding on_inst sees every instruction", "[visitor]" )
{
    ir::IRModule irm;
    for ( int i { 0 }; i < 4; ++i )
    {
        const auto    ref = irm.declare( "g" + std::to_string( i ), Type::i64, { Type::i64 } );
        ir::IRBuilder b { *irm.get( ref ) };
        b.switch_to_block( b.add_block( { Type::i64 } ) );
        auto v = b.param( 0 );
        for ( int k { 0 }; k < i; ++k ) v = b.iadd( v, v );
        b.ret( v );
        b.finalise( );
    }
    InstCounter c;
    c.visit_module( irm );
    CHECK( c.insts == 1 + 2 + 3 + 4 );
}

TEST_CASE( "visitor can mutate instructions in place", "[visitor]" )
{
    Fn f { Type::i64, { } };
    f.build(
        []( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { } ) );
            b.ret( b.iadd( b.iconst( Type::i64, 1 ), b.iconst( Type::i64, 2 ) ) );
        } );
    ConstRewriter w;
    w.visit_function( *f.fn );
    CHECK( f.fn->insts[ 0 ].imm == 99 );
    CHECK( f.fn->insts[ 1 ].imm == 99 );
    CHECK( f.fn->insts[ 2 ].imm == 0 );
}

TEST_CASE( "visitor visits blocks in creation order regardless of fill order", "[visitor]" )
{
    Fn f { Type::unit, { } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto b0 = b.add_block( { } );
            const auto b1 = b.add_block( { } );
            const auto b2 = b.add_block( { } );
            b.switch_to_block( b0 );
            b.br( b2 );
            b.switch_to_block( b2 );
            b.br( b1 );
            b.switch_to_block( b1 );
            b.ret( );
        } );
    Recorder r;
    r.visit_function( *f.fn );
    CHECK( r.log
           == std::vector< std::string > { "fn 0", "block 0", "inst br", "block_end", "block 2", "inst ret", "block_end", "block 1",
                                           "inst br", "block_end", "fn_end 0" } );
}

TEST_CASE( "verifier rejects ret without a value in a function that returns one", "[verifier]" )
{
    CHECK( errors_of( Type::i64, { },
                      []( ir::IRBuilder& b )
                      {
                          b.ret( );
                      } )
           == Errs { "ret: 0 operands, expected 1" } );
}

TEST_CASE( "verifier rejects ret with a value in a unit function", "[verifier]" )
{
    CHECK( errors_of( Type::unit, { },
                      []( ir::IRBuilder& b )
                      {
                          b.ret( b.iconst( Type::i64, 1 ) );
                      } )
           == Errs { "ret: 1 operands, expected 0" } );
}

TEST_CASE( "verifier rejects ret whose value type differs from the signature", "[verifier]" )
{
    CHECK( errors_of( Type::i64, { },
                      []( ir::IRBuilder& b )
                      {
                          b.ret( b.iconst( Type::i8, 1 ) );
                      } )
           == Errs { "ret: value type is i8, the function's signature wants i64" } );
}

TEST_CASE( "verifier rejects a terminator before the end of a block", "[verifier]" )
{
    CHECK( errors_of( Type::i64, { Type::i64 },
                      []( ir::IRBuilder& b )
                      {
                          b.ret( b.param( 0 ) );
                          b.ret( b.param( 0 ) );
                      } )
           == Errs { "ret: terminator before the end of the block" } );
}

TEST_CASE( "verifier rejects an operand that is not a value of the function", "[verifier]" )
{
    Fn f { Type::i64, { Type::i64 } };
    f.build(
        []( ir::IRBuilder& b )
        {
            b.switch_to_block( b.add_block( { Type::i64 } ) );
            b.ret( b.iadd( b.param( 0 ), b.param( 0 ) ) );
        } );
    f.fn->operands[ f.fn->insts[ 0 ].ops_offset + 1 ] = ValueRef { 999 };
    CHECK( f.errors( ) == Errs { "iadd: operand 1 is v999, but the function has 3 values" } );
}

TEST_CASE( "verifier rejects a use that its definition does not dominate", "[verifier]" )
{
    Fn       f { Type::i64, { Type::i64 } };
    ValueRef only_left { };
    f.build(
        [ & ]( ir::IRBuilder& b )
        {
            const auto entry = b.add_block( { Type::i64 } );
            const auto left  = b.add_block( { } );
            const auto right = b.add_block( { } );
            const auto merge = b.add_block( { } );
            b.switch_to_block( entry );
            b.brif( b.param( 0 ), left, Args { }, right, Args { } );
            b.switch_to_block( left );
            only_left = b.iadd( b.param( 0 ), b.param( 0 ) );
            b.br( merge );
            b.switch_to_block( right );
            b.br( merge );
            b.switch_to_block( merge );
            b.ret( only_left );
        } );
    CHECK( f.errors( ) == Errs { std::format( "ret: uses v{} from a definition that does not dominate it", only_left.id ) } );
}

TEST_CASE( "verifier accepts a value used in a loop body that its header defines", "[verifier]" )
{
    Fn f { Type::i64, { Type::i64 } };
    f.build(
        []( ir::IRBuilder& b )
        {
            const auto entry  = b.add_block( { Type::i64 } );
            const auto header = b.add_block( { } );
            const auto body   = b.add_block( { } );
            const auto exit   = b.add_block( { } );
            b.switch_to_block( entry );
            b.br( header );
            b.switch_to_block( header );
            const auto x = b.iadd( b.param( 0 ), b.param( 0 ) );
            b.brif( x, body, Args { }, exit, Args { } );
            b.switch_to_block( body );
            ( void ) b.isub( x, b.param( 0 ) );
            b.br( header );
            b.switch_to_block( exit );
            b.ret( x );
        } );
    CHECK( f.errors( ).empty( ) );
}
