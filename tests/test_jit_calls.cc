#include "support.hh"

#include <array>
#include <cstdint>
#include <vector>

using namespace xt;

namespace
{
    int64_t h_zero( )
    { return 77; }

    int64_t h_one( const int64_t a )
    { return a * 3 + 1; }

    int64_t h_two( const int64_t a, const int64_t b )
    { return a - 2 * b; }

    int64_t h_three( const int64_t a, const int64_t b, const int64_t c )
    { return a * 100 + b * 10 + c; }

    int64_t h_four( const int64_t a, const int64_t b, const int64_t c, const int64_t d )
    { return a * 1000 + b * 100 + c * 10 + d; }

    int64_t h_mixed( const int8_t a, const int16_t b, const int32_t c, const int64_t d )
    { return int64_t { a } * 7 + int64_t { b } * 11 + int64_t { c } * 13 + d; }

    int8_t h_ret8( const int64_t x )
    { return static_cast< int8_t >( x ); }

    uint8_t h_retu8( const int64_t x )
    { return static_cast< uint8_t >( x ); }

    int16_t h_ret16( const int64_t x )
    { return static_cast< int16_t >( x ); }

    uint16_t h_retu16( const int64_t x )
    { return static_cast< uint16_t >( x ); }

    int32_t h_ret32( const int64_t x )
    { return static_cast< int32_t >( x ); }

    uint32_t h_retu32( const int64_t x )
    { return static_cast< uint32_t >( x ); }

    void h_accumulate( int64_t* const p, const int64_t v )
    { *p += v; }

    int g_ticks = 0;

    void h_tick( )
    { ++g_ticks; }

    int64_t h_apply( int64_t ( *f )( int64_t ), const int64_t x )
    { return f( x ) + f( x + 1 ); }

    int64_t h_ptr_diff( const char* const a, const char* const b )
    { return a - b; }

    jit::JITBuilder host_builder( )
    {
        jit::JITBuilder b { isa::lookup_host( ) };
        b.symbol( "h_zero", reinterpret_cast< const void* >( &h_zero ) );
        b.symbol( "h_one", reinterpret_cast< const void* >( &h_one ) );
        b.symbol( "h_two", reinterpret_cast< const void* >( &h_two ) );
        b.symbol( "h_three", reinterpret_cast< const void* >( &h_three ) );
        b.symbol( "h_four", reinterpret_cast< const void* >( &h_four ) );
        b.symbol( "h_mixed", reinterpret_cast< const void* >( &h_mixed ) );
        b.symbol( "h_ret8", reinterpret_cast< const void* >( &h_ret8 ) );
        b.symbol( "h_retu8", reinterpret_cast< const void* >( &h_retu8 ) );
        b.symbol( "h_ret16", reinterpret_cast< const void* >( &h_ret16 ) );
        b.symbol( "h_retu16", reinterpret_cast< const void* >( &h_retu16 ) );
        b.symbol( "h_ret32", reinterpret_cast< const void* >( &h_ret32 ) );
        b.symbol( "h_retu32", reinterpret_cast< const void* >( &h_retu32 ) );
        b.symbol( "h_accumulate", reinterpret_cast< const void* >( &h_accumulate ) );
        b.symbol( "h_tick", reinterpret_cast< const void* >( &h_tick ) );
        b.symbol( "h_apply", reinterpret_cast< const void* >( &h_apply ) );
        b.symbol( "h_ptr_diff", reinterpret_cast< const void* >( &h_ptr_diff ) );
        return b;
    }

    const std::vector< int64_t > small_values { 0, 1, -1, 2, -3, 9, 100, -1000, 123456, -987654321, 0x7fffffff, -0x80000000LL };
} // namespace

TEST_CASE( "call a host function with no arguments", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_zero", Type::i64, { } );
    const auto id     = j.define( "f", Type::i64, { },
                                  [ & ]( ir::IRBuilder& b, const Args& )
                                  {
                                  const auto fr = j.mod.declare_func_in_func( callee, b );
                                  b.ret( b.call( fr, { } ) );
                              } );
    j.finalise( );
    CHECK( j.get< int64_t( ) >( id )( ) == 77 );
}

