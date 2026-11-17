#include "support.hh"

#include <catch2/generators/catch_generators.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace xt;

namespace
{
    struct Built
    {
        ir::IRModule    irm;
        ir::FunctionRef ref { };

        [[nodiscard]] ir::IRFunction& fn( )
        { return *irm.get( ref ); }
    };

    template < typename Body >
    [[nodiscard]] std::unique_ptr< Built > build( const Type ret, const std::vector< Type >& params, Body&& body )
    {
        auto out = std::make_unique< Built >( );
        out->ref = out->irm.declare( "f", ret, std::span< const Type > { params } );
        ir::IRBuilder b { out->fn( ) };
        body( b );
        b.finalise( );
        return out;
    }

    [[nodiscard]] std::vector< uint32_t > ids( const std::span< const ValueRef > refs )
    {
        std::vector< uint32_t > out;
        for ( const auto r : refs ) out.push_back( r.id );
        return out;
    }

    [[nodiscard]] std::vector< uint32_t > ops_of( const ir::IRFunction& fn, const ir::IRInst& inst )
    { return ids( fn.operands.subspan( inst.ops_offset, inst.ops_count ) ); }

    [[nodiscard]] const ir::IRInst& def_of( const ir::IRFunction& fn, const ValueRef v )
    { return fn.insts[ fn.values[ v.id ].inst.id ]; }

    using U = std::vector< uint32_t >;

    constexpr auto cc = codegen::host_call_conv( );
} // namespace

TEST_CASE( "builder assigns sequential block ids and parameter values", "[ir][builder]" )
{
    auto  m  = build( Type::unit, { Type::i64, Type::i32 },
                      []( ir::IRBuilder& b )
                      {
                        CHECK( b.add_block( { Type::i64, Type::i32 } ).id == 0 );
                        CHECK( b.add_block( { } ).id == 1 );
                        CHECK( b.add_block( { Type::ptr } ).id == 2 );
                        CHECK( b.param( 0 ).id == 0 );
                        CHECK( b.param( 1 ).id == 1 );
                        CHECK( b.block_param( ir::BlockRef { 0 }, 1 ).id == 1 );
                        CHECK( b.block_param( ir::BlockRef { 2 }, 0 ).id == 2 );
                    } );
    auto& fn = m->fn( );

    REQUIRE( fn.blocks.size( ) == 3 );
    CHECK( fn.blocks[ 0 ].params_offset == 0 );
    CHECK( fn.blocks[ 0 ].params_count == 2 );
    CHECK( fn.blocks[ 1 ].params_offset == 2 );
    CHECK( fn.blocks[ 1 ].params_count == 0 );
    CHECK( fn.blocks[ 2 ].params_offset == 2 );
    CHECK( fn.blocks[ 2 ].params_count == 1 );
    CHECK( ids( fn.block_params ) == U { 0, 1, 2 } );

    REQUIRE( fn.values.size( ) == 3 );
    CHECK( fn.values[ 0 ].type == Type::i64 );
    CHECK( fn.values[ 1 ].type == Type::i32 );
    CHECK( fn.values[ 2 ].type == Type::ptr );
    for ( const auto& v : fn.values ) CHECK( v.inst.id == ~0u );

    CHECK( fn.insts.empty( ) );
    CHECK( fn.operands.empty( ) );
    CHECK( fn.succs.empty( ) );
    for ( const auto& blk : fn.blocks ) CHECK( blk.insts_count == 0 );
}

TEST_CASE( "builder add_block accepts a span of parameter types", "[ir][builder]" )
{
    const std::vector< Type > params { Type::i8, Type::i16, Type::ptr };
    auto                      m  = build( Type::unit, params,
                                          [ & ]( ir::IRBuilder& b )
                                          {
                        const auto e = b.add_block( std::span< const Type > { params } );
                        b.switch_to_block( e );
                        b.ret( );
                    } );
    auto&                     fn = m->fn( );
    REQUIRE( fn.blocks.size( ) == 1 );
    CHECK( fn.blocks[ 0 ].params_count == 3 );
    CHECK( fn.values[ 0 ].type == Type::i8 );
    CHECK( fn.values[ 1 ].type == Type::i16 );
    CHECK( fn.values[ 2 ].type == Type::ptr );
}

TEST_CASE( "builder lays out a single block function", "[ir][builder]" )
{
    auto  m  = build( Type::i64, { Type::i64, Type::i64 },
                      []( ir::IRBuilder& b )
                      {
                        b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
                        const auto s = b.iadd( b.param( 0 ), b.param( 1 ) );
                        const auto k = b.iconst( Type::i64, 7 );
                        b.ret( b.imul( s, k ) );
                    } );
    auto& fn = m->fn( );

    REQUIRE( fn.insts.size( ) == 4 );
    CHECK( fn.insts[ 0 ].op == ir::IROp::iadd );
    CHECK( fn.insts[ 0 ].result.id == 2 );
    CHECK( fn.insts[ 0 ].ops_offset == 0 );
    CHECK( fn.insts[ 0 ].ops_count == 2 );
    CHECK( fn.insts[ 1 ].op == ir::IROp::iconst );
    CHECK( fn.insts[ 1 ].result.id == 3 );
    CHECK( fn.insts[ 1 ].ops_offset == 2 );
    CHECK( fn.insts[ 1 ].ops_count == 0 );
    CHECK( fn.insts[ 1 ].imm == 7 );
    CHECK( fn.insts[ 2 ].op == ir::IROp::imul );
    CHECK( fn.insts[ 2 ].result.id == 4 );
    CHECK( ops_of( fn, fn.insts[ 2 ] ) == U { 2, 3 } );
    CHECK( fn.insts[ 3 ].op == ir::IROp::ret );
    CHECK( fn.insts[ 3 ].result.id == 5 );
    CHECK( ops_of( fn, fn.insts[ 3 ] ) == U { 4 } );

    CHECK( ids( fn.operands ) == U { 0, 1, 2, 3, 4 } );

    REQUIRE( fn.values.size( ) == 6 );
    CHECK( fn.values[ 2 ].inst.id == 0 );
    CHECK( fn.values[ 3 ].inst.id == 1 );
    CHECK( fn.values[ 4 ].inst.id == 2 );
    CHECK( fn.values[ 5 ].inst.id == 3 );
    CHECK( fn.values[ 5 ].type == Type::unit );

    REQUIRE( fn.blocks.size( ) == 1 );
    CHECK( fn.blocks[ 0 ].insts_offset == 0 );
    CHECK( fn.blocks[ 0 ].insts_count == 4 );
    CHECK( fn.blocks[ 0 ].succs_count == 0 );

    CHECK( fn.ext_funcs.empty( ) );
    CHECK( fn.ext_param_types.empty( ) );
    CHECK( fn.global_values.empty( ) );
    CHECK( fn.signatures.empty( ) );
}

