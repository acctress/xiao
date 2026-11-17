#include "support.hh"

using namespace xt;

namespace
{
    std::string named( const ir::ExternalName n )
    { return std::format( "{}{}", n.ns == ir::extns::function ? "fn" : "data", n.index ); }

    template < typename Body >
    std::string print_one( const Type ret, const std::vector< Type >& params, Body&& body,
                           const codegen::CallConv cc = codegen::host_call_conv( ) )
    {
        ir::IRModule m;
        const auto   r = m.declare( "f", ret, std::span< const Type > { params }, cc );
        {
            ir::IRBuilder b { *m.get( r ) };
            const auto    e = b.add_block( std::span< const Type > { params } );
            b.switch_to_block( e );
            Args args;
            for ( uint16_t i { 0 }; i < params.size( ); ++i ) args.push_back( b.param( i ) );
            body( b, args );
            b.finalise( );
        }
        std::string out;
        m.fmt( std::back_inserter( out ), named );
        return out;
    }

    constexpr auto other_cc
        = codegen::host_call_conv( ) == codegen::CallConv::SystemV ? codegen::CallConv::WindowsFastcall : codegen::CallConv::SystemV;
}

TEST_CASE( "printer writes a define line with named params", "[ir][print]" )
{
    CHECK( print_one( Type::i64, { Type::i64, Type::i32 },
                      []( ir::IRBuilder& b, const Args& a )
                      {
                          b.ret( a[ 0 ] );
                      } )
           == "define @f(%v0: i64, %v1: i32) -> i64 {\nblock0:\n    return %v0\n}\n" );
}

TEST_CASE( "printer leaves out the arrow for unit functions", "[ir][print]" )
{
    CHECK( print_one( Type::unit, { },
                      []( ir::IRBuilder& b, const Args& )
                      {
                          b.ret( );
                      } )
           == "define @f() {\nblock0:\n    return\n}\n" );
}

TEST_CASE( "printer writes the calling convention only when it is not the host's", "[ir][print]" )
{
    const auto host  = print_one( Type::unit, { },
                                  []( ir::IRBuilder& b, const Args& )
                                  {
                                     b.ret( );
                                  } );
    const auto other = print_one(
        Type::unit, { },
        []( ir::IRBuilder& b, const Args& )
        {
            b.ret( );
        },
        other_cc );
    CHECK( host.starts_with( "define @f() {\n" ) );
    CHECK( other.starts_with( std::format( "define @f() {} {{\n", codegen::to_string( other_cc ) ) ) );
}

TEST_CASE( "printer writes calling convention names", "[ir][print]" )
{
    CHECK( codegen::to_string( codegen::CallConv::SystemV ) == "system_v" );
    CHECK( codegen::to_string( codegen::CallConv::WindowsFastcall ) == "windows_fastcall" );
}

TEST_CASE( "printer writes every binary op", "[ir][print]" )
{
    using Fn                                                   = ValueRef ( ir::IRBuilder::* )( ValueRef, ValueRef );
    const std::vector< std::pair< std::string_view, Fn > > ops = {
        {"iadd", &ir::IRBuilder::iadd},
        {"isub", &ir::IRBuilder::isub},
        {"imul", &ir::IRBuilder::imul},
        {"idiv", &ir::IRBuilder::idiv},
        {"udiv", &ir::IRBuilder::udiv},
        {"srem", &ir::IRBuilder::srem},
        {"urem", &ir::IRBuilder::urem},
        {"band", &ir::IRBuilder::band},
        { "bor",  &ir::IRBuilder::bor},
        {"bxor", &ir::IRBuilder::bxor},
        {"ishl", &ir::IRBuilder::ishl},
        {"ushr", &ir::IRBuilder::ushr},
        {"sshr", &ir::IRBuilder::sshr},
    };
    for ( const auto& [ name, fn ] : ops )
    {
        INFO( name );
        CHECK( print_one( Type::i64, { Type::i64, Type::i64 },
                          [ & ]( ir::IRBuilder& b, const Args& a )
                          {
                              b.ret( ( b.*fn )( a[ 0 ], a[ 1 ] ) );
                          } )
               == std::format( "define @f(%v0: i64, %v1: i64) -> i64 {{\nblock0:\n    %v2 = {} %v0, %v1\n    return %v2\n}}\n", name ) );
    }
}