TEST_CASE( "call a host function with one argument", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_one", Type::i64, { Type::i64 } );
    const auto id     = j.define( "f", Type::i64, { Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                  const auto fr = j.mod.declare_func_in_func( callee, b );
                                  b.ret( b.call( fr, Args { a[ 0 ] } ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( id );
    for ( const auto x : small_values ) CHECK( f( x ) == h_one( x ) );
}

TEST_CASE( "call a host function with two arguments in both orders", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_two", Type::i64, { Type::i64, Type::i64 } );
    const auto fwd    = j.define( "fwd", Type::i64, { Type::i64, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                   const auto fr = j.mod.declare_func_in_func( callee, b );
                                   b.ret( b.call( fr, Args { a[ 0 ], a[ 1 ] } ) );
                               } );
    const auto rev    = j.define( "rev", Type::i64, { Type::i64, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                   const auto fr = j.mod.declare_func_in_func( callee, b );
                                   b.ret( b.call( fr, Args { a[ 1 ], a[ 0 ] } ) );
                               } );
    j.finalise( );
    for ( const auto x : small_values )
        for ( const auto y : { int64_t { 0 }, int64_t { 5 }, int64_t { -11 } } )
        {
            CHECK( j.get< int64_t( int64_t, int64_t ) >( fwd )( x, y ) == h_two( x, y ) );
            CHECK( j.get< int64_t( int64_t, int64_t ) >( rev )( x, y ) == h_two( y, x ) );
        }
}

TEST_CASE( "call a host function with three arguments", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_three", Type::i64, { Type::i64, Type::i64, Type::i64 } );
    const auto id     = j.define( "f", Type::i64, { Type::i64, Type::i64, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                  const auto fr = j.mod.declare_func_in_func( callee, b );
                                  b.ret( b.call( fr, Args { a[ 2 ], a[ 0 ], a[ 1 ] } ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t, int64_t ) >( id );
    CHECK( f( 1, 2, 3 ) == h_three( 3, 1, 2 ) );
    CHECK( f( -4, 0, 9 ) == h_three( 9, -4, 0 ) );
    CHECK( f( 0, 0, 0 ) == 0 );
}

TEST_CASE( "call a host function with four arguments in every rotation", "[jit][calls]" )
{
    Jit                                     j { host_builder( ) };
    const auto                              callee = j.import( "h_four", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 } );
    std::array< module::FuncId, 4 >         ids { };
    const std::array< std::string_view, 4 > names { "r0", "r1", "r2", "r3" };
    for ( int r = 0; r < 4; ++r )
        ids[ r ] = j.define( names[ r ], Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 },
                             [ & ]( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto fr = j.mod.declare_func_in_func( callee, b );
                                 b.ret( b.call( fr, Args { a[ r % 4 ], a[ ( r + 1 ) % 4 ], a[ ( r + 2 ) % 4 ], a[ ( r + 3 ) % 4 ] } ) );
                             } );
    j.finalise( );
    const int64_t in[ 4 ] = { 1, 2, 3, 4 };
    for ( int r = 0; r < 4; ++r )
        CHECK( j.get< int64_t( int64_t, int64_t, int64_t, int64_t ) >( ids[ r ] )( 1, 2, 3, 4 )
               == h_four( in[ r % 4 ], in[ ( r + 1 ) % 4 ], in[ ( r + 2 ) % 4 ], in[ ( r + 3 ) % 4 ] ) );
}

TEST_CASE( "call a host function with four reversed arguments", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_four", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 } );
    const auto id     = j.define( "f", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                  const auto fr = j.mod.declare_func_in_func( callee, b );
                                  b.ret( b.call( fr, Args { a[ 3 ], a[ 2 ], a[ 1 ], a[ 0 ] } ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t, int64_t, int64_t ) >( id );
    CHECK( f( 1, 2, 3, 4 ) == 4321 );
    CHECK( f( 9, 8, 7, 6 ) == 6789 );
    CHECK( f( -1, 0, 0, 0 ) == -1 );
}

TEST_CASE( "call a host function with duplicated arguments", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_four", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 } );
    const auto same   = j.define( "same", Type::i64, { Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                    const auto fr = j.mod.declare_func_in_func( callee, b );
                                    b.ret( b.call( fr, Args { a[ 0 ], a[ 0 ], a[ 0 ], a[ 0 ] } ) );
                                } );
    const auto pairs  = j.define( "pairs", Type::i64, { Type::i64, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                     const auto fr = j.mod.declare_func_in_func( callee, b );
                                     b.ret( b.call( fr, Args { a[ 1 ], a[ 1 ], a[ 0 ], a[ 0 ] } ) );
                                 } );
    j.finalise( );
    CHECK( j.get< int64_t( int64_t ) >( same )( 3 ) == 3333 );
    CHECK( j.get< int64_t( int64_t ) >( same )( -2 ) == -2222 );
    CHECK( j.get< int64_t( int64_t, int64_t ) >( pairs )( 1, 2 ) == 2211 );
}

TEST_CASE( "call a host function with constant arguments", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_four", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 } );
    const auto id     = j.define( "f", Type::i64, { },
                                  [ & ]( ir::IRBuilder& b, const Args& )
                                  {
                                  const auto fr = j.mod.declare_func_in_func( callee, b );
                                  b.ret( b.call( fr, Args { b.iconst( Type::i64, 5 ), b.iconst( Type::i64, 0 ), b.iconst( Type::i64, 7 ),
                                                            b.iconst( Type::i64, static_cast< uint64_t >( -1 ) ) } ) );
                              } );
    j.finalise( );
    CHECK( j.get< int64_t( ) >( id )( ) == h_four( 5, 0, 7, -1 ) );
}

TEST_CASE( "call a host function taking each integer width", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_mixed", Type::i64, { Type::i8, Type::i16, Type::i32, Type::i64 } );
    const auto id     = j.define( "f", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                  const auto fr = j.mod.declare_func_in_func( callee, b );
                                  b.ret( b.call( fr, Args { b.ireduce( Type::i8, a[ 0 ] ), b.ireduce( Type::i16, a[ 1 ] ),
                                                            b.ireduce( Type::i32, a[ 2 ] ), a[ 3 ] } ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t, int64_t, int64_t ) >( id );
    for ( const auto x : edge_values( ) )
        CHECK( f( x, x, x, x ) == h_mixed( static_cast< int8_t >( x ), static_cast< int16_t >( x ), static_cast< int32_t >( x ), x ) );
    CHECK( f( 300, 70000, 0x100000005LL, 1 ) == h_mixed( 44, 4464, 5, 1 ) );
}

TEST_CASE( "call a jit function taking narrow parameters directly", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_mixed", Type::i64, { Type::i8, Type::i16, Type::i32, Type::i64 } );
    const auto id     = j.define( "f", Type::i64, { Type::i8, Type::i16, Type::i32, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                  const auto fr = j.mod.declare_func_in_func( callee, b );
                                  b.ret( b.call( fr, a ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int8_t, int16_t, int32_t, int64_t ) >( id );
    CHECK( f( -1, -1, -1, -1 ) == h_mixed( -1, -1, -1, -1 ) );
    CHECK( f( 127, 32767, 0x7fffffff, 0 ) == h_mixed( 127, 32767, 0x7fffffff, 0 ) );
    CHECK( f( -128, -32768, -0x7fffffff - 1, 5 ) == h_mixed( -128, -32768, -0x7fffffff - 1, 5 ) );
}

TEST_CASE( "narrow host return values extend correctly", "[jit][calls]" )
{
    struct Case
    {
        std::string_view name;
        Type             ty;
        bool             is_signed;
    };

    const Case cases[] = {
        {  "h_ret8",  Type::i8,  true},
        { "h_retu8",  Type::i8, false},
        { "h_ret16", Type::i16,  true},
        {"h_retu16", Type::i16, false},
        { "h_ret32", Type::i32,  true},
        {"h_retu32", Type::i32, false},
    };

    for ( const auto& c : cases )
    {
        DYNAMIC_SECTION( c.name )
        {
            Jit        j { host_builder( ) };
            const auto callee = j.import( c.name, c.ty, { Type::i64 } );
            const auto id     = j.define( "f", Type::i64, { Type::i64 },
                                          [ & ]( ir::IRBuilder& b, const Args& a )
                                          {
                                          const auto fr = j.mod.declare_func_in_func( callee, b );
                                          const auto r  = b.call( fr, Args { a[ 0 ] } );
                                          b.ret( c.is_signed ? b.sextend( Type::i64, r ) : b.uextend( Type::i64, r ) );
                                      } );
            j.finalise( );
            const auto f = j.get< int64_t( int64_t ) >( id );
            for ( const auto x : edge_values( ) )
            {
                const auto raw  = static_cast< uint64_t >( x );
                const auto want = static_cast< int64_t >( c.is_signed ? sext( c.ty, raw ) : mask( c.ty, raw ) );
                CHECK( f( x ) == want );
            }
        }
    }
}

TEST_CASE( "narrow host return value passed straight back to the caller", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto r8  = j.import( "h_ret8", Type::i8, { Type::i64 } );
    const auto r16 = j.import( "h_ret16", Type::i16, { Type::i64 } );
    const auto r32 = j.import( "h_ret32", Type::i32, { Type::i64 } );
    const auto f8  = j.define( "f8", Type::i8, { Type::i64 },
                               [ & ]( ir::IRBuilder& b, const Args& a )
                               {
                                  b.ret( b.call( j.mod.declare_func_in_func( r8, b ), Args { a[ 0 ] } ) );
                              } );
    const auto f16 = j.define( "f16", Type::i16, { Type::i64 },
                               [ & ]( ir::IRBuilder& b, const Args& a )
                               {
                                   b.ret( b.call( j.mod.declare_func_in_func( r16, b ), Args { a[ 0 ] } ) );
                               } );
    const auto f32 = j.define( "f32", Type::i32, { Type::i64 },
                               [ & ]( ir::IRBuilder& b, const Args& a )
                               {
                                   b.ret( b.call( j.mod.declare_func_in_func( r32, b ), Args { a[ 0 ] } ) );
                               } );
    j.finalise( );
    for ( const auto x : edge_values( ) )
    {
        CHECK( j.get< int8_t( int64_t ) >( f8 )( x ) == static_cast< int8_t >( x ) );
        CHECK( j.get< int16_t( int64_t ) >( f16 )( x ) == static_cast< int16_t >( x ) );
        CHECK( j.get< int32_t( int64_t ) >( f32 )( x ) == static_cast< int32_t >( x ) );
    }
}

TEST_CASE( "call a void host function through a pointer argument", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_accumulate", Type::unit, { Type::ptr, Type::i64 } );
    const auto id     = j.define( "f", Type::unit, { Type::ptr, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                  const auto fr = j.mod.declare_func_in_func( callee, b );
                                  ( void ) b.call( fr, Args { a[ 0 ], a[ 1 ] } );
                                  ( void ) b.call( fr, Args { a[ 0 ], b.iadd( a[ 1 ], a[ 1 ] ) } );
                                  b.ret( );
                              } );
    j.finalise( );
    int64_t    acc = 10;
    const auto f   = j.get< void( int64_t*, int64_t ) >( id );
    f( &acc, 5 );
    CHECK( acc == 25 );
    f( &acc, -25 );
    CHECK( acc == -50 );
}

TEST_CASE( "call a void host function with no arguments repeatedly", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_tick", Type::unit, { } );
    const auto id     = j.define( "f", Type::i64, { Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                  const auto fr = j.mod.declare_func_in_func( callee, b );
                                  for ( int k = 0; k < 5; ++k ) ( void ) b.call( fr, { } );
                                  b.ret( a[ 0 ] );
                              } );
    j.finalise( );
    g_ticks = 0;
    CHECK( j.get< int64_t( int64_t ) >( id )( 1234 ) == 1234 );
    CHECK( g_ticks == 5 );
}

TEST_CASE( "nested host calls feed results into each other", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto one = j.import( "h_one", Type::i64, { Type::i64 } );
    const auto two = j.import( "h_two", Type::i64, { Type::i64, Type::i64 } );
    const auto id  = j.define( "f", Type::i64, { Type::i64, Type::i64 },
                               [ & ]( ir::IRBuilder& b, const Args& a )
                               {
                                  const auto f1 = j.mod.declare_func_in_func( one, b );
                                  const auto f2 = j.mod.declare_func_in_func( two, b );
                                  const auto x  = b.call( f2, Args { b.call( f1, Args { a[ 0 ] } ), a[ 0 ] } );
                                  const auto y  = b.call( f2, Args { a[ 1 ], a[ 0 ] } );
                                  const auto z  = b.call( f2, Args { x, y } );
                                  b.ret( b.iadd( b.call( f1, Args { z } ), b.imul( a[ 0 ], a[ 1 ] ) ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t ) >( id );
    for ( const auto x : small_values )
        for ( const auto y : { int64_t { 0 }, int64_t { 3 }, int64_t { -8 } } )
            CHECK( f( x, y ) == h_one( h_two( h_two( h_one( x ), x ), h_two( y, x ) ) ) + x * y );
}

TEST_CASE( "many values stay live across a single host call", "[jit][calls]" )
{
    constexpr int n = 16;
    Jit           j { host_builder( ) };
    const auto    one = j.import( "h_one", Type::i64, { Type::i64 } );
    const auto    id  = j.define( "f", Type::i64, { Type::i64, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                  const auto              fr = j.mod.declare_func_in_func( one, b );
                                  std::vector< ValueRef > vs;
                                  for ( int k = 0; k < n; ++k )
                                      vs.push_back( b.iadd( b.imul( a[ 0 ], b.iconst( Type::i64, k + 2 ) ), a[ 1 ] ) );
                                  auto acc = b.call( fr, Args { a[ 1 ] } );
                                  for ( int k = 0; k < n; ++k ) acc = b.iadd( b.imul( acc, b.iconst( Type::i64, 3 ) ), vs[ k ] );
                                  b.ret( b.iadd( acc, b.isub( a[ 0 ], a[ 1 ] ) ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t ) >( id );
    for ( const auto x : small_values )
        for ( const auto y : { int64_t { 1 }, int64_t { -77 }, int64_t { 1 } << 40 } )
        {
            const auto ux  = static_cast< uint64_t >( x );
            const auto uy  = static_cast< uint64_t >( y );
            uint64_t   acc = static_cast< uint64_t >( h_one( y ) );
            for ( int k = 0; k < n; ++k ) acc = acc * 3 + ( ux * static_cast< uint64_t >( k + 2 ) + uy );
            acc += ux - uy;
            CHECK( static_cast< uint64_t >( f( x, y ) ) == acc );
        }
}

TEST_CASE( "values defined between several host calls survive all of them", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto one = j.import( "h_one", Type::i64, { Type::i64 } );
    const auto two = j.import( "h_two", Type::i64, { Type::i64, Type::i64 } );
    const auto id  = j.define( "f", Type::i64, { Type::i64, Type::i64, Type::i64 },
                               [ & ]( ir::IRBuilder& b, const Args& a )
                               {
                                  const auto f1 = j.mod.declare_func_in_func( one, b );
                                  const auto f2 = j.mod.declare_func_in_func( two, b );
                                  const auto p  = b.iadd( a[ 0 ], a[ 1 ] );
                                  const auto q  = b.isub( a[ 1 ], a[ 2 ] );
                                  const auto c1 = b.call( f1, Args { p } );
                                  const auto r  = b.bxor( c1, q );
                                  const auto s  = b.imul( a[ 2 ], b.iconst( Type::i64, 9 ) );
                                  const auto c2 = b.call( f2, Args { r, s } );
                                  const auto t  = b.band( c2, b.iconst( Type::i64, 0xffff ) );
                                  const auto c3 = b.call( f1, Args { t } );
                                  const auto u  = b.iadd( b.iadd( p, q ), b.iadd( r, s ) );
                                  const auto v  = b.iadd( b.iadd( c1, c2 ), b.iadd( c3, t ) );
                                  b.ret( b.iadd( b.iadd( u, v ), b.iadd( a[ 0 ], b.iadd( a[ 1 ], a[ 2 ] ) ) ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t, int64_t ) >( id );
    for ( const auto x : small_values )
    {
        const int64_t y  = x ^ 0x55;
        const int64_t z  = -x / 3;
        const auto    p  = x + y;
        const auto    q  = y - z;
        const auto    c1 = h_one( p );
        const auto    r  = c1 ^ q;
        const auto    s  = z * 9;
        const auto    c2 = h_two( r, s );
        const auto    t  = c2 & 0xffff;
        const auto    c3 = h_one( t );
        CHECK( f( x, y, z ) == ( p + q ) + ( r + s ) + ( c1 + c2 ) + ( c3 + t ) + ( x + ( y + z ) ) );
    }
}

TEST_CASE( "a long chain of sequential host calls", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto two = j.import( "h_two", Type::i64, { Type::i64, Type::i64 } );
    const auto id  = j.define( "f", Type::i64, { Type::i64, Type::i64 },
                               [ & ]( ir::IRBuilder& b, const Args& a )
                               {
                                  const auto fr  = j.mod.declare_func_in_func( two, b );
                                  auto       acc = a[ 0 ];
                                  for ( int k = 0; k < 24; ++k )
                                      acc = b.call( fr, Args { acc, k % 2 ? a[ 1 ] : b.iconst( Type::i64, k ) } );
                                  b.ret( acc );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t ) >( id );
    for ( const auto x : small_values )
    {
        int64_t acc = x;
        for ( int k = 0; k < 24; ++k ) acc = h_two( acc, k % 2 ? 6 : k );
        CHECK( f( x, 6 ) == acc );
    }
}

TEST_CASE( "host call inside a counted loop", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto one = j.import( "h_one", Type::i64, { Type::i64 } );
    const auto id  = j.define( "f", Type::i64, { Type::i64 },
                               [ & ]( ir::IRBuilder& b, const Args& a )
                               {
                                  const auto fr   = j.mod.declare_func_in_func( one, b );
                                  const auto loop = b.add_block( { Type::i64, Type::i64 } );
                                  const auto body = b.add_block( { } );
                                  const auto exit = b.add_block( { } );
                                  b.br( loop, Args { b.iconst( Type::i64, 0 ), b.iconst( Type::i64, 0 ) } );
                                  b.switch_to_block( loop );
                                  const auto i   = b.block_param( loop, 0 );
                                  const auto acc = b.block_param( loop, 1 );
                                  b.brif( b.icmp( ir::ICmpCC::slt, i, a[ 0 ] ), body, { }, exit, { } );
                                  b.switch_to_block( body );
                                  const auto v = b.call( fr, Args { i } );
                                  b.br( loop, Args { b.iadd( i, b.iconst( Type::i64, 1 ) ), b.iadd( acc, v ) } );
                                  b.switch_to_block( exit );
                                  b.ret( acc );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( id );
    for ( const int64_t n : { 0, 1, 2, 5, 17, 100, 1000 } )
    {
        int64_t want = 0;
        for ( int64_t i = 0; i < n; ++i ) want += h_one( i );
        CHECK( f( n ) == want );
    }
    CHECK( f( -5 ) == 0 );
}

TEST_CASE( "loop with many loop carried values and a host call per iteration", "[jit][calls]" )
{
    constexpr int k_vals = 8;
    Jit           j { host_builder( ) };
    const auto    two = j.import( "h_two", Type::i64, { Type::i64, Type::i64 } );
    const auto    id  = j.define(
        "f", Type::i64, { Type::i64, Type::i64 },
        [ & ]( ir::IRBuilder& b, const Args& a )
        {
            const auto          fr = j.mod.declare_func_in_func( two, b );
            std::vector< Type > tys( k_vals + 1, Type::i64 );
            const auto          loop = b.add_block( std::span< const Type > { tys } );
            const auto          body = b.add_block( { } );
            const auto          exit = b.add_block( { } );
            Args                init { b.iconst( Type::i64, 0 ) };
            for ( int k = 0; k < k_vals; ++k ) init.push_back( b.iadd( a[ 1 ], b.iconst( Type::i64, k ) ) );
            b.br( loop, init );
            b.switch_to_block( loop );
            const auto i = b.block_param( loop, 0 );
            b.brif( b.icmp( ir::ICmpCC::slt, i, a[ 0 ] ), body, { }, exit, { } );
            b.switch_to_block( body );
            const auto c = b.call( fr, Args { i, a[ 1 ] } );
            Args       next { b.iadd( i, b.iconst( Type::i64, 1 ) ) };
            for ( int k = 0; k < k_vals; ++k )
                next.push_back( b.iadd( b.block_param( loop, static_cast< uint16_t >( ( k + 1 ) % k_vals + 1 ) ), c ) );
            b.br( loop, next );
            b.switch_to_block( exit );
            auto acc = b.block_param( loop, 1 );
            for ( int k = 1; k < k_vals; ++k )
                acc = b.iadd( b.imul( acc, b.iconst( Type::i64, 31 ) ), b.block_param( loop, static_cast< uint16_t >( k + 1 ) ) );
            b.ret( acc );
        } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t ) >( id );
    for ( const int64_t n : { 0, 1, 3, 10, 33 } )
        for ( const int64_t y : { 0, 4, -9 } )
        {
            std::array< uint64_t, k_vals > v { };
            for ( int k = 0; k < k_vals; ++k ) v[ k ] = static_cast< uint64_t >( y + k );
            for ( int64_t i = 0; i < n; ++i )
            {
                const auto                     c = static_cast< uint64_t >( h_two( i, y ) );
                std::array< uint64_t, k_vals > nv { };
                for ( int k = 0; k < k_vals; ++k ) nv[ k ] = v[ ( k + 1 ) % k_vals ] + c;
                v = nv;
            }
            uint64_t acc = v[ 0 ];
            for ( int k = 1; k < k_vals; ++k ) acc = acc * 31 + v[ k ];
            CHECK( static_cast< uint64_t >( f( n, y ) ) == acc );
        }
}

TEST_CASE( "recursive factorial through a colocated self call", "[jit][calls]" )
{
    Jit        j;
    const auto fact = j.declare( "fact", Type::i64, { Type::i64 } );
    j.build( fact,
             [ & ]( ir::IRBuilder& b, const Args& a )
             {
                 const auto self = j.mod.declare_func_in_func( fact, b );
                 const auto base = b.add_block( { } );
                 const auto rec  = b.add_block( { } );
                 b.brif( b.icmp( ir::ICmpCC::sle, a[ 0 ], b.iconst( Type::i64, 1 ) ), base, { }, rec, { } );
                 b.switch_to_block( base );
                 b.ret( b.iconst( Type::i64, 1 ) );
                 b.switch_to_block( rec );
                 const auto r = b.call( self, Args { b.isub( a[ 0 ], b.iconst( Type::i64, 1 ) ) } );
                 b.ret( b.imul( a[ 0 ], r ) );
             } );
    j.finalise( );
    const auto f    = j.get< int64_t( int64_t ) >( fact );
    int64_t    want = 1;
    for ( int64_t n = 1; n <= 20; ++n )
    {
        want *= n;
        CHECK( f( n ) == want );
    }
    CHECK( f( 0 ) == 1 );
    CHECK( f( -3 ) == 1 );
}

TEST_CASE( "recursive fibonacci with two self calls", "[jit][calls]" )
{
    Jit        j;
    const auto fib = j.declare( "fib", Type::i64, { Type::i64 } );
    j.build( fib,
             [ & ]( ir::IRBuilder& b, const Args& a )
             {
                 const auto self = j.mod.declare_func_in_func( fib, b );
                 const auto base = b.add_block( { } );
                 const auto rec  = b.add_block( { } );
                 b.brif( b.icmp( ir::ICmpCC::slt, a[ 0 ], b.iconst( Type::i64, 2 ) ), base, { }, rec, { } );
                 b.switch_to_block( base );
                 b.ret( a[ 0 ] );
                 b.switch_to_block( rec );
                 const auto x = b.call( self, Args { b.isub( a[ 0 ], b.iconst( Type::i64, 1 ) ) } );
                 const auto y = b.call( self, Args { b.isub( a[ 0 ], b.iconst( Type::i64, 2 ) ) } );
                 b.ret( b.iadd( x, y ) );
             } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( fib );
    int64_t    p = 0, q = 1;
    for ( int64_t n = 0; n <= 22; ++n )
    {
        CHECK( f( n ) == p );
        const auto t = p + q;
        p            = q;
        q            = t;
    }
}

TEST_CASE( "deep linear recursion", "[jit][calls]" )
{
    Jit        j;
    const auto sum = j.declare( "sum", Type::i64, { Type::i64 } );
    j.build( sum,
             [ & ]( ir::IRBuilder& b, const Args& a )
             {
                 const auto self = j.mod.declare_func_in_func( sum, b );
                 const auto base = b.add_block( { } );
                 const auto rec  = b.add_block( { } );
                 b.brif( b.icmp( ir::ICmpCC::eq, a[ 0 ], b.iconst( Type::i64, 0 ) ), base, { }, rec, { } );
                 b.switch_to_block( base );
                 b.ret( b.iconst( Type::i64, 0 ) );
                 b.switch_to_block( rec );
                 b.ret( b.iadd( a[ 0 ], b.call( self, Args { b.isub( a[ 0 ], b.iconst( Type::i64, 1 ) ) } ) ) );
             } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( sum );
    CHECK( f( 0 ) == 0 );
    CHECK( f( 1 ) == 1 );
    CHECK( f( 100 ) == 5050 );
    CHECK( f( 5000 ) == 5000 * 5001 / 2 );
}

TEST_CASE( "mutual recursion between two jit functions", "[jit][calls]" )
{
    Jit        j;
    const auto even = j.declare( "is_even", Type::i64, { Type::i64 } );
    const auto odd  = j.declare( "is_odd", Type::i64, { Type::i64 } );
    const auto make = [ & ]( const module::FuncId self, const module::FuncId other, const uint64_t at_zero )
    {
        j.build( self,
                 [ & ]( ir::IRBuilder& b, const Args& a )
                 {
                     const auto o    = j.mod.declare_func_in_func( other, b );
                     const auto base = b.add_block( { } );
                     const auto rec  = b.add_block( { } );
                     b.brif( b.icmp( ir::ICmpCC::eq, a[ 0 ], b.iconst( Type::i64, 0 ) ), base, { }, rec, { } );
                     b.switch_to_block( base );
                     b.ret( b.iconst( Type::i64, at_zero ) );
                     b.switch_to_block( rec );
                     b.ret( b.call( o, Args { b.isub( a[ 0 ], b.iconst( Type::i64, 1 ) ) } ) );
                 } );
    };
    make( even, odd, 1 );
    make( odd, even, 0 );
    j.finalise( );
    const auto fe = j.get< int64_t( int64_t ) >( even );
    const auto fo = j.get< int64_t( int64_t ) >( odd );
    for ( int64_t n = 0; n < 50; ++n )
    {
        CHECK( fe( n ) == ( n % 2 == 0 ) );
        CHECK( fo( n ) == ( n % 2 == 1 ) );
    }
    CHECK( fe( 1001 ) == 0 );
    CHECK( fo( 1001 ) == 1 );
}

TEST_CASE( "caller defined before its jit callee", "[jit][calls]" )
{
    Jit        j;
    const auto callee = j.declare( "callee", Type::i64, { Type::i64, Type::i64 }, module::Linkage::Local );
    const auto caller = j.define( "caller", Type::i64, { Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                      const auto fr = j.mod.declare_func_in_func( callee, b );
                                      b.ret( b.iadd( b.call( fr, Args { a[ 0 ], b.iconst( Type::i64, 10 ) } ), b.iconst( Type::i64, 1 ) ) );
                                  } );
    j.build( callee,
             [ & ]( ir::IRBuilder& b, const Args& a )
             {
                 b.ret( b.isub( b.imul( a[ 0 ], a[ 0 ] ), a[ 1 ] ) );
             } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( caller );
    CHECK( f( 0 ) == -9 );
    CHECK( f( 5 ) == 16 );
    CHECK( f( -7 ) == 40 );
}

TEST_CASE( "caller defined after its jit callee", "[jit][calls]" )
{
    Jit        j;
    const auto callee = j.define( "callee", Type::i64, { Type::i64, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                      b.ret( b.isub( a[ 1 ], a[ 0 ] ) );
                                  } );
    const auto caller = j.define( "caller", Type::i64, { Type::i64, Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                      const auto fr = j.mod.declare_func_in_func( callee, b );
                                      b.ret( b.call( fr, Args { a[ 1 ], a[ 0 ] } ) );
                                  } );
    j.finalise( );
    CHECK( j.get< int64_t( int64_t, int64_t ) >( caller )( 10, 3 ) == 7 );
    CHECK( j.get< int64_t( int64_t, int64_t ) >( callee )( 10, 3 ) == -7 );
}

TEST_CASE( "jit to jit call with narrow parameter and return", "[jit][calls]" )
{
    Jit        j;
    const auto callee = j.define( "triple32", Type::i32, { Type::i32 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                      b.ret( b.imul( a[ 0 ], b.iconst( Type::i32, 3 ) ) );
                                  } );
    const auto caller = j.define( "caller", Type::i64, { Type::i64 },
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                      const auto fr = j.mod.declare_func_in_func( callee, b );
                                      b.ret( b.sextend( Type::i64, b.call( fr, Args { b.ireduce( Type::i32, a[ 0 ] ) } ) ) );
                                  } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( caller );
    for ( const auto x : edge_values( ) )
    {
        const auto want = static_cast< int32_t >( static_cast< uint32_t >( x ) * 3u );
        CHECK( f( x ) == want );
    }
}

TEST_CASE( "chain of jit functions each calling the next", "[jit][calls]" )
{
    constexpr int                 depth = 12;
    Jit                           j;
    std::vector< module::FuncId > ids;
    for ( int k = 0; k < depth; ++k ) ids.push_back( j.declare( std::format( "link{}", k ), Type::i64, { Type::i64 } ) );
    for ( int k = 0; k < depth; ++k )
        j.build( ids[ k ],
                 [ & ]( ir::IRBuilder& b, const Args& a )
                 {
                     const auto bumped = b.iadd( b.imul( a[ 0 ], b.iconst( Type::i64, 2 ) ), b.iconst( Type::i64, k ) );
                     if ( k + 1 == depth ) b.ret( bumped );
                     else
                         b.ret( b.call( j.mod.declare_func_in_func( ids[ k + 1 ], b ), Args { bumped } ) );
                 } );
    j.finalise( );
    for ( const int64_t x : { 0, 1, -1, 1000 } )
    {
        int64_t want = x;
        for ( int k = 0; k < depth; ++k ) want = want * 2 + k;
        CHECK( j.get< int64_t( int64_t ) >( ids[ 0 ] )( x ) == want );
        CHECK( j.get< int64_t( int64_t ) >( ids[ depth - 1 ] )( x ) == x * 2 + depth - 1 );
    }
}

TEST_CASE( "func_addr of a local function then call_indirect", "[jit][calls]" )
{
    Jit        j;
    const auto sq = j.define(
        "sq", Type::i64, { Type::i64 },
        [ & ]( ir::IRBuilder& b, const Args& a )
        {
            b.ret( b.imul( a[ 0 ], a[ 0 ] ) );
        },
        module::Linkage::Local );
    const auto id = j.define( "f", Type::i64, { Type::i64 },
                              [ & ]( ir::IRBuilder& b, const Args& a )
                              {
                                  const auto fr  = j.mod.declare_func_in_func( sq, b );
                                  const auto sig = b.import_signature( { Type::i64 }, Type::i64, codegen::host_call_conv( ) );
                                  const auto p   = b.func_addr( Type::ptr, fr );
                                  b.ret( b.iadd( b.call_indirect( sig, p, Args { a[ 0 ] } ), b.iconst( Type::i64, 1 ) ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( id );
    for ( const auto x : small_values ) CHECK( f( x ) == x * x + 1 );
}

TEST_CASE( "func_addr of an imported function then call_indirect", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto four = j.import( "h_four", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 } );
    const auto id   = j.define( "f", Type::i64, { Type::i64, Type::i64 },
                                [ & ]( ir::IRBuilder& b, const Args& a )
                                {
                                  const auto fr  = j.mod.declare_func_in_func( four, b );
                                  const auto sig = b.import_signature( { Type::i64, Type::i64, Type::i64, Type::i64 }, Type::i64,
                                                                         codegen::host_call_conv( ) );
                                  const auto p   = b.func_addr( Type::ptr, fr );
                                  b.ret( b.call_indirect( sig, p, Args { a[ 1 ], a[ 0 ], a[ 1 ], a[ 0 ] } ) );
                              } );
    j.finalise( );
    CHECK( j.get< int64_t( int64_t, int64_t ) >( id )( 1, 2 ) == 2121 );
    CHECK( j.get< int64_t( int64_t, int64_t ) >( id )( 0, 9 ) == 9090 );
}

TEST_CASE( "func_addr returned to the host is callable", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto sq         = j.define( "sq", Type::i64, { Type::i64 },
                                      [ & ]( ir::IRBuilder& b, const Args& a )
                                      {
                                  b.ret( b.imul( a[ 0 ], a[ 0 ] ) );
                              } );
    const auto one        = j.import( "h_one", Type::i64, { Type::i64 } );
    const auto get_local  = j.define( "get_local", Type::ptr, { },
                                      [ & ]( ir::IRBuilder& b, const Args& )
                                      {
                                         b.ret( b.func_addr( Type::ptr, j.mod.declare_func_in_func( sq, b ) ) );
                                     } );
    const auto get_import = j.define( "get_import", Type::ptr, { },
                                      [ & ]( ir::IRBuilder& b, const Args& )
                                      {
                                          b.ret( b.func_addr( Type::ptr, j.mod.declare_func_in_func( one, b ) ) );
                                      } );
    j.finalise( );
    using Fn      = int64_t ( * )( int64_t );
    const auto pl = reinterpret_cast< Fn >( j.get< void*( ) >( get_local )( ) );
    const auto pi = reinterpret_cast< Fn >( j.get< void*( ) >( get_import )( ) );
    CHECK( reinterpret_cast< const void* >( pl ) == reinterpret_cast< const void* >( j.get< int64_t( int64_t ) >( sq ) ) );
    CHECK( reinterpret_cast< const void* >( pi ) == reinterpret_cast< const void* >( &h_one ) );
    CHECK( pl( 12 ) == 144 );
    CHECK( pi( 12 ) == 37 );
}

TEST_CASE( "call_indirect through a pointer passed as a parameter", "[jit][calls]" )
{
    Jit        j;
    const auto sq = j.define( "sq", Type::i64, { Type::i64 },
                              [ & ]( ir::IRBuilder& b, const Args& a )
                              {
                                  b.ret( b.imul( a[ 0 ], a[ 0 ] ) );
                              } );
    const auto id = j.define( "f", Type::i64, { Type::ptr, Type::i64 },
                              [ & ]( ir::IRBuilder& b, const Args& a )
                              {
                                  const auto sig = b.import_signature( { Type::i64 }, Type::i64, codegen::host_call_conv( ) );
                                  b.ret( b.call_indirect( sig, a[ 0 ], Args { a[ 1 ] } ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( const void*, int64_t ) >( id );
    for ( const auto x : small_values )
    {
        CHECK( f( reinterpret_cast< const void* >( &h_one ), x ) == h_one( x ) );
        CHECK( f( reinterpret_cast< const void* >( j.get< int64_t( int64_t ) >( sq ) ), x ) == x * x );
    }
}

TEST_CASE( "call_indirect with four arguments through a parameter", "[jit][calls]" )
{
    Jit        j;
    const auto id = j.define( "f", Type::i64, { Type::ptr, Type::i64, Type::i64, Type::i64 },
                              [ & ]( ir::IRBuilder& b, const Args& a )
                              {
                                  const auto sig = b.import_signature( { Type::i64, Type::i64, Type::i64, Type::i64 }, Type::i64,
                                                                       codegen::host_call_conv( ) );
                                  b.ret( b.call_indirect( sig, a[ 0 ], Args { a[ 3 ], a[ 2 ], a[ 1 ], b.iconst( Type::i64, 8 ) } ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( const void*, int64_t, int64_t, int64_t ) >( id );
    CHECK( f( reinterpret_cast< const void* >( &h_four ), 1, 2, 3 ) == 3218 );
    CHECK( f( reinterpret_cast< const void* >( &h_four ), 0, 0, 0 ) == 8 );
}

TEST_CASE( "call_indirect with no return value", "[jit][calls]" )
{
    Jit        j;
    const auto id = j.define( "f", Type::unit, { Type::ptr, Type::ptr, Type::i64 },
                              [ & ]( ir::IRBuilder& b, const Args& a )
                              {
                                  const auto sig = b.import_signature( { Type::ptr, Type::i64 }, Type::unit, codegen::host_call_conv( ) );
                                  b.call_indirect( sig, a[ 0 ], Args { a[ 1 ], a[ 2 ] } );
                                  b.call_indirect( sig, a[ 0 ], Args { a[ 1 ], b.ineg( a[ 2 ] ) } );
                                  b.call_indirect( sig, a[ 0 ], Args { a[ 1 ], b.iconst( Type::i64, 100 ) } );
                                  b.ret( );
                              } );
    j.finalise( );
    int64_t acc = 1;
    j.get< void( const void*, int64_t*, int64_t ) >( id )( reinterpret_cast< const void* >( &h_accumulate ), &acc, 55 );
    CHECK( acc == 101 );
}

TEST_CASE( "call_indirect through a function pointer loaded from memory", "[jit][calls]" )
{
    Jit        j;
    const auto id    = j.define( "f", Type::i64, { Type::ptr, Type::i64, Type::i64 },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                  const auto sig  = b.import_signature( { Type::i64 }, Type::i64, codegen::host_call_conv( ) );
                                  const auto slot = b.iadd( a[ 0 ], b.ishl( a[ 1 ], b.iconst( Type::i64, 3 ) ) );
                                  const auto fp   = b.load( Type::ptr, slot );
                                  b.ret( b.call_indirect( sig, fp, Args { a[ 2 ] } ) );
                              } );
    const auto fixed = j.define( "g", Type::i64, { Type::ptr, Type::i64 },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto sig = b.import_signature( { Type::i64 }, Type::i64, codegen::host_call_conv( ) );
                                     b.ret( b.call_indirect( sig, b.load( Type::ptr, a[ 0 ], 16 ), Args { a[ 1 ] } ) );
                                 } );
    j.finalise( );
    using Fn              = int64_t ( * )( int64_t );
    const Fn   table[ 3 ] = { &h_one,
                              +[]( const int64_t x ) -> int64_t
                              {
                                return -x;
                            },
                              +[]( const int64_t x ) -> int64_t
                              {
                                return x << 4;
                            } };
    const auto f          = j.get< int64_t( const Fn*, int64_t, int64_t ) >( id );
    for ( const auto x : small_values )
    {
        CHECK( f( table, 0, x ) == h_one( x ) );
        CHECK( f( table, 1, x ) == -x );
        CHECK( f( table, 2, x ) == ( x << 4 ) );
    }
    CHECK( j.get< int64_t( const Fn*, int64_t ) >( fixed )( table, 3 ) == 48 );
}

TEST_CASE( "values survive an indirect call", "[jit][calls]" )
{
    Jit        j;
    const auto id = j.define( "f", Type::i64, { Type::ptr, Type::i64, Type::i64, Type::i64 },
                              [ & ]( ir::IRBuilder& b, const Args& a )
                              {
                                  const auto sig = b.import_signature( { Type::i64 }, Type::i64, codegen::host_call_conv( ) );
                                  Args       vs;
                                  for ( int k = 0; k < 10; ++k ) vs.push_back( b.bxor( a[ 1 + k % 3 ], b.iconst( Type::i64, k * 17 ) ) );
                                  auto acc = b.call_indirect( sig, a[ 0 ], Args { a[ 1 ] } );
                                  for ( const auto v : vs ) acc = b.iadd( b.imul( acc, b.iconst( Type::i64, 5 ) ), v );
                                  b.ret( acc );
                              } );
    j.finalise( );
    const auto    f       = j.get< int64_t( const void*, int64_t, int64_t, int64_t ) >( id );
    const int64_t in[ 3 ] = { 11, -22, 33 };
    uint64_t      acc     = static_cast< uint64_t >( h_one( in[ 0 ] ) );
    for ( int k = 0; k < 10; ++k ) acc = acc * 5 + static_cast< uint64_t >( in[ k % 3 ] ^ ( k * 17 ) );
    CHECK( static_cast< uint64_t >( f( reinterpret_cast< const void* >( &h_one ), in[ 0 ], in[ 1 ], in[ 2 ] ) ) == acc );
}

TEST_CASE( "host function calls back into jit code", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto sq    = j.define( "sq", Type::i64, { Type::i64 },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                  b.ret( b.imul( a[ 0 ], a[ 0 ] ) );
                              } );
    const auto apply = j.import( "h_apply", Type::i64, { Type::ptr, Type::i64 } );
    const auto id    = j.define( "f", Type::i64, { Type::i64 },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                  const auto ap = j.mod.declare_func_in_func( apply, b );
                                  const auto p  = b.func_addr( Type::ptr, j.mod.declare_func_in_func( sq, b ) );
                                  b.ret( b.iadd( b.call( ap, Args { p, a[ 0 ] } ), a[ 0 ] ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( id );
    for ( const int64_t x : { 0, 1, 2, -5, 1000 } ) CHECK( f( x ) == x * x + ( x + 1 ) * ( x + 1 ) + x );
}

TEST_CASE( "host receives pointer arguments unchanged", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto diff = j.import( "h_ptr_diff", Type::i64, { Type::ptr, Type::ptr } );
    const auto id   = j.define( "f", Type::i64, { Type::ptr, Type::i64 },
                                [ & ]( ir::IRBuilder& b, const Args& a )
                                {
                                  const auto fr = j.mod.declare_func_in_func( diff, b );
                                  b.ret( b.call( fr, Args { b.iadd( a[ 0 ], a[ 1 ] ), a[ 0 ] } ) );
                              } );
    j.finalise( );
    const char buf[ 64 ] { };
    for ( const int64_t k : { 0, 1, 7, 63 } ) CHECK( j.get< int64_t( const char*, int64_t ) >( id )( buf, k ) == k );
}

TEST_CASE( "call result selected between branches", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto one = j.import( "h_one", Type::i64, { Type::i64 } );
    const auto two = j.import( "h_two", Type::i64, { Type::i64, Type::i64 } );
    const auto id  = j.define( "f", Type::i64, { Type::i64, Type::i64 },
                               [ & ]( ir::IRBuilder& b, const Args& a )
                               {
                                  const auto f1   = j.mod.declare_func_in_func( one, b );
                                  const auto f2   = j.mod.declare_func_in_func( two, b );
                                  const auto lhs  = b.add_block( { } );
                                  const auto rhs  = b.add_block( { } );
                                  const auto join = b.add_block( { Type::i64 } );
                                  b.brif( b.icmp( ir::ICmpCC::sgt, a[ 0 ], a[ 1 ] ), lhs, { }, rhs, { } );
                                  b.switch_to_block( lhs );
                                  b.br( join, Args { b.call( f1, Args { a[ 0 ] } ) } );
                                  b.switch_to_block( rhs );
                                  b.br( join, Args { b.call( f2, Args { a[ 1 ], a[ 0 ] } ) } );
                                  b.switch_to_block( join );
                                  b.ret( b.iadd( b.block_param( join, 0 ), b.isub( a[ 0 ], a[ 1 ] ) ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t ) >( id );
    for ( const auto x : small_values )
        for ( const int64_t y : { -5, 0, 5 } ) CHECK( f( x, y ) == ( x > y ? h_one( x ) : h_two( y, x ) ) + ( x - y ) );
}

TEST_CASE( "read eight parameters with the last four on the stack", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto id = j.define( "f", Type::i64, std::vector< Type >( 8, Type::i64 ),
                              [ & ]( ir::IRBuilder& b, const Args& a )
                              {
                                  auto sum = b.iconst( Type::i64, 0 );
                                  for ( uint64_t k { 0 }; k < 8; ++k ) sum = b.iadd( sum, b.imul( a[ k ], b.iconst( Type::i64, k + 1 ) ) );
                                  b.ret( sum );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t ) >( id );
    CHECK( f( 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000 ) == 87654321 );
}


TEST_CASE( "read a narrow parameter passed on the stack", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto id = j.define( "f", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64, Type::i64, Type::i32 },
                              [ & ]( ir::IRBuilder& b, const Args& a )
                              {
                                  b.ret( b.sextend( Type::i64, a[ 5 ] ) );
                              } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t, int64_t, int64_t, int64_t, int32_t ) >( id );
    CHECK( f( 0, 0, 0, 0, 0, -1 ) == -1 );
    CHECK( f( 0, 0, 0, 0, 0, 0x7fffffff ) == 0x7fffffff );
}

TEST_CASE( "read stack parameters in a function that makes a call", "[jit][calls]" )
{
    Jit        j { host_builder( ) };
    const auto callee = j.import( "h_one", Type::i64, { Type::i64 } );
    const auto id     = j.define( "f", Type::i64, std::vector< Type >( 8, Type::i64 ),
                                  [ & ]( ir::IRBuilder& b, const Args& a )
                                  {
                                      const auto fr = j.mod.declare_func_in_func( callee, b );
                                      b.ret( b.iadd( b.call( fr, Args { a[ 5 ] } ), a[ 7 ] ) );
                                  } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t ) >( id );
    CHECK( f( 0, 0, 0, 0, 0, 20, 0, 9 ) == 20 * 3 + 1 + 9 );
}