TEST_CASE( "builder gives every instruction a value, unit for effects", "[ir][builder]" )
{
    auto  m  = build( Type::unit, { Type::ptr, Type::i64 },
                      []( ir::IRBuilder& b )
                      {
                        const auto e    = b.add_block( { Type::ptr, Type::i64 } );
                        const auto next = b.add_block( { } );
                        b.switch_to_block( e );
                        b.store( b.param( 1 ), b.param( 0 ) );
                        b.br( next );
                        b.switch_to_block( next );
                        b.ret( );
                    } );
    auto& fn = m->fn( );
    REQUIRE( fn.insts.size( ) == 3 );
    for ( const auto& inst : fn.insts ) CHECK( fn.values[ inst.result.id ].type == Type::unit );
    CHECK( fn.insts[ 0 ].op == ir::IROp::store );
    CHECK( fn.insts[ 1 ].op == ir::IROp::br );
    CHECK( fn.insts[ 2 ].op == ir::IROp::ret );
    CHECK( fn.insts[ 2 ].ops_count == 0 );
    CHECK( fn.values.size( ) == 5 );
}

TEST_CASE( "builder result types follow each opcode's rule", "[ir][builder]" )
{
    std::vector< std::pair< ValueRef, Type > > expect;
    auto                                       m  = build( Type::unit, { Type::i8, Type::i16, Type::i32, Type::i64, Type::ptr },
                                                           [ & ]( ir::IRBuilder& b )
                                                           {
                        const auto f = b.import_function( { ir::extns::function, 0 }, { Type::i64 }, Type::i32, cc, false );
                        const auto s = b.import_signature( std::span< const Type > { }, Type::i16, cc );
                        const auto g = b.create_global_value( {
                                                                   .name = { ir::extns::data, 0 }
                        } );
                        b.switch_to_block( b.add_block( { Type::i8, Type::i16, Type::i32, Type::i64, Type::ptr } ) );
                        const auto p8  = b.param( 0 );
                        const auto p16 = b.param( 1 );
                        const auto p32 = b.param( 2 );
                        const auto p64 = b.param( 3 );
                        const auto pp  = b.param( 4 );

                        const auto c = b.icmp( ir::ICmpCC::eq, p64, p64 );
                        expect.emplace_back( c, Type::i8 );
                        expect.emplace_back( b.select( c, p32, p32 ), Type::i32 );
                        expect.emplace_back( b.select( c, pp, pp ), Type::ptr );
                        expect.emplace_back( b.bnot( p16 ), Type::i16 );
                        expect.emplace_back( b.ineg( p32 ), Type::i32 );
                        expect.emplace_back( b.uextend( Type::i64, p8 ), Type::i64 );
                        expect.emplace_back( b.sextend( Type::i32, p16 ), Type::i32 );
                        expect.emplace_back( b.ireduce( Type::i8, p64 ), Type::i8 );
                        expect.emplace_back( b.load( Type::i16, pp ), Type::i16 );
                        expect.emplace_back( b.load( Type::ptr, pp ), Type::ptr );
                        expect.emplace_back( b.iadd( pp, p64 ), Type::ptr );
                        expect.emplace_back( b.isub( p8, p8 ), Type::i8 );
                        expect.emplace_back( b.ishl( p32, p8 ), Type::i32 );
                        expect.emplace_back( b.sshr( p64, p8 ), Type::i64 );
                        expect.emplace_back( b.iconst( Type::i16, 1 ), Type::i16 );
                        const auto fa = b.func_addr( Type::ptr, f );
                        expect.emplace_back( fa, Type::ptr );
                        expect.emplace_back( b.symbol_value( Type::ptr, g ), Type::ptr );
                        const ValueRef args[] = { p64 };
                        expect.emplace_back( b.call( f, args ), Type::i32 );
                        expect.emplace_back( b.call_indirect( s, fa, { } ), Type::i16 );
                        b.ret( );
                    } );
    auto&                                      fn = m->fn( );
    for ( const auto& [ v, ty ] : expect ) CHECK( fn.values[ v.id ].type == ty );
}

TEST_CASE( "builder binary ops record opcode and operand order", "[ir][builder]" )
{
    using Bin                 = ValueRef ( ir::IRBuilder::* )( ValueRef, ValueRef );
    const auto [ method, op ] = GENERATE( values< std::pair< Bin, ir::IROp > >( {
        {&ir::IRBuilder::iadd, ir::IROp::iadd},
        {&ir::IRBuilder::isub, ir::IROp::isub},
        {&ir::IRBuilder::imul, ir::IROp::imul},
        {&ir::IRBuilder::idiv, ir::IROp::idiv},
        {&ir::IRBuilder::udiv, ir::IROp::udiv},
        {&ir::IRBuilder::srem, ir::IROp::srem},
        {&ir::IRBuilder::urem, ir::IROp::urem},
        {&ir::IRBuilder::band, ir::IROp::band},
        { &ir::IRBuilder::bor,  ir::IROp::bor},
        {&ir::IRBuilder::bxor, ir::IROp::bxor},
        {&ir::IRBuilder::ishl, ir::IROp::ishl},
        {&ir::IRBuilder::ushr, ir::IROp::ushr},
        {&ir::IRBuilder::sshr, ir::IROp::sshr},
    } ) );

    ValueRef    r { };
    auto        m    = build( Type::i64, { Type::i64, Type::i64 },
                              [ & ]( ir::IRBuilder& b )
                              {
                        b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
                        r = ( b.*method )( b.param( 1 ), b.param( 0 ) );
                        b.ret( r );
                    } );
    auto&       fn   = m->fn( );
    const auto& inst = def_of( fn, r );
    CHECK( inst.op == op );
    CHECK( inst.result.id == r.id );
    CHECK( ops_of( fn, inst ) == U { 1, 0 } );
    CHECK( inst.imm == 0 );
    CHECK( fn.values[ r.id ].type == Type::i64 );
}