TEST_CASE( "printer writes unary ops and conversions", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { Type::i32 },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto n = b.bnot( a[ 0 ] );
                                     const auto g = b.ineg( n );
                                     const auto u = b.uextend( Type::i64, g );
                                     const auto s = b.sextend( Type::i64, a[ 0 ] );
                                     const auto r = b.ireduce( Type::i8, u );
                                     const auto w = b.uextend( Type::i64, r );
                                     b.ret( b.iadd( b.iadd( u, s ), w ) );
                                 } );
    CHECK( text
           == "define @f(%v0: i32) -> i64 {\n"
              "block0:\n"
              "    %v1 = bnot %v0\n"
              "    %v2 = ineg %v1\n"
              "    %v3 = uextend.i64 %v2\n"
              "    %v4 = sextend.i64 %v0\n"
              "    %v5 = ireduce.i8 %v3\n"
              "    %v6 = uextend.i64 %v5\n"
              "    %v7 = iadd %v3, %v4\n"
              "    %v8 = iadd %v7, %v6\n"
              "    return %v8\n"
              "}\n" );
}

TEST_CASE( "printer writes every icmp condition", "[ir][print]" )
{
    const std::pair< ir::ICmpCC, std::string_view > conds[] = {
        { ir::ICmpCC::eq,  "eq"},
        { ir::ICmpCC::ne,  "ne"},
        {ir::ICmpCC::slt, "slt"},
        {ir::ICmpCC::sle, "sle"},
        {ir::ICmpCC::sgt, "sgt"},
        {ir::ICmpCC::sge, "sge"},
        {ir::ICmpCC::ult, "ult"},
        {ir::ICmpCC::ule, "ule"},
        {ir::ICmpCC::ugt, "ugt"},
        {ir::ICmpCC::uge, "uge"},
    };
    for ( const auto& [ cc, name ] : conds )
        CHECK(
            print_one( Type::i8, { Type::i64, Type::i64 },
                       [ & ]( ir::IRBuilder& b, const Args& a )
                       {
                           b.ret( b.icmp( cc, a[ 0 ], a[ 1 ] ) );
                       } )
            == std::format( "define @f(%v0: i64, %v1: i64) -> i8 {{\nblock0:\n    %v2 = icmp {} %v0, %v1\n    return %v2\n}}\n", name ) );
}

TEST_CASE( "printer inlines constants whose type follows from the other operand", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { Type::i64 },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto c = b.iconst( Type::i64, 5 );
                                     const auto x = b.iadd( a[ 0 ], c );
                                     const auto d = b.iconst( Type::i64, static_cast< uint64_t >( -7 ) );
                                     b.ret( b.imul( d, x ) );
                                 } );
    CHECK( text == "define @f(%v0: i64) -> i64 {\nblock0:\n    %v2 = iadd %v0, 5\n    %v4 = imul -7, %v2\n    return %v4\n}\n" );
}

TEST_CASE( "printer keeps iconst when both operands are constants", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { },
                                 []( ir::IRBuilder& b, const Args& )
                                 {
                                     const auto x = b.iconst( Type::i64, 2 );
                                     const auto y = b.iconst( Type::i64, 3 );
                                     b.ret( b.iadd( x, y ) );
                                 } );
    CHECK(
        text
        == "define @f() -> i64 {\nblock0:\n    %v0 = iconst.i64 2\n    %v1 = iconst.i64 3\n    %v2 = iadd %v0, %v1\n    return %v2\n}\n" );
}

TEST_CASE( "printer keeps iconst when any use cannot infer its type", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { Type::i64 },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto c = b.iconst( Type::i32, 9 );
                                     const auto w = b.uextend( Type::i64, c );
                                     b.ret( b.iadd( a[ 0 ], w ) );
                                 } );
    CHECK( text
           == "define @f(%v0: i64) -> i64 {\nblock0:\n    %v1 = iconst.i32 9\n    %v2 = uextend.i64 %v1\n    %v3 = iadd %v0, %v2\n    "
              "return %v3\n}\n" );
}

TEST_CASE( "printer keeps iconst whose type differs from the inferred one", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { Type::i64 },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     b.ret( b.ishl( a[ 0 ], b.iconst( Type::i8, 3 ) ) );
                                 } );
    CHECK( text == "define @f(%v0: i64) -> i64 {\nblock0:\n    %v1 = iconst.i8 3\n    %v2 = ishl %v0, %v1\n    return %v2\n}\n" );
}

TEST_CASE( "printer keeps constants that have no uses", "[ir][print]" )
{
    const auto text = print_one( Type::unit, { },
                                 []( ir::IRBuilder& b, const Args& )
                                 {
                                     ( void ) b.iconst( Type::i64, 1 );
                                     b.ret( );
                                 } );
    CHECK( text == "define @f() {\nblock0:\n    %v0 = iconst.i64 1\n    return\n}\n" );
}

TEST_CASE( "printer inlines constants returned from the function", "[ir][print]" )
{
    CHECK( print_one( Type::i32, { },
                      []( ir::IRBuilder& b, const Args& )
                      {
                          b.ret( b.iconst( Type::i32, 0 ) );
                      } )
           == "define @f() -> i32 {\nblock0:\n    return 0\n}\n" );
    CHECK( print_one( Type::i64, { },
                      []( ir::IRBuilder& b, const Args& )
                      {
                          b.ret( b.iconst( Type::i32, 0 ) );
                      } )
           == "define @f() -> i64 {\nblock0:\n    %v0 = iconst.i32 0\n    return %v0\n}\n" );
}

TEST_CASE( "printer writes negative and extreme constants as signed", "[ir][print]" )
{
    for ( const auto v : { int64_t { -1 }, std::numeric_limits< int64_t >::min( ), std::numeric_limits< int64_t >::max( ), int64_t { 0 } } )
        CHECK( print_one( Type::i64, { },
                          [ & ]( ir::IRBuilder& b, const Args& )
                          {
                              b.ret( b.iconst( Type::i64, static_cast< uint64_t >( v ) ) );
                          } )
               == std::format( "define @f() -> i64 {{\nblock0:\n    return {}\n}}\n", v ) );
}

TEST_CASE( "printer writes select with an inferred condition", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { Type::i8, Type::i64 },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     b.ret( b.select( a[ 0 ], a[ 1 ], b.iconst( Type::i64, 4 ) ) );
                                 } );
    CHECK( text == "define @f(%v0: i8, %v1: i64) -> i64 {\nblock0:\n    %v3 = select %v0, %v1, 4\n    return %v3\n}\n" );
}

TEST_CASE( "printer writes loads and stores with offsets", "[ir][print]" )
{
    const auto text = print_one( Type::i32, { Type::ptr },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto x = b.load( Type::i32, a[ 0 ], 8 );
                                     const auto y = b.load( Type::i64, a[ 0 ], -16 );
                                     const auto z = b.load( Type::i8, a[ 0 ] );
                                     b.store( y, a[ 0 ], 24 );
                                     b.store( z, a[ 0 ], -1 );
                                     b.store( x, a[ 0 ] );
                                     b.ret( x );
                                 } );
    CHECK( text
           == "define @f(%v0: ptr) -> i32 {\n"
              "block0:\n"
              "    %v1 = load.i32 %v0+8\n"
              "    %v2 = load.i64 %v0-16\n"
              "    %v3 = load.i8 %v0\n"
              "    store %v2, %v0+24\n"
              "    store %v3, %v0-1\n"
              "    store %v1, %v0\n"
              "    return %v1\n"
              "}\n" );
}

TEST_CASE( "printer writes a typed store for inline constants", "[ir][print]" )
{
    const auto text = print_one( Type::unit, { Type::ptr },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     b.store( b.iconst( Type::i32, static_cast< uint64_t >( -5 ) ), a[ 0 ], 4 );
                                     b.store( b.iconst( Type::i8, 7 ), a[ 0 ] );
                                     b.ret( );
                                 } );
    CHECK( text == "define @f(%v0: ptr) {\nblock0:\n    store.i32 -5, %v0+4\n    store.i8 7, %v0\n    return\n}\n" );
}

TEST_CASE( "printer never inlines a constant address", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { },
                                 []( ir::IRBuilder& b, const Args& )
                                 {
                                     b.ret( b.load( Type::i64, b.iconst( Type::ptr, 4096 ) ) );
                                 } );
    CHECK( text == "define @f() -> i64 {\nblock0:\n    %v0 = iconst.ptr 4096\n    %v1 = load.i64 %v0\n    return %v1\n}\n" );
}