TEST_CASE( "builder unary ops record a single operand", "[ir][builder]" )
{
    ValueRef n { }, g { }, z { }, s { }, rd { };
    auto     m  = build( Type::unit, { Type::i32 },
                         [ & ]( ir::IRBuilder& b )
                         {
                        b.switch_to_block( b.add_block( { Type::i32 } ) );
                        n  = b.bnot( b.param( 0 ) );
                        g  = b.ineg( n );
                        z  = b.uextend( Type::i64, g );
                        s  = b.sextend( Type::i64, b.param( 0 ) );
                        rd = b.ireduce( Type::i16, z );
                        b.ret( );
                    } );
    auto&    fn = m->fn( );
    CHECK( def_of( fn, n ).op == ir::IROp::bnot );
    CHECK( ops_of( fn, def_of( fn, n ) ) == U { 0 } );
    CHECK( def_of( fn, g ).op == ir::IROp::ineg );
    CHECK( ops_of( fn, def_of( fn, g ) ) == U { n.id } );
    CHECK( def_of( fn, z ).op == ir::IROp::uextend );
    CHECK( ops_of( fn, def_of( fn, z ) ) == U { g.id } );
    CHECK( def_of( fn, s ).op == ir::IROp::sextend );
    CHECK( def_of( fn, rd ).op == ir::IROp::ireduce );
    CHECK( fn.values[ rd.id ].type == Type::i16 );
}

TEST_CASE( "builder icmp records its condition code", "[ir][builder]" )
{
    const auto cond = GENERATE( ir::ICmpCC::eq, ir::ICmpCC::ne, ir::ICmpCC::slt, ir::ICmpCC::sle, ir::ICmpCC::sgt, ir::ICmpCC::sge,
                                ir::ICmpCC::ult, ir::ICmpCC::ule, ir::ICmpCC::ugt, ir::ICmpCC::uge );
    ValueRef   c { };
    auto       m  = build( Type::i8, { Type::i64, Type::i64 },
                           [ & ]( ir::IRBuilder& b )
                           {
                        b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
                        c = b.icmp( cond, b.param( 0 ), b.param( 1 ) );
                        b.ret( c );
                    } );
    auto&      fn = m->fn( );
    CHECK( def_of( fn, c ).op == ir::IROp::icmp );
    CHECK( def_of( fn, c ).cc == cond );
    CHECK( ops_of( fn, def_of( fn, c ) ) == U { 0, 1 } );
    CHECK( fn.values[ c.id ].type == Type::i8 );
}

TEST_CASE( "builder select records condition then both values", "[ir][builder]" )
{
    ValueRef s { };
    auto     m  = build( Type::i64, { Type::i8, Type::i64, Type::i64 },
                         [ & ]( ir::IRBuilder& b )
                         {
                        b.switch_to_block( b.add_block( { Type::i8, Type::i64, Type::i64 } ) );
                        s = b.select( b.param( 0 ), b.param( 1 ), b.param( 2 ) );
                        b.ret( s );
                    } );
    auto&    fn = m->fn( );
    CHECK( def_of( fn, s ).op == ir::IROp::select );
    CHECK( ops_of( fn, def_of( fn, s ) ) == U { 0, 1, 2 } );
}

TEST_CASE( "builder iconst stores the immediate verbatim", "[ir][builder]" )
{
    const auto [ ty, imm ] = GENERATE( values< std::pair< Type, uint64_t > >( {
        {Type::i64,                     0},
        {Type::i64,       ~uint64_t { 0 }},
        {Type::i64, 0x8000000000000000ull},
        { Type::i8,                   300},
        {Type::i16,               0x12345},
        {Type::i32,         0xFFFFFFFFull},
        {Type::ptr, 0xDEADBEEFCAFEBABEull},
    } ) );
    ValueRef k { };
    auto     m  = build( Type::unit, { },
                         [ & ]( ir::IRBuilder& b )
                         {
                        b.switch_to_block( b.add_block( { } ) );
                        k = b.iconst( ty, imm );
                        b.ret( );
                    } );
    auto&    fn = m->fn( );
    CHECK( def_of( fn, k ).imm == imm );
    CHECK( def_of( fn, k ).ops_count == 0 );
    CHECK( fn.values[ k.id ].type == ty );
}

TEST_CASE( "builder load and store keep sign extended offsets", "[ir][builder]" )
{
    const int32_t off
        = GENERATE( 0, 1, -1, 8, -8, 4096, -4096, std::numeric_limits< int32_t >::min( ), std::numeric_limits< int32_t >::max( ) );
    ValueRef    l { };
    auto        m  = build( Type::unit, { Type::ptr, Type::i32 },
                            [ & ]( ir::IRBuilder& b )
                            {
                        b.switch_to_block( b.add_block( { Type::ptr, Type::i32 } ) );
                        l = b.load( Type::i32, b.param( 0 ), off );
                        b.store( b.param( 1 ), b.param( 0 ), off );
                        b.ret( );
                    } );
    auto&       fn = m->fn( );
    const auto& ld = def_of( fn, l );
    CHECK( ld.op == ir::IROp::load );
    CHECK( ld.imm == static_cast< uint64_t >( static_cast< int64_t >( off ) ) );
    CHECK( ir::mem_offset( ld ) == off );
    CHECK( ops_of( fn, ld ) == U { 0 } );

    const auto& st = fn.insts[ 1 ];
    CHECK( st.op == ir::IROp::store );
    CHECK( st.imm == static_cast< uint64_t >( static_cast< int64_t >( off ) ) );
    CHECK( ir::mem_offset( st ) == off );
    CHECK( ops_of( fn, st ) == U { 1, 0 } );
}

TEST_CASE( "builder load and store default to a zero offset", "[ir][builder]" )
{
    auto  m  = build( Type::unit, { Type::ptr },
                      []( ir::IRBuilder& b )
                      {
                        b.switch_to_block( b.add_block( { Type::ptr } ) );
                        b.store( b.load( Type::i64, b.param( 0 ) ), b.param( 0 ) );
                        b.ret( );
                    } );
    auto& fn = m->fn( );
    CHECK( fn.insts[ 0 ].imm == 0 );
    CHECK( fn.insts[ 1 ].imm == 0 );
}