TEST_CASE( "printer writes data symbols inline with their offsets", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { },
                                 []( ir::IRBuilder& b, const Args& )
                                 {
                                     const auto g0 = b.create_global_value( {
                                         .name      = { ir::extns::data, 3 },
                                         .offset    = 0,
                                         .colocated = true
                                     } );
                                     const auto g1 = b.create_global_value( {
                                         .name      = { ir::extns::data, 4 },
                                         .offset    = 16,
                                         .colocated = false
                                     } );
                                     const auto g2 = b.create_global_value( {
                                         .name      = { ir::extns::data, 5 },
                                         .offset    = -8,
                                         .colocated = true
                                     } );
                                     const auto a  = b.load( Type::i64, b.symbol_value( Type::ptr, g0 ) );
                                     const auto c  = b.load( Type::i64, b.symbol_value( Type::ptr, g1 ), 8 );
                                     const auto d  = b.load( Type::i64, b.symbol_value( Type::ptr, g2 ) );
                                     b.ret( b.iadd( b.iadd( a, c ), d ) );
                                 } );
    CHECK( text
           == "define @f() -> i64 {\n"
              "block0:\n"
              "    %v1 = load.i64 @data3\n"
              "    %v3 = load.i64 @data4+16+8\n"
              "    %v5 = load.i64 @data5-8\n"
              "    %v6 = iadd %v1, %v3\n"
              "    %v7 = iadd %v6, %v5\n"
              "    return %v7\n"
              "}\n" );
}

TEST_CASE( "printer writes direct calls by name with and without results", "[ir][print]" )
{
    const auto text
        = print_one( Type::i64, { Type::i64 },
                     []( ir::IRBuilder& b, const Args& a )
                     {
                         const auto f = b.import_function( { ir::extns::function, 2 }, { Type::i64, Type::i32 }, Type::i64,
                                                           codegen::host_call_conv( ), true );
                         const auto g = b.import_function( { ir::extns::function, 7 }, { }, Type::unit, codegen::host_call_conv( ), false );
                         const ValueRef args[] = { a[ 0 ], b.iconst( Type::i32, 3 ) };
                         const auto     r      = b.call( f, args );
                         ( void ) b.call( g, { } );
                         b.ret( r );
                     } );
    CHECK( text == "define @f(%v0: i64) -> i64 {\nblock0:\n    %v2 = call @fn2(%v0, 3)\n    call @fn7()\n    return %v2\n}\n" );
}

TEST_CASE( "printer writes function addresses inline", "[ir][print]" )
{
    const auto text
        = print_one( Type::ptr, { },
                     []( ir::IRBuilder& b, const Args& )
                     {
                         const auto f = b.import_function( { ir::extns::function, 9 }, { }, Type::i64, codegen::host_call_conv( ), true );
                         b.ret( b.func_addr( Type::ptr, f ) );
                     } );
    CHECK( text == "define @f() -> ptr {\nblock0:\n    return @fn9\n}\n" );
}

TEST_CASE( "printer writes signatures and indirect calls", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { Type::ptr, Type::i64 },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto s0 = b.import_signature( { Type::i64, Type::i64 }, Type::i64, codegen::host_call_conv( ) );
                                     const auto s1 = b.import_signature( { Type::ptr }, Type::unit, other_cc );
                                     const ValueRef args0[] = { a[ 1 ], b.iconst( Type::i64, 2 ) };
                                     const auto     r       = b.call_indirect( s0, a[ 0 ], args0 );
                                     const ValueRef args1[] = { a[ 0 ] };
                                     ( void ) b.call_indirect( s1, a[ 0 ], args1 );
                                     b.ret( r );
                                 } );
    CHECK( text
           == std::format( "define @f(%v0: ptr, %v1: i64) -> i64 {{\n"
                           "    sig0 = (i64, i64) -> i64\n"
                           "    sig1 = (ptr) {}\n"
                           "\n"
                           "block0:\n"
                           "    %v3 = call_indirect sig0, %v0(%v1, 2)\n"
                           "    call_indirect sig1, %v0(%v0)\n"
                           "    return %v3\n"
                           "}}\n",
                           codegen::to_string( other_cc ) ) );
}