TEST_CASE( "builder lays out a diamond with block arguments", "[ir][builder]" )
{
    auto  m  = build( Type::i64, { Type::i64 },
                      []( ir::IRBuilder& b )
                      {
                        const auto b0 = b.add_block( { Type::i64 } );
                        const auto b1 = b.add_block( { Type::i64 } );
                        const auto b2 = b.add_block( { } );
                        b.switch_to_block( b0 );
                        const auto z = b.iconst( Type::i64, 0 );
                        const auto c = b.icmp( ir::ICmpCC::eq, b.param( 0 ), z );
                        b.brif( c, b1, Args { b.param( 0 ) }, b2, Args { } );
                        b.switch_to_block( b1 );
                        b.ret( b.block_param( b1, 0 ) );
                        b.switch_to_block( b2 );
                        b.ret( z );
                    } );
    auto& fn = m->fn( );

    REQUIRE( fn.blocks.size( ) == 3 );
    CHECK( fn.blocks[ 0 ].insts_offset == 0 );
    CHECK( fn.blocks[ 0 ].insts_count == 3 );
    CHECK( fn.blocks[ 0 ].succs_offset == 0 );
    CHECK( fn.blocks[ 0 ].succs_count == 2 );
    CHECK( fn.blocks[ 1 ].insts_offset == 3 );
    CHECK( fn.blocks[ 1 ].insts_count == 1 );
    CHECK( fn.blocks[ 1 ].succs_count == 0 );
    CHECK( fn.blocks[ 2 ].insts_offset == 4 );
    CHECK( fn.blocks[ 2 ].insts_count == 1 );
    CHECK( fn.blocks[ 2 ].succs_count == 0 );

    REQUIRE( fn.succs.size( ) == 2 );
    CHECK( fn.succs[ 0 ].id == 1 );
    CHECK( fn.succs[ 1 ].id == 2 );

    const auto& term = fn.insts[ 2 ];
    CHECK( term.op == ir::IROp::brif );
    CHECK( ops_of( fn, term ) == U { 3, 0 } );

    const auto [ then_args, else_args ] = ir::branch_args( fn, fn.blocks[ 0 ], term );
    CHECK( ids( then_args ) == U { 0 } );
    CHECK( else_args.empty( ) );

    CHECK( ops_of( fn, fn.insts[ 3 ] ) == U { 1 } );
    CHECK( ops_of( fn, fn.insts[ 4 ] ) == U { 2 } );
}

TEST_CASE( "builder block order is creation order, instruction order is fill order", "[ir][builder]" )
{
    auto  m  = build( Type::unit, { },
                      []( ir::IRBuilder& b )
                      {
                        const auto b0 = b.add_block( { } );
                        const auto b1 = b.add_block( { } );
                        const auto b2 = b.add_block( { } );
                        b.switch_to_block( b0 );
                        b.br( b2 );
                        b.switch_to_block( b2 );
                        ( void ) b.iconst( Type::i64, 1 );
                        b.br( b1 );
                        b.switch_to_block( b1 );
                        b.ret( );
                    } );
    auto& fn = m->fn( );
    CHECK( fn.blocks[ 0 ].insts_offset == 0 );
    CHECK( fn.blocks[ 0 ].insts_count == 1 );
    CHECK( fn.blocks[ 2 ].insts_offset == 1 );
    CHECK( fn.blocks[ 2 ].insts_count == 2 );
    CHECK( fn.blocks[ 1 ].insts_offset == 3 );
    CHECK( fn.blocks[ 1 ].insts_count == 1 );
    CHECK( fn.insts[ fn.blocks[ 1 ].insts_offset ].op == ir::IROp::ret );
}

TEST_CASE( "builder br records its arguments and one successor", "[ir][builder]" )
{
    auto  m  = build( Type::i64, { Type::i64, Type::i64 },
                      []( ir::IRBuilder& b )
                      {
                        const auto e = b.add_block( { Type::i64, Type::i64 } );
                        const auto t = b.add_block( { Type::i64, Type::i64 } );
                        b.switch_to_block( e );
                        b.br( t, Args { b.param( 1 ), b.param( 0 ) } );
                        b.switch_to_block( t );
                        b.ret( b.block_param( t, 0 ) );
                    } );
    auto& fn = m->fn( );
    CHECK( fn.blocks[ 0 ].succs_count == 1 );
    CHECK( fn.succs[ fn.blocks[ 0 ].succs_offset ].id == 1 );
    const auto& term = fn.insts[ 0 ];
    CHECK( term.op == ir::IROp::br );
    CHECK( ops_of( fn, term ) == U { 1, 0 } );
    const auto [ then_args, else_args ] = ir::branch_args( fn, fn.blocks[ 0 ], term );
    CHECK( ids( then_args ) == U { 1, 0 } );
    CHECK( else_args.empty( ) );
}

TEST_CASE( "builder brif lays out condition, then args, else args", "[ir][builder]" )
{
    auto        m    = build( Type::i64, { Type::i64, Type::i64, Type::i8 },
                              []( ir::IRBuilder& b )
                              {
                        const auto e  = b.add_block( { Type::i64, Type::i64, Type::i8 } );
                        const auto t  = b.add_block( { Type::i64, Type::i64 } );
                        const auto el = b.add_block( { Type::i8 } );
                        b.switch_to_block( e );
                        b.brif( b.param( 2 ), t, Args { b.param( 0 ), b.param( 1 ) }, el, Args { b.param( 2 ) } );
                        b.switch_to_block( t );
                        b.ret( b.block_param( t, 1 ) );
                        b.switch_to_block( el );
                        b.ret( b.uextend( Type::i64, b.block_param( el, 0 ) ) );
                    } );
    auto&       fn   = m->fn( );
    const auto& term = fn.insts[ 0 ];
    CHECK( term.op == ir::IROp::brif );
    CHECK( ops_of( fn, term ) == U { 2, 0, 1, 2 } );
    const auto [ then_args, else_args ] = ir::branch_args( fn, fn.blocks[ 0 ], term );
    CHECK( ids( then_args ) == U { 0, 1 } );
    CHECK( ids( else_args ) == U { 2 } );
}

TEST_CASE( "builder brif to the same block twice keeps both edges", "[ir][builder]" )
{
    auto  m  = build( Type::i64, { Type::i8 },
                      []( ir::IRBuilder& b )
                      {
                        const auto e = b.add_block( { Type::i8 } );
                        const auto t = b.add_block( { Type::i64 } );
                        b.switch_to_block( e );
                        const auto one = b.iconst( Type::i64, 1 );
                        const auto two = b.iconst( Type::i64, 2 );
                        b.brif( b.param( 0 ), t, Args { one }, t, Args { two } );
                        b.switch_to_block( t );
                        b.ret( b.block_param( t, 0 ) );
                    } );
    auto& fn = m->fn( );
    CHECK( fn.blocks[ 0 ].succs_count == 2 );
    CHECK( fn.succs[ 0 ].id == 1 );
    CHECK( fn.succs[ 1 ].id == 1 );
    const auto [ then_args, else_args ] = ir::branch_args( fn, fn.blocks[ 0 ], fn.insts[ 2 ] );
    CHECK( ids( then_args ) == U { 2 } );
    CHECK( ids( else_args ) == U { 3 } );
}

TEST_CASE( "builder successor pool accumulates across blocks", "[ir][builder]" )
{
    auto  m  = build( Type::unit, { },
                      []( ir::IRBuilder& b )
                      {
                        const auto b0 = b.add_block( { } );
                        const auto b1 = b.add_block( { } );
                        const auto b2 = b.add_block( { } );
                        const auto b3 = b.add_block( { } );
                        b.switch_to_block( b0 );
                        b.brif( b.iconst( Type::i8, 1 ), b1, Args { }, b2, Args { } );
                        b.switch_to_block( b1 );
                        b.br( b3 );
                        b.switch_to_block( b2 );
                        b.brif( b.iconst( Type::i8, 0 ), b3, Args { }, b1, Args { } );
                        b.switch_to_block( b3 );
                        b.ret( );
                    } );
    auto& fn = m->fn( );
    REQUIRE( fn.succs.size( ) == 5 );
    CHECK( fn.blocks[ 0 ].succs_offset == 0 );
    CHECK( fn.blocks[ 0 ].succs_count == 2 );
    CHECK( fn.blocks[ 1 ].succs_offset == 2 );
    CHECK( fn.blocks[ 1 ].succs_count == 1 );
    CHECK( fn.blocks[ 2 ].succs_offset == 3 );
    CHECK( fn.blocks[ 2 ].succs_count == 2 );
    CHECK( fn.blocks[ 3 ].succs_count == 0 );
    std::vector< uint32_t > succs;
    for ( const auto s : fn.succs ) succs.push_back( s.id );
    CHECK( succs == U { 1, 2, 3, 3, 1 } );
}

TEST_CASE( "builder leaves a never visited block empty", "[ir][builder]" )
{
    auto  m  = build( Type::unit, { },
                      []( ir::IRBuilder& b )
                      {
                        const auto e = b.add_block( { } );
                        ( void ) b.add_block( { Type::i64 } );
                        b.switch_to_block( e );
                        ( void ) b.iconst( Type::i64, 5 );
                        b.ret( );
                    } );
    auto& fn = m->fn( );
    REQUIRE( fn.blocks.size( ) == 2 );
    CHECK( fn.blocks[ 0 ].insts_count == 2 );
    CHECK( fn.blocks[ 1 ].insts_offset == 0 );
    CHECK( fn.blocks[ 1 ].insts_count == 0 );
    CHECK( fn.blocks[ 1 ].params_count == 1 );
}

TEST_CASE( "branch_args is empty for non branch instructions", "[ir][builder]" )
{
    auto  m  = build( Type::i64, { Type::i64 },
                      []( ir::IRBuilder& b )
                      {
                        b.switch_to_block( b.add_block( { Type::i64 } ) );
                        b.ret( b.iadd( b.param( 0 ), b.param( 0 ) ) );
                    } );
    auto& fn = m->fn( );
    for ( const auto& inst : fn.insts )
    {
        const auto [ then_args, else_args ] = ir::branch_args( fn, fn.blocks[ 0 ], inst );
        CHECK( then_args.empty( ) );
        CHECK( else_args.empty( ) );
    }
}

TEST_CASE( "builder import_function records imports and shared parameter pool", "[ir][builder]" )
{
    const std::vector< Type > span_params { Type::i8, Type::i16 };
    ir::FuncRef               f0 { }, f1 { }, f2 { };
    ir::SigRef                s0 { };
    auto                      m  = build( Type::unit, { },
                                          [ & ]( ir::IRBuilder& b )
                                          {
                        f0 = b.import_function( { ir::extns::function, 3 }, { Type::i64, Type::i32 }, Type::i64, codegen::CallConv::SystemV,
                                                                      true );
                        s0 = b.import_signature( { Type::ptr }, Type::i32, codegen::CallConv::WindowsFastcall );
                        f1 = b.import_function( { ir::extns::function, 9 }, std::span< const Type > { }, Type::unit, cc, false );
                        f2 = b.import_function( { ir::extns::data, 1 }, std::span< const Type > { span_params }, Type::ptr, cc, true );
                        b.switch_to_block( b.add_block( { } ) );
                        b.ret( );
                    } );
    auto&                     fn = m->fn( );

    CHECK( f0.id == 0 );
    CHECK( f1.id == 1 );
    CHECK( f2.id == 2 );
    CHECK( s0.id == 0 );

    REQUIRE( fn.ext_funcs.size( ) == 3 );
    REQUIRE( fn.signatures.size( ) == 1 );
    REQUIRE( fn.ext_param_types.size( ) == 5 );
    CHECK( fn.ext_param_types[ 0 ] == Type::i64 );
    CHECK( fn.ext_param_types[ 1 ] == Type::i32 );
    CHECK( fn.ext_param_types[ 2 ] == Type::ptr );
    CHECK( fn.ext_param_types[ 3 ] == Type::i8 );
    CHECK( fn.ext_param_types[ 4 ] == Type::i16 );

    const auto& e0 = fn.ext_funcs[ 0 ];
    CHECK( e0.name == ir::ExternalName { ir::extns::function, 3 } );
    CHECK( e0.ret == Type::i64 );
    CHECK( e0.params_offset == 0 );
    CHECK( e0.params_count == 2 );
    CHECK( e0.cc == codegen::CallConv::SystemV );
    CHECK( e0.colocated );

    const auto& sg = fn.signatures[ 0 ];
    CHECK( sg.ret == Type::i32 );
    CHECK( sg.params_offset == 2 );
    CHECK( sg.params_count == 1 );
    CHECK( sg.cc == codegen::CallConv::WindowsFastcall );

    const auto& e1 = fn.ext_funcs[ 1 ];
    CHECK( e1.params_offset == 3 );
    CHECK( e1.params_count == 0 );
    CHECK( e1.ret == Type::unit );
    CHECK_FALSE( e1.colocated );

    const auto& e2 = fn.ext_funcs[ 2 ];
    CHECK( e2.name == ir::ExternalName { ir::extns::data, 1 } );
    CHECK( e2.params_offset == 3 );
    CHECK( e2.params_count == 2 );
    CHECK( e2.ret == Type::ptr );
}