TEST_CASE( "printer writes stack slots and stack_addr", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { },
                                 []( ir::IRBuilder& b, const Args& )
                                 {
                                     const auto s0 = b.create_stack_slot( 16, 8 );
                                     const auto s1 = b.create_stack_slot( 64, 16 );
                                     const auto s2 = b.create_stack_slot( 1, 1 );
                                     const auto p  = b.stack_addr( Type::ptr, s0, 8 );
                                     const auto q  = b.stack_addr( Type::ptr, s1 );
                                     ( void ) b.stack_addr( Type::ptr, s2, 1 );
                                     const auto c = b.iconst( Type::i64, 42 );
                                     const auto v = b.load( Type::i64, q, 8 );
                                     b.store( c, p );
                                     b.ret( v );
                                 } );
    CHECK( text
           == "define @f() -> i64 {\n"
              "    ss0 = explicit_slot 16, align = 8\n"
              "    ss1 = explicit_slot 64, align = 16\n"
              "    ss2 = explicit_slot 1, align = 1\n"
              "\n"
              "block0:\n"
              "    %v0 = stack_addr.ptr ss0+8\n"
              "    %v1 = stack_addr.ptr ss1\n"
              "    %v2 = stack_addr.ptr ss2+1\n"
              "    %v4 = load.i64 %v1+8\n"
              "    store.i64 42, %v0\n"
              "    return %v4\n"
              "}\n" );
}

TEST_CASE( "printer writes signatures then stack slots with one blank line", "[ir][print]" )
{
    const auto text = print_one( Type::ptr, { },
                                 []( ir::IRBuilder& b, const Args& )
                                 {
                                     ( void ) b.import_signature( { Type::i64 }, Type::i64, codegen::host_call_conv( ) );
                                     b.ret( b.stack_addr( Type::ptr, b.create_stack_slot( 8, 8 ) ) );
                                 } );
    CHECK( text
           == "define @f() -> ptr {\n"
              "    sig0 = (i64) -> i64\n"
              "    ss0 = explicit_slot 8, align = 8\n"
              "\n"
              "block0:\n"
              "    %v0 = stack_addr.ptr ss0\n"
              "    return %v0\n"
              "}\n" );
}

TEST_CASE( "printer writes jumps, brif and block params", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { Type::i64 },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto     loop   = b.add_block( { Type::i64, Type::i64 } );
                                     const auto     body   = b.add_block( { } );
                                     const auto     exit   = b.add_block( { Type::i64 } );
                                     const ValueRef init[] = { b.iconst( Type::i64, 0 ), b.iconst( Type::i64, 1 ) };
                                     b.br( loop, init );
                                     b.switch_to_block( loop );
                                     const auto     done  = b.icmp( ir::ICmpCC::sgt, b.block_param( loop, 1 ), a[ 0 ] );
                                     const ValueRef out[] = { b.block_param( loop, 0 ) };
                                     b.brif( done, exit, out, body, { } );
                                     b.switch_to_block( body );
                                     const auto     acc     = b.iadd( b.block_param( loop, 0 ), b.block_param( loop, 1 ) );
                                     const auto     next    = b.iadd( b.block_param( loop, 1 ), b.iconst( Type::i64, 1 ) );
                                     const ValueRef again[] = { acc, next };
                                     b.br( loop, again );
                                     b.switch_to_block( exit );
                                     b.ret( b.block_param( exit, 0 ) );
                                 } );
    CHECK( text
           == "define @f(%v0: i64) -> i64 {\n"
              "block0:\n"
              "    jump block1(0, 1)\n"
              "\n"
              "block1(%v1: i64, %v2: i64):\n"
              "    %v7 = icmp sgt %v2, %v0\n"
              "    brif %v7, block3(%v1), block2\n"
              "\n"
              "block2:\n"
              "    %v9 = iadd %v1, %v2\n"
              "    %v11 = iadd %v2, 1\n"
              "    jump block1(%v9, %v11)\n"
              "\n"
              "block3(%v3: i64):\n"
              "    return %v3\n"
              "}\n" );
}