TEST_CASE( "builder import_signature accepts a span", "[ir][builder]" )
{
    const std::vector< Type > params { Type::i64, Type::i64, Type::ptr };
    auto                      m  = build( Type::unit, { },
                                          [ & ]( ir::IRBuilder& b )
                                          {
                        CHECK( b.import_signature( std::span< const Type > { params }, Type::i64, cc ).id == 0 );
                        CHECK( b.import_signature( { }, Type::unit, cc ).id == 1 );
                        b.switch_to_block( b.add_block( { } ) );
                        b.ret( );
                    } );
    auto&                     fn = m->fn( );
    REQUIRE( fn.signatures.size( ) == 2 );
    CHECK( fn.signatures[ 0 ].params_count == 3 );
    CHECK( fn.signatures[ 1 ].params_offset == 3 );
    CHECK( fn.signatures[ 1 ].params_count == 0 );
    CHECK( fn.signatures[ 1 ].ret == Type::unit );
}

TEST_CASE( "builder call records callee index, arguments and result", "[ir][builder]" )
{
    ValueRef r0 { }, r1 { };
    auto     m  = build( Type::unit, { Type::i64, Type::ptr },
                         [ & ]( ir::IRBuilder& b )
                         {
                        const auto f = b.import_function( { ir::extns::function, 0 }, { Type::i64, Type::ptr }, Type::i64, cc, true );
                        const auto g = b.import_function( { ir::extns::function, 1 }, std::span< const Type > { }, Type::unit, cc, false );
                        b.switch_to_block( b.add_block( { Type::i64, Type::ptr } ) );
                        r0 = b.call( f, Args { b.param( 0 ), b.param( 1 ) } );
                        r1 = b.call( g, { } );
                        b.ret( );
                    } );
    auto&    fn = m->fn( );
    CHECK( def_of( fn, r0 ).op == ir::IROp::call );
    CHECK( def_of( fn, r0 ).imm == 0 );
    CHECK( ops_of( fn, def_of( fn, r0 ) ) == U { 0, 1 } );
    CHECK( fn.values[ r0.id ].type == Type::i64 );
    CHECK( def_of( fn, r1 ).imm == 1 );
    CHECK( def_of( fn, r1 ).ops_count == 0 );
    CHECK( fn.values[ r1.id ].type == Type::unit );
}

TEST_CASE( "builder call_indirect puts the callee before the arguments", "[ir][builder]" )
{
    ValueRef addr { }, r0 { }, r1 { };
    auto     m  = build( Type::unit, { Type::i64 },
                         [ & ]( ir::IRBuilder& b )
                         {
                        const auto f  = b.import_function( { ir::extns::function, 0 }, { Type::i64 }, Type::i64, cc, true );
                        const auto s0 = b.import_signature( { Type::i64 }, Type::i64, cc );
                        const auto s1 = b.import_signature( { }, Type::unit, cc );
                        b.switch_to_block( b.add_block( { Type::i64 } ) );
                        addr = b.func_addr( Type::ptr, f );
                        r0   = b.call_indirect( s0, addr, Args { b.param( 0 ) } );
                        r1   = b.call_indirect( s1, addr, { } );
                        b.ret( );
                    } );
    auto&    fn = m->fn( );
    CHECK( def_of( fn, addr ).op == ir::IROp::func_addr );
    CHECK( def_of( fn, addr ).imm == 0 );
    CHECK( def_of( fn, addr ).ops_count == 0 );
    CHECK( def_of( fn, r0 ).op == ir::IROp::call_indirect );
    CHECK( def_of( fn, r0 ).imm == 0 );
    CHECK( ops_of( fn, def_of( fn, r0 ) ) == U { addr.id, 0 } );
    CHECK( fn.values[ r0.id ].type == Type::i64 );
    CHECK( def_of( fn, r1 ).imm == 1 );
    CHECK( ops_of( fn, def_of( fn, r1 ) ) == U { addr.id } );
    CHECK( fn.values[ r1.id ].type == Type::unit );
}

TEST_CASE( "builder global values and symbol_value", "[ir][builder]" )
{
    ValueRef s0 { }, s2 { };
    auto     m  = build( Type::unit, { },
                         [ & ]( ir::IRBuilder& b )
                         {
                        CHECK( b.create_global_value( {
                                                               .name      = { ir::extns::data, 4 },
                                                               .offset    = 0,
                                                               .colocated = true
                        } )
                                        .id
                                    == 0 );
                        CHECK( b.create_global_value( {
                                                               .name      = { ir::extns::data, 5 },
                                                               .offset    = 16,
                                                               .colocated = false
                        } )
                                        .id
                                    == 1 );
                        const auto g2 = b.create_global_value( {
                                 .name      = { ir::extns::function, 2 },
                                 .offset    = -8,
                                 .colocated = true
                        } );
                        CHECK( g2.id == 2 );
                        b.switch_to_block( b.add_block( { } ) );
                        s0 = b.symbol_value( Type::ptr, ir::GlobalValueRef { 0 } );
                        s2 = b.symbol_value( Type::ptr, g2 );
                        b.ret( );
                    } );
    auto&    fn = m->fn( );
    REQUIRE( fn.global_values.size( ) == 3 );
    CHECK( fn.global_values[ 0 ].name == ir::ExternalName { ir::extns::data, 4 } );
    CHECK( fn.global_values[ 0 ].offset == 0 );
    CHECK( fn.global_values[ 0 ].colocated );
    CHECK( fn.global_values[ 1 ].offset == 16 );
    CHECK_FALSE( fn.global_values[ 1 ].colocated );
    CHECK( fn.global_values[ 2 ].offset == -8 );
    CHECK( def_of( fn, s0 ).op == ir::IROp::symbol_value );
    CHECK( def_of( fn, s0 ).imm == 0 );
    CHECK( def_of( fn, s0 ).ops_count == 0 );
    CHECK( def_of( fn, s2 ).imm == 2 );
}

TEST_CASE( "builder stack slots and stack_addr", "[ir][builder]" )
{
    ValueRef a { }, c { };
    auto     m  = build( Type::unit, { },
                         [ & ]( ir::IRBuilder& b )
                         {
                        CHECK( b.create_stack_slot( 24, 16 ).id == 0 );
                        const auto s1 = b.create_stack_slot( 1, 1 );
                        CHECK( s1.id == 1 );
                        b.switch_to_block( b.add_block( { } ) );
                        a = b.stack_addr( Type::ptr, ir::StackSlotRef { 0 }, 20 );
                        c = b.stack_addr( Type::ptr, s1, -4 );
                        b.ret( );
                    } );
    auto&    fn = m->fn( );
    REQUIRE( fn.stack_slots.size( ) == 2 );
    CHECK( fn.stack_slots[ 0 ].size == 24 );
    CHECK( fn.stack_slots[ 0 ].align_shift == 4 );
    CHECK( fn.stack_slots[ 1 ].size == 1 );
    CHECK( fn.stack_slots[ 1 ].align_shift == 0 );
    CHECK( def_of( fn, a ).op == ir::IROp::stack_addr );
    CHECK( def_of( fn, a ).ops_count == 0 );
    CHECK( ir::slot_of( def_of( fn, a ) ).id == 0 );
    CHECK( ir::slot_offset( def_of( fn, a ) ) == 20 );
    CHECK( ir::slot_of( def_of( fn, c ) ).id == 1 );
    CHECK( ir::slot_offset( def_of( fn, c ) ) == -4 );
}