TEST_CASE( "printer infers block argument constants from the target params", "[ir][print]" )
{
    const auto text = print_one( Type::i64, { Type::i64 },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto     t      = b.add_block( { Type::i32 } );
                                     const auto     e      = b.add_block( { Type::i64 } );
                                     const ValueRef ta[]   = { b.iconst( Type::i32, 1 ) };
                                     const ValueRef ea[]   = { b.iconst( Type::i32, 2 ) };
                                     const ValueRef pass[] = { a[ 0 ] };
                                     b.brif( a[ 0 ], t, ta, e, pass );
                                     b.switch_to_block( t );
                                     const ValueRef wide[] = { b.uextend( Type::i64, b.block_param( t, 0 ) ) };
                                     ( void ) ea;
                                     b.br( e, wide );
                                     b.switch_to_block( e );
                                     b.ret( b.block_param( e, 0 ) );
                                 } );
    CHECK( text.contains( "    %v4 = iconst.i32 2\n" ) );
    CHECK( text.contains( "    brif %v0, block1(1), block2(%v0)\n" ) );
    CHECK( text.contains( "    jump block2(%v6)\n" ) );
}

TEST_CASE( "printer writes brif on a constant condition inline only for i64", "[ir][print]" )
{
    const auto body = []( const Type ty )
    {
        return print_one( Type::i64, { },
                          [ & ]( ir::IRBuilder& b, const Args& )
                          {
                              const auto t = b.add_block( { } );
                              const auto e = b.add_block( { } );
                              b.brif( b.iconst( ty, 1 ), t, { }, e, { } );
                              b.switch_to_block( t );
                              b.ret( b.iconst( Type::i64, 1 ) );
                              b.switch_to_block( e );
                              b.ret( b.iconst( Type::i64, 0 ) );
                          } );
    };
    CHECK( body( Type::i64 ).contains( "    brif 1, block1, block2\n" ) );
    CHECK( body( Type::i8 ).contains( "    %v0 = iconst.i8 1\n    brif %v0, block1, block2\n" ) );
}

TEST_CASE( "printer falls back to u-names without a resolver", "[ir][print]" )
{
    ir::IRModule m;
    const auto   r = m.declare( "g", Type::i64, { } );
    {
        ir::IRBuilder b { *m.get( r ) };
        const auto    f = b.import_function( { ir::extns::function, 4 }, { }, Type::i64, codegen::host_call_conv( ), false );
        b.switch_to_block( b.add_block( { } ) );
        b.ret( b.call( f, { } ) );
        b.finalise( );
    }
    CHECK( print( m ) == "define @g() -> i64 {\nblock0:\n    %v0 = call @u0:4()\n    return %v0\n}\n" );
}

TEST_CASE( "printer separates functions with a blank line", "[ir][print]" )
{
    ir::IRModule m;
    for ( const auto* name : { "a", "b", "c" } )
    {
        const auto    r = m.declare( name, Type::unit, { } );
        ir::IRBuilder b { *m.get( r ) };
        b.switch_to_block( b.add_block( { } ) );
        b.ret( );
        b.finalise( );
    }
    CHECK( print( m )
           == "define @a() {\nblock0:\n    return\n}\n\n"
              "define @b() {\nblock0:\n    return\n}\n\n"
              "define @c() {\nblock0:\n    return\n}\n" );
}

TEST_CASE( "printer handles a declared function with no body", "[ir][print]" )
{
    ir::IRModule m;
    ( void ) m.declare( "empty", Type::i64, { Type::i64, Type::ptr } );
    CHECK( print( m ) == "define @empty(i64, ptr) -> i64 {\n}\n" );
}

TEST_CASE( "printer prints an empty module as nothing", "[ir][print]" )
{
    ir::IRModule m;
    CHECK( print( m ).empty( ) );
}

TEST_CASE( "fmt_function prints a single function", "[ir][print]" )
{
    ir::IRModule m;
    for ( const auto* name : { "first", "second" } )
    {
        const auto    r = m.declare( name, Type::unit, { } );
        ir::IRBuilder b { *m.get( r ) };
        b.switch_to_block( b.add_block( { } ) );
        b.ret( );
        b.finalise( );
    }
    std::string out;
    m.fmt_function( std::back_inserter( out ), *m.functions( )[ 1 ], named );
    CHECK( out == "define @second() {\nblock0:\n    return\n}\n" );
}