TEST_CASE( "slot immediates round trip every offset", "[ir][builder]" )
{
    const int32_t  offset = GENERATE( 0, 1, -1, 4096, std::numeric_limits< int32_t >::min( ), std::numeric_limits< int32_t >::max( ) );
    const uint32_t id     = GENERATE( 0u, 7u, 0xFFFF'FFFFu );
    ir::IRInst     inst { };
    inst.imm = ir::slot_imm( ir::StackSlotRef { id }, offset );
    CHECK( ir::slot_of( inst ).id == id );
    CHECK( ir::slot_offset( inst ) == offset );
}

TEST_CASE( "builder finalise with no blocks leaves every pool empty", "[ir][builder]" )
{
    auto  m  = build( Type::unit, { }, []( ir::IRBuilder& ) { } );
    auto& fn = m->fn( );
    CHECK( fn.blocks.empty( ) );
    CHECK( fn.insts.empty( ) );
    CHECK( fn.operands.empty( ) );
    CHECK( fn.succs.empty( ) );
    CHECK( fn.values.empty( ) );
    CHECK( fn.block_params.empty( ) );
    CHECK( fn.ext_funcs.empty( ) );
    CHECK( fn.ext_param_types.empty( ) );
    CHECK( fn.global_values.empty( ) );
    CHECK( fn.signatures.empty( ) );
}

TEST_CASE( "builder finalise copies pools into the module arena", "[ir][builder]" )
{
    ir::IRModule irm;
    const auto   ref    = irm.declare( "f", Type::i64, std::span< const Type > { } );
    auto&        fn     = *irm.get( ref );
    const auto   before = irm.arena( ).bytes_used( );
    {
        ir::IRBuilder b { fn };
        b.switch_to_block( b.add_block( { } ) );
        b.ret( b.iconst( Type::i64, 1 ) );
        b.finalise( );
    }
    CHECK( fn.arena == &irm.arena( ) );
    CHECK( irm.arena( ).bytes_used( ) > before );
    REQUIRE( fn.insts.size( ) == 2 );
    CHECK( fn.insts[ 0 ].imm == 1 );
}

TEST_CASE( "builder large function keeps consistent pools", "[ir][builder]" )
{
    constexpr uint32_t n  = 500;
    auto               m  = build( Type::i64, { Type::i64 },
                                   []( ir::IRBuilder& b )
                                   {
                        b.switch_to_block( b.add_block( { Type::i64 } ) );
                        auto acc = b.param( 0 );
                        for ( uint32_t i { 0 }; i < n; ++i ) acc = b.iadd( acc, b.iconst( Type::i64, i ) );
                        b.ret( acc );
                    } );
    auto&              fn = m->fn( );
    CHECK( fn.insts.size( ) == 2 * n + 1 );
    CHECK( fn.values.size( ) == 2 * n + 2 );
    CHECK( fn.operands.size( ) == 2 * n + 1 );
    CHECK( fn.blocks[ 0 ].insts_count == 2 * n + 1 );
    for ( uint32_t i { 0 }; i < fn.insts.size( ); ++i )
    {
        CHECK( fn.values[ fn.insts[ i ].result.id ].inst.id == i );
        CHECK( fn.insts[ i ].ops_offset + fn.insts[ i ].ops_count <= fn.operands.size( ) );
    }
}

TEST_CASE( "module declare assigns sequential refs and records the signature", "[ir][module]" )
{
    ir::IRModule irm;
    const auto   a = irm.declare( "alpha", Type::i64, { Type::i64, Type::ptr } );
    const auto   b = irm.declare( "beta", Type::unit, std::span< const Type > { } );
    const auto   c = irm.declare( "gamma", Type::i32, { Type::i8 }, codegen::CallConv::SystemV );

    CHECK( a.id == 0 );
    CHECK( b.id == 1 );
    CHECK( c.id == 2 );

    const auto* fa = irm.get( a );
    CHECK( fa->self.id == 0 );
    CHECK( irm.string( fa->name ) == "alpha" );
    CHECK( fa->sig.ret == Type::i64 );
    REQUIRE( fa->sig.params.size( ) == 2 );
    CHECK( fa->sig.params[ 0 ] == Type::i64 );
    CHECK( fa->sig.params[ 1 ] == Type::ptr );
    CHECK( fa->sig.cc == codegen::host_call_conv( ) );
    CHECK( fa->arena == &irm.arena( ) );

    CHECK( irm.get( b )->sig.params.empty( ) );
    CHECK( irm.get( b )->sig.ret == Type::unit );
    CHECK( irm.get( c )->sig.cc == codegen::CallConv::SystemV );

    CHECK( irm.get( a )->blocks.empty( ) );
    CHECK( irm.get( a )->insts.empty( ) );
}

TEST_CASE( "module declare with a span copies the parameter types", "[ir][module]" )
{
    ir::IRModule        irm;
    std::vector< Type > params { Type::i8, Type::i16, Type::i32 };
    const auto          ref = irm.declare( "f", Type::i8, std::span< const Type > { params }, codegen::CallConv::WindowsFastcall );
    params[ 0 ]             = Type::ptr;
    params.clear( );
    const auto* fn = irm.get( ref );
    REQUIRE( fn->sig.params.size( ) == 3 );
    CHECK( fn->sig.params[ 0 ] == Type::i8 );
    CHECK( fn->sig.params[ 2 ] == Type::i32 );
    CHECK( fn->sig.cc == codegen::CallConv::WindowsFastcall );
}

TEST_CASE( "module functions lists declarations in order", "[ir][module]" )
{
    ir::IRModule irm;
    for ( int i { 0 }; i < 10; ++i ) ( void ) irm.declare( "fn" + std::to_string( i ), Type::i64, std::span< const Type > { } );
    const auto fns = irm.functions( );
    REQUIRE( fns.size( ) == 10 );
    for ( uint32_t i { 0 }; i < 10; ++i )
    {
        CHECK( fns[ i ]->self.id == i );
        CHECK( irm.string( fns[ i ]->name ) == "fn" + std::to_string( i ) );
        CHECK( fns[ i ] == irm.get( ir::FunctionRef { i } ) );
    }
    const ir::IRModule& cref = irm;
    CHECK( cref.get( ir::FunctionRef { 3 } ) == fns[ 3 ] );
}

TEST_CASE( "module declaring the same name twice shares the interned name", "[ir][module]" )
{
    ir::IRModule irm;
    const auto   a = irm.declare( "dup", Type::i64, std::span< const Type > { } );
    const auto   b = irm.declare( "dup", Type::i32, std::span< const Type > { } );
    CHECK( a.id != b.id );
    CHECK( irm.get( a )->name.id == irm.get( b )->name.id );
    CHECK( irm.get( a )->sig.ret == Type::i64 );
    CHECK( irm.get( b )->sig.ret == Type::i32 );
}

TEST_CASE( "module intern deduplicates strings", "[ir][module]" )
{
    ir::IRModule      irm;
    const std::string src { "symbol" };
    const auto [ r1, v1 ] = irm.intern( src );
    const auto [ r2, v2 ] = irm.intern( "symbol" );
    const auto [ r3, v3 ] = irm.intern( "other" );
    CHECK( r1.id == r2.id );
    CHECK( v1.data( ) == v2.data( ) );
    CHECK( v1 == "symbol" );
    CHECK( v1.data( ) != src.data( ) );
    CHECK( r3.id != r1.id );
    CHECK( v3 == "other" );
    CHECK( irm.string( r1 ) == "symbol" );
    CHECK( irm.string( r3 ) == "other" );
}

TEST_CASE( "module intern is case and length sensitive", "[ir][module]" )
{
    ir::IRModule irm;
    const auto   a = irm.intern( "abc" ).first;
    const auto   b = irm.intern( "ABC" ).first;
    const auto   c = irm.intern( "abcd" ).first;
    const auto   d = irm.intern( "ab" ).first;
    CHECK( a.id != b.id );
    CHECK( a.id != c.id );
    CHECK( a.id != d.id );
    CHECK( c.id != d.id );
    CHECK( irm.intern( "abc" ).first.id == a.id );
}

TEST_CASE( "module intern handles the empty string", "[ir][module]" )
{
    ir::IRModule irm;
    const auto [ r1, v1 ] = irm.intern( "" );
    const auto [ r2, v2 ] = irm.intern( "" );
    CHECK( r1.id == r2.id );
    CHECK( v1.empty( ) );
    CHECK( irm.string( r1 ).empty( ) );
}

TEST_CASE( "module with a tiny arena spills over many slabs", "[ir][module]" )
{
    ir::IRModule irm { 64 };
    for ( int i { 0 }; i < 50; ++i )
        ( void ) irm.declare( "a_rather_long_function_name_" + std::to_string( i ), Type::i64, { Type::i64, Type::i64 } );
    CHECK( irm.arena( ).slab_count( ) > 1 );
    for ( uint32_t i { 0 }; i < 50; ++i )
    {
        const auto* fn = irm.get( ir::FunctionRef { i } );
        CHECK( irm.string( fn->name ) == "a_rather_long_function_name_" + std::to_string( i ) );
        CHECK( fn->sig.params.size( ) == 2 );
    }
}

TEST_CASE( "module move keeps functions and names", "[ir][module]" )
{
    ir::IRModule a;
    const auto   ref = a.declare( "moved", Type::i64, { Type::i32 } );
    ir::IRModule b { std::move( a ) };
    const auto*  fn = b.get( ref );
    CHECK( b.string( fn->name ) == "moved" );
    REQUIRE( fn->sig.params.size( ) == 1 );
    CHECK( fn->sig.params[ 0 ] == Type::i32 );
    CHECK( b.intern( "moved" ).first.id == fn->name.id );
}

TEST_CASE( "module move keeps each function pointing at the module's arena", "[ir][module]" )
{
    ir::IRModule a;
    const auto   ref = a.declare( "f", Type::i64, std::span< const Type > { } );
    ir::IRModule b { std::move( a ) };
    CHECK( b.get( ref )->arena == &b.arena( ) );
}

TEST_CASE( "module move assignment keeps each function pointing at the module's arena", "[ir][module]" )
{
    ir::IRModule a;
    const auto   ref = a.declare( "f", Type::i64, std::span< const Type > { } );
    ir::IRModule b;
    b              = std::move( a );
    const auto* fn = b.get( ref );
    CHECK( fn->arena == &b.arena( ) );
    CHECK( b.string( fn->name ) == "f" );
}
