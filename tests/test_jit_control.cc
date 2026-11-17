#include "support.hh"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

using namespace xt;

namespace
{
    using CC = ir::ICmpCC;

    uint64_t u( const int64_t v )
    { return static_cast< uint64_t >( v ); }

    ValueRef k64( ir::IRBuilder& b, const int64_t v )
    { return b.iconst( Type::i64, u( v ) ); }

    template < typename F >
    uint64_t oracle( const Compiled< F >& c, const std::initializer_list< uint64_t > args )
    {
        const std::vector< uint64_t > v { args };
        return Interp { }.run( *c.jit->irm.get( ir::FunctionRef { 0 } ), v );
    }

    const std::vector< int64_t >& small_inputs( )
    {
        static const std::vector< int64_t > values = []
        {
            std::vector< int64_t > out;
            for ( int64_t i { -20 }; i <= 120; ++i ) out.push_back( i );
            for ( const int64_t v : { 255, 256, 1000, 4095, 65535, 100000 } ) out.push_back( v );
            return out;
        }( );
        return values;
    }
} // namespace

TEST_CASE( "unconditional branch to a block with no params", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto next = b.add_block( { } );
                                                const auto x    = b.imul( a[ 0 ], k64( b, 3 ) );
                                                b.br( next );
                                                b.switch_to_block( next );
                                                b.ret( b.iadd( x, a[ 0 ] ) );
                                            } );
    for ( const auto x : edge_values( ) ) CHECK( u( f( x ) ) == u( x ) * 4 );
}

TEST_CASE( "unconditional branch passing block args", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     []( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         const auto next = b.add_block( { Type::i64, Type::i64, Type::i64 } );
                                                         b.br( next, Args { a[ 1 ], a[ 0 ], b.iadd( a[ 0 ], a[ 1 ] ) } );
                                                         b.switch_to_block( next );
                                                         const auto p = b.block_param( next, 0 );
                                                         const auto q = b.block_param( next, 1 );
                                                         const auto r = b.block_param( next, 2 );
                                                         b.ret( b.iadd( b.isub( p, q ), b.imul( r, k64( b, 5 ) ) ) );
                                                     } );
    for ( const auto x : edge_values( ) )
        for ( const auto y : edge_values( ) )
        {
            CAPTURE( x, y );
            CHECK( u( f( x, y ) ) == u( y ) - u( x ) + ( u( x ) + u( y ) ) * 5 );
        }
}

TEST_CASE( "block args of narrow and pointer types", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::ptr, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto next = b.add_block( { Type::i8, Type::i16, Type::i32, Type::ptr } );
                                                b.br( next,
                                                      Args { b.ireduce( Type::i8, a[ 0 ] ), b.ireduce( Type::i16, a[ 0 ] ),
                                                             b.ireduce( Type::i32, a[ 0 ] ), b.iadd( b.iconst( Type::ptr, 0 ), a[ 0 ] ) } );
                                                b.switch_to_block( next );
                                                const auto s8  = b.sextend( Type::i64, b.block_param( next, 0 ) );
                                                const auto z16 = b.uextend( Type::i64, b.block_param( next, 1 ) );
                                                const auto s32 = b.sextend( Type::i64, b.block_param( next, 2 ) );
                                                const auto p   = b.block_param( next, 3 );
                                                b.ret( b.iadd( b.band( p, k64( b, 0xf ) ), b.iadd( b.iadd( s8, z16 ), s32 ) ) );
                                            } );
    for ( const auto x : edge_values( ) )
    {
        CAPTURE( x );
        const auto expected
            = u( static_cast< int8_t >( x ) ) + u( static_cast< uint16_t >( x ) ) + u( static_cast< int32_t >( x ) ) + ( u( x ) & 0xf );
        CHECK( u( f( x ) ) == expected );
        CHECK( u( f( x ) ) == oracle( f, { u( x ) } ) );
    }
}

TEST_CASE( "diamond computing abs merges through a block param", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto neg   = b.add_block( { } );
                                                const auto pos   = b.add_block( { } );
                                                const auto merge = b.add_block( { Type::i64 } );
                                                b.brif( b.icmp( CC::slt, a[ 0 ], k64( b, 0 ) ), neg, { }, pos, { } );
                                                b.switch_to_block( neg );
                                                b.br( merge, Args { b.ineg( a[ 0 ] ) } );
                                                b.switch_to_block( pos );
                                                b.br( merge, Args { a[ 0 ] } );
                                                b.switch_to_block( merge );
                                                b.ret( b.block_param( merge, 0 ) );
                                            } );
    for ( const auto x : edge_values( ) )
    {
        CAPTURE( x );
        CHECK( u( f( x ) ) == ( x < 0 ? 0 - u( x ) : u( x ) ) );
    }
}

TEST_CASE( "diamonds computing min max and clamp", "[jit][control]" )
{
    const auto minmax = []( const CC cc )
    {
        return compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                       [ cc ]( ir::IRBuilder& b, const Args& a )
                                                       {
                                                           const auto merge = b.add_block( { Type::i64 } );
                                                           b.brif( b.icmp( cc, a[ 0 ], a[ 1 ] ), merge, Args { a[ 0 ] }, merge,
                                                                   Args { a[ 1 ] } );
                                                           b.switch_to_block( merge );
                                                           b.ret( b.block_param( merge, 0 ) );
                                                       } );
    };
    auto smin = minmax( CC::slt );
    auto smax = minmax( CC::sgt );
    auto umin = minmax( CC::ult );
    auto umax = minmax( CC::ugt );

    auto clamp
        = compile< int64_t( int64_t, int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64, Type::i64 },
                                                           []( ir::IRBuilder& b, const Args& a )
                                                           {
                                                               const auto low   = b.add_block( { } );
                                                               const auto check = b.add_block( { } );
                                                               const auto high  = b.add_block( { } );
                                                               const auto out   = b.add_block( { Type::i64 } );
                                                               b.brif( b.icmp( CC::slt, a[ 0 ], a[ 1 ] ), low, { }, check, { } );
                                                               b.switch_to_block( low );
                                                               b.br( out, Args { a[ 1 ] } );
                                                               b.switch_to_block( check );
                                                               b.brif( b.icmp( CC::sgt, a[ 0 ], a[ 2 ] ), high, { }, out, Args { a[ 0 ] } );
                                                               b.switch_to_block( high );
                                                               b.br( out, Args { a[ 2 ] } );
                                                               b.switch_to_block( out );
                                                               b.ret( b.block_param( out, 0 ) );
                                                           } );

    for ( const auto x : edge_values( ) )
        for ( const auto y : edge_values( ) )
        {
            CAPTURE( x, y );
            CHECK( smin( x, y ) == std::min( x, y ) );
            CHECK( smax( x, y ) == std::max( x, y ) );
            CHECK( u( umin( x, y ) ) == std::min( u( x ), u( y ) ) );
            CHECK( u( umax( x, y ) ) == std::max( u( x ), u( y ) ) );
            if ( x <= y )
                for ( const auto v : edge_values( ) ) CHECK( clamp( v, x, y ) == std::clamp( v, x, y ) );
        }
}

TEST_CASE( "nested diamonds classify a value into four ranges", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto neg     = b.add_block( { } );
                                                const auto nonneg  = b.add_block( { } );
                                                const auto very    = b.add_block( { } );
                                                const auto little  = b.add_block( { } );
                                                const auto big     = b.add_block( { } );
                                                const auto small   = b.add_block( { } );
                                                const auto inner_n = b.add_block( { Type::i64 } );
                                                const auto inner_p = b.add_block( { Type::i64 } );
                                                const auto out     = b.add_block( { Type::i64 } );
                                                b.brif( b.icmp( CC::slt, a[ 0 ], k64( b, 0 ) ), neg, { }, nonneg, { } );
                                                b.switch_to_block( neg );
                                                b.brif( b.icmp( CC::slt, a[ 0 ], k64( b, -100 ) ), very, { }, little, { } );
                                                b.switch_to_block( nonneg );
                                                b.brif( b.icmp( CC::sgt, a[ 0 ], k64( b, 100 ) ), big, { }, small, { } );
                                                b.switch_to_block( very );
                                                b.br( inner_n, Args { k64( b, 0 ) } );
                                                b.switch_to_block( little );
                                                b.br( inner_n, Args { k64( b, 1 ) } );
                                                b.switch_to_block( big );
                                                b.br( inner_p, Args { k64( b, 3 ) } );
                                                b.switch_to_block( small );
                                                b.br( inner_p, Args { k64( b, 2 ) } );
                                                b.switch_to_block( inner_n );
                                                b.br( out, Args { b.iadd( b.block_param( inner_n, 0 ), k64( b, 10 ) ) } );
                                                b.switch_to_block( inner_p );
                                                b.br( out, Args { b.iadd( b.block_param( inner_p, 0 ), k64( b, 20 ) ) } );
                                                b.switch_to_block( out );
                                                b.ret( b.block_param( out, 0 ) );
                                            } );
    for ( int64_t x { -300 }; x <= 300; ++x )
    {
        const int64_t expected = x < 0 ? ( x < -100 ? 10 : 11 ) : ( x > 100 ? 23 : 22 );
        CAPTURE( x );
        CHECK( f( x ) == expected );
    }
    for ( const auto x : edge_values( ) ) CHECK( u( f( x ) ) == oracle( f, { u( x ) } ) );
}

TEST_CASE( "binary decision tree of depth six over the bits of the input", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                std::function< void( int, int64_t ) > node = [ & ]( const int depth, const int64_t prefix )
                                                {
                                                    if ( depth == 6 )
                                                    {
                                                        b.ret( k64( b, prefix * 7 + 1 ) );
                                                        return;
                                                    }
                                                    const auto one  = b.add_block( { } );
                                                    const auto zero = b.add_block( { } );
                                                    const auto bit  = b.band( b.ushr( a[ 0 ], k64( b, depth ) ), k64( b, 1 ) );
                                                    b.brif( bit, one, { }, zero, { } );
                                                    b.switch_to_block( one );
                                                    node( depth + 1, prefix | ( int64_t { 1 } << depth ) );
                                                    b.switch_to_block( zero );
                                                    node( depth + 1, prefix );
                                                };
                                                node( 0, 0 );
                                            } );
    for ( int64_t x { 0 }; x < 256; ++x )
    {
        CAPTURE( x );
        CHECK( f( x ) == ( x & 63 ) * 7 + 1 );
    }
    for ( const auto x : edge_values( ) ) CHECK( f( x ) == ( x & 63 ) * 7 + 1 );
}

TEST_CASE( "while loop summing 1 to n", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto hdr  = b.add_block( { Type::i64, Type::i64 } );
                                                const auto body = b.add_block( { } );
                                                const auto exit = b.add_block( { Type::i64 } );
                                                b.br( hdr, Args { a[ 0 ], k64( b, 0 ) } );
                                                b.switch_to_block( hdr );
                                                const auto i   = b.block_param( hdr, 0 );
                                                const auto acc = b.block_param( hdr, 1 );
                                                b.brif( b.icmp( CC::sgt, i, k64( b, 0 ) ), body, { }, exit, Args { acc } );
                                                b.switch_to_block( body );
                                                b.br( hdr, Args { b.isub( i, k64( b, 1 ) ), b.iadd( acc, i ) } );
                                                b.switch_to_block( exit );
                                                b.ret( b.block_param( exit, 0 ) );
                                            } );
    for ( const auto n : small_inputs( ) )
    {
        CAPTURE( n );
        CHECK( f( n ) == ( n > 0 ? n * ( n + 1 ) / 2 : 0 ) );
    }
}

TEST_CASE( "do while loop with the back edge on the conditional branch", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto loop = b.add_block( { Type::i64, Type::i64 } );
                                                const auto exit = b.add_block( { Type::i64 } );
                                                b.br( loop, Args { k64( b, 0 ), k64( b, 0 ) } );
                                                b.switch_to_block( loop );
                                                const auto i    = b.block_param( loop, 0 );
                                                const auto acc  = b.block_param( loop, 1 );
                                                const auto next = b.iadd( i, k64( b, 1 ) );
                                                const auto sum  = b.iadd( acc, b.imul( next, next ) );
                                                b.brif( b.icmp( CC::slt, next, a[ 0 ] ), loop, Args { next, sum }, exit, Args { sum } );
                                                b.switch_to_block( exit );
                                                b.ret( b.block_param( exit, 0 ) );
                                            } );
    for ( const auto n : small_inputs( ) )
    {
        int64_t i { 0 }, acc { 0 };
        do
        {
            ++i;
            acc += i * i;
        } while ( i < n );
        CAPTURE( n );
        CHECK( f( n ) == acc );
    }
}

TEST_CASE( "factorial loop with wrapping multiplication", "[jit][control]" )
{
    auto     f = compile< uint64_t( uint64_t ) >( Type::i64, { Type::i64 },
                                                  []( ir::IRBuilder& b, const Args& a )
                                                  {
                                                  const auto hdr  = b.add_block( { Type::i64, Type::i64 } );
                                                  const auto body = b.add_block( { } );
                                                  const auto exit = b.add_block( { Type::i64 } );
                                                  b.br( hdr, Args { a[ 0 ], k64( b, 1 ) } );
                                                  b.switch_to_block( hdr );
                                                  const auto i   = b.block_param( hdr, 0 );
                                                  const auto acc = b.block_param( hdr, 1 );
                                                  b.brif( b.icmp( CC::ugt, i, k64( b, 1 ) ), body, { }, exit, Args { acc } );
                                                  b.switch_to_block( body );
                                                  b.br( hdr, Args { b.isub( i, k64( b, 1 ) ), b.imul( acc, i ) } );
                                                  b.switch_to_block( exit );
                                                  b.ret( b.block_param( exit, 0 ) );
                                              } );
    uint64_t expected { 1 };
    for ( uint64_t n { 0 }; n <= 70; ++n )
    {
        if ( n > 1 ) expected *= n;
        CAPTURE( n );
        CHECK( f( n ) == expected );
    }
}

TEST_CASE( "fibonacci loop swapping block args on a critical back edge", "[jit][control]" )
{
    auto     f = compile< uint64_t( uint64_t ) >( Type::i64, { Type::i64 },
                                                  []( ir::IRBuilder& b, const Args& a )
                                                  {
                                                  const auto hdr  = b.add_block( { Type::i64, Type::i64, Type::i64 } );
                                                  const auto exit = b.add_block( { Type::i64 } );
                                                  b.br( hdr, Args { a[ 0 ], k64( b, 0 ), k64( b, 1 ) } );
                                                  b.switch_to_block( hdr );
                                                  const auto n = b.block_param( hdr, 0 );
                                                  const auto x = b.block_param( hdr, 1 );
                                                  const auto y = b.block_param( hdr, 2 );
                                                  b.brif( b.icmp( CC::ne, n, k64( b, 0 ) ), hdr,
                                                              Args { b.isub( n, k64( b, 1 ) ), y, b.iadd( x, y ) }, exit, Args { x } );
                                                  b.switch_to_block( exit );
                                                  b.ret( b.block_param( exit, 0 ) );
                                              } );
    uint64_t x { 0 }, y { 1 };
    for ( uint64_t n { 0 }; n <= 150; ++n )
    {
        CAPTURE( n );
        CHECK( f( n ) == x );
        CHECK( f( n ) == oracle( f, { n } ) );
        const auto t = x + y;
        x            = y;
        y            = t;
    }
}

TEST_CASE( "euclid gcd loop with urem in the body", "[jit][control]" )
{
    auto f = compile< uint64_t( uint64_t, uint64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                        []( ir::IRBuilder& b, const Args& a )
                                                        {
                                                            const auto hdr  = b.add_block( { Type::i64, Type::i64 } );
                                                            const auto step = b.add_block( { } );
                                                            const auto exit = b.add_block( { Type::i64 } );
                                                            b.br( hdr, Args { a[ 0 ], a[ 1 ] } );
                                                            b.switch_to_block( hdr );
                                                            const auto x = b.block_param( hdr, 0 );
                                                            const auto y = b.block_param( hdr, 1 );
                                                            b.brif( b.icmp( CC::eq, y, k64( b, 0 ) ), exit, Args { x }, step, { } );
                                                            b.switch_to_block( step );
                                                            b.br( hdr, Args { y, b.urem( x, y ) } );
                                                            b.switch_to_block( exit );
                                                            b.ret( b.block_param( exit, 0 ) );
                                                        } );
    for ( uint64_t x { 0 }; x < 60; ++x )
        for ( uint64_t y { 0 }; y < 60; ++y )
        {
            CAPTURE( x, y );
            CHECK( f( x, y ) == std::gcd( x, y ) );
        }
    for ( const auto x : edge_values( ) )
        for ( const auto y : edge_values( ) )
        {
            CAPTURE( x, y );
            CHECK( f( u( x ), u( y ) ) == std::gcd( u( x ), u( y ) ) );
        }
}

TEST_CASE( "collatz step counting with three predecessors on the loop header", "[jit][control]" )
{
    auto f = compile< int64_t( uint64_t ) >( Type::i64, { Type::i64 },
                                             []( ir::IRBuilder& b, const Args& a )
                                             {
                                                 const auto hdr  = b.add_block( { Type::i64, Type::i64 } );
                                                 const auto body = b.add_block( { } );
                                                 const auto odd  = b.add_block( { } );
                                                 const auto even = b.add_block( { } );
                                                 const auto exit = b.add_block( { Type::i64 } );
                                                 b.br( hdr, Args { a[ 0 ], k64( b, 0 ) } );
                                                 b.switch_to_block( hdr );
                                                 const auto n     = b.block_param( hdr, 0 );
                                                 const auto steps = b.block_param( hdr, 1 );
                                                 b.brif( b.icmp( CC::ule, n, k64( b, 1 ) ), exit, Args { steps }, body, { } );
                                                 b.switch_to_block( body );
                                                 const auto next_steps = b.iadd( steps, k64( b, 1 ) );
                                                 b.brif( b.band( n, k64( b, 1 ) ), odd, { }, even, { } );
                                                 b.switch_to_block( odd );
                                                 b.br( hdr, Args { b.iadd( b.imul( n, k64( b, 3 ) ), k64( b, 1 ) ), next_steps } );
                                                 b.switch_to_block( even );
                                                 b.br( hdr, Args { b.ushr( n, k64( b, 1 ) ), next_steps } );
                                                 b.switch_to_block( exit );
                                                 b.ret( b.block_param( exit, 0 ) );
                                             } );
    for ( uint64_t n { 0 }; n <= 1000; ++n )
    {
        int64_t  steps { 0 };
        uint64_t m { n };
        while ( m > 1 )
        {
            m = ( m & 1 ) ? m * 3 + 1 : m >> 1;
            ++steps;
        }
        CAPTURE( n );
        CHECK( f( n ) == steps );
    }
}

TEST_CASE( "collatz step counting with select instead of branches", "[jit][control]" )
{
    auto f = compile< int64_t( uint64_t ) >(
        Type::i64, { Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            const auto hdr  = b.add_block( { Type::i64, Type::i64 } );
            const auto exit = b.add_block( { Type::i64 } );
            b.br( hdr, Args { a[ 0 ], k64( b, 0 ) } );
            b.switch_to_block( hdr );
            const auto n = b.block_param( hdr, 0 );
            const auto s = b.block_param( hdr, 1 );
            const auto next
                = b.select( b.band( n, k64( b, 1 ) ), b.iadd( b.imul( n, k64( b, 3 ) ), k64( b, 1 ) ), b.ushr( n, k64( b, 1 ) ) );
            b.brif( b.icmp( CC::ugt, n, k64( b, 1 ) ), hdr, Args { next, b.iadd( s, k64( b, 1 ) ) }, exit, Args { s } );
            b.switch_to_block( exit );
            b.ret( b.block_param( exit, 0 ) );
        } );
    for ( uint64_t n { 0 }; n <= 1000; ++n )
    {
        int64_t  steps { 0 };
        uint64_t m { n };
        while ( m > 1 )
        {
            m = ( m & 1 ) ? m * 3 + 1 : m >> 1;
            ++steps;
        }
        CAPTURE( n );
        CHECK( f( n ) == steps );
    }
}

TEST_CASE( "popcount loop clearing the lowest set bit", "[jit][control]" )
{
    auto f = compile< int64_t( uint64_t ) >( Type::i64, { Type::i64 },
                                             []( ir::IRBuilder& b, const Args& a )
                                             {
                                                 const auto hdr  = b.add_block( { Type::i64, Type::i64 } );
                                                 const auto body = b.add_block( { } );
                                                 const auto exit = b.add_block( { Type::i64 } );
                                                 b.br( hdr, Args { a[ 0 ], k64( b, 0 ) } );
                                                 b.switch_to_block( hdr );
                                                 const auto x = b.block_param( hdr, 0 );
                                                 const auto c = b.block_param( hdr, 1 );
                                                 b.brif( b.icmp( CC::ne, x, k64( b, 0 ) ), body, { }, exit, Args { c } );
                                                 b.switch_to_block( body );
                                                 b.br( hdr, Args { b.band( x, b.isub( x, k64( b, 1 ) ) ), b.iadd( c, k64( b, 1 ) ) } );
                                                 b.switch_to_block( exit );
                                                 b.ret( b.block_param( exit, 0 ) );
                                             } );
    for ( const auto x : edge_values( ) )
    {
        CAPTURE( x );
        CHECK( f( u( x ) ) == std::popcount( u( x ) ) );
    }
    for ( uint64_t x { 0 }; x < 1024; ++x ) CHECK( f( x * 0x9e3779b97f4a7c15ull ) == std::popcount( x * 0x9e3779b97f4a7c15ull ) );
}

TEST_CASE( "nested loops summing products below the diagonal", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto outer      = b.add_block( { Type::i64, Type::i64 } );
                                                const auto inner_init = b.add_block( { } );
                                                const auto inner      = b.add_block( { Type::i64, Type::i64 } );
                                                const auto ibody      = b.add_block( { } );
                                                const auto onext      = b.add_block( { } );
                                                const auto exit       = b.add_block( { Type::i64 } );
                                                b.br( outer, Args { k64( b, 0 ), k64( b, 0 ) } );

                                                b.switch_to_block( outer );
                                                const auto i   = b.block_param( outer, 0 );
                                                const auto acc = b.block_param( outer, 1 );
                                                b.brif( b.icmp( CC::slt, i, a[ 0 ] ), inner_init, { }, exit, Args { acc } );

                                                b.switch_to_block( inner_init );
                                                b.br( inner, Args { k64( b, 0 ), acc } );

                                                b.switch_to_block( inner );
                                                const auto j    = b.block_param( inner, 0 );
                                                const auto acc2 = b.block_param( inner, 1 );
                                                b.brif( b.icmp( CC::slt, j, i ), ibody, { }, onext, { } );

                                                b.switch_to_block( ibody );
                                                b.br( inner, Args { b.iadd( j, k64( b, 1 ) ), b.iadd( acc2, b.imul( i, j ) ) } );

                                                b.switch_to_block( onext );
                                                b.br( outer, Args { b.iadd( i, k64( b, 1 ) ), acc2 } );

                                                b.switch_to_block( exit );
                                                b.ret( b.block_param( exit, 0 ) );
                                            } );
    for ( int64_t n { -3 }; n <= 60; ++n )
    {
        int64_t expected { 0 };
        for ( int64_t i { 0 }; i < n; ++i )
            for ( int64_t j { 0 }; j < i; ++j ) expected += i * j;
        CAPTURE( n );
        CHECK( f( n ) == expected );
    }
}

TEST_CASE( "triple nested loop counting lattice points", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto l1    = b.add_block( { Type::i64, Type::i64 } );
                                                const auto l2    = b.add_block( { Type::i64, Type::i64 } );
                                                const auto l3    = b.add_block( { Type::i64, Type::i64 } );
                                                const auto l3b   = b.add_block( { } );
                                                const auto l2n   = b.add_block( { } );
                                                const auto l1n   = b.add_block( { } );
                                                const auto l2ini = b.add_block( { } );
                                                const auto l3ini = b.add_block( { } );
                                                const auto exit  = b.add_block( { Type::i64 } );
                                                b.br( l1, Args { k64( b, 0 ), k64( b, 0 ) } );

                                                b.switch_to_block( l1 );
                                                const auto x = b.block_param( l1, 0 );
                                                const auto c = b.block_param( l1, 1 );
                                                b.brif( b.icmp( CC::sle, x, a[ 0 ] ), l2ini, { }, exit, Args { c } );

                                                b.switch_to_block( l2ini );
                                                b.br( l2, Args { k64( b, 0 ), c } );

                                                b.switch_to_block( l2 );
                                                const auto y  = b.block_param( l2, 0 );
                                                const auto c2 = b.block_param( l2, 1 );
                                                b.brif( b.icmp( CC::sle, y, a[ 0 ] ), l3ini, { }, l1n, { } );

                                                b.switch_to_block( l3ini );
                                                b.br( l3, Args { k64( b, 0 ), c2 } );

                                                b.switch_to_block( l3 );
                                                const auto z  = b.block_param( l3, 0 );
                                                const auto c3 = b.block_param( l3, 1 );
                                                b.brif( b.icmp( CC::sle, z, a[ 0 ] ), l3b, { }, l2n, { } );

                                                b.switch_to_block( l3b );
                                                const auto sq     = b.iadd( b.iadd( b.imul( x, x ), b.imul( y, y ) ), b.imul( z, z ) );
                                                const auto inside = b.uextend( Type::i64, b.icmp( CC::sle, sq, b.imul( a[ 0 ], a[ 0 ] ) ) );
                                                b.br( l3, Args { b.iadd( z, k64( b, 1 ) ), b.iadd( c3, inside ) } );

                                                b.switch_to_block( l2n );
                                                b.br( l2, Args { b.iadd( y, k64( b, 1 ) ), c3 } );

                                                b.switch_to_block( l1n );
                                                b.br( l1, Args { b.iadd( x, k64( b, 1 ) ), c2 } );

                                                b.switch_to_block( exit );
                                                b.ret( b.block_param( exit, 0 ) );
                                            } );
    for ( int64_t r { 0 }; r <= 12; ++r )
    {
        int64_t expected { 0 };
        for ( int64_t x { 0 }; x <= r; ++x )
            for ( int64_t y { 0 }; y <= r; ++y )
                for ( int64_t z { 0 }; z <= r; ++z ) expected += x * x + y * y + z * z <= r * r;
        CAPTURE( r );
        CHECK( f( r ) == expected );
    }
}

TEST_CASE( "loop with an early exit finds the integer square root", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto loop  = b.add_block( { Type::i64 } );
                                                const auto next  = b.add_block( { } );
                                                const auto found = b.add_block( { Type::i64 } );
                                                b.br( loop, Args { k64( b, 0 ) } );
                                                b.switch_to_block( loop );
                                                const auto i = b.block_param( loop, 0 );
                                                b.brif( b.icmp( CC::sgt, b.imul( i, i ), a[ 0 ] ), found, Args { b.isub( i, k64( b, 1 ) ) },
                                                        next, { } );
                                                b.switch_to_block( next );
                                                b.br( loop, Args { b.iadd( i, k64( b, 1 ) ) } );
                                                b.switch_to_block( found );
                                                b.ret( b.block_param( found, 0 ) );
                                            } );
    for ( int64_t n { 0 }; n <= 3000; ++n )
    {
        int64_t r { 0 };
        while ( ( r + 1 ) * ( r + 1 ) <= n ) ++r;
        CAPTURE( n );
        CHECK( f( n ) == r );
    }
    CHECK( f( 1000000 ) == 1000 );
    CHECK( f( 999999 ) == 999 );
}

TEST_CASE( "loop with two distinct exits", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     []( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         const auto loop     = b.add_block( { Type::i64 } );
                                                         const auto check    = b.add_block( { } );
                                                         const auto next     = b.add_block( { } );
                                                         const auto found    = b.add_block( { Type::i64 } );
                                                         const auto notfound = b.add_block( { } );
                                                         b.br( loop, Args { k64( b, 0 ) } );
                                                         b.switch_to_block( loop );
                                                         const auto k = b.block_param( loop, 0 );
                                                         b.brif( b.icmp( CC::slt, k, a[ 0 ] ), check, { }, notfound, { } );
                                                         b.switch_to_block( check );
                                                         const auto h = b.urem(
                                                             b.iadd( b.imul( b.imul( k, k ), k64( b, 7 ) ), k64( b, 3 ) ), k64( b, 13 ) );
                                                         b.brif( b.icmp( CC::eq, h, a[ 1 ] ), found, Args { k }, next, { } );
                                                         b.switch_to_block( next );
                                                         b.br( loop, Args { b.iadd( k, k64( b, 1 ) ) } );
                                                         b.switch_to_block( found );
                                                         b.ret( b.block_param( found, 0 ) );
                                                         b.switch_to_block( notfound );
                                                         b.ret( k64( b, -1 ) );
                                                     } );
    for ( int64_t n { 0 }; n <= 20; ++n )
        for ( int64_t t { 0 }; t < 14; ++t )
        {
            int64_t expected { -1 };
            for ( int64_t k { 0 }; k < n; ++k )
                if ( ( k * k * 7 + 3 ) % 13 == t )
                {
                    expected = k;
                    break;
                }
            CAPTURE( n, t );
            CHECK( f( n, t ) == expected );
        }
}

TEST_CASE( "brif with both arms targeting the same block with different args", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     []( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         const auto m = b.add_block( { Type::i64 } );
                                                         b.brif( a[ 1 ], m, Args { b.iadd( a[ 0 ], k64( b, 1 ) ) }, m,
                                                                 Args { b.isub( a[ 0 ], k64( b, 1 ) ) } );
                                                         b.switch_to_block( m );
                                                         b.ret( b.imul( b.block_param( m, 0 ), k64( b, 2 ) ) );
                                                     } );
    for ( const auto x : edge_values( ) )
        for ( const int64_t c : { int64_t { 0 }, int64_t { 1 }, int64_t { -1 }, int64_t { 0x100000000LL } } )
        {
            CAPTURE( x, c );
            CHECK( u( f( x, c ) ) == ( c != 0 ? u( x ) + 1 : u( x ) - 1 ) * 2 );
        }
}

TEST_CASE( "brif with both arms targeting the same block without args", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto m = b.add_block( { } );
                                                b.brif( b.icmp( CC::slt, a[ 0 ], k64( b, 5 ) ), m, { }, m, { } );
                                                b.switch_to_block( m );
                                                b.ret( b.iadd( a[ 0 ], k64( b, 9 ) ) );
                                            } );
    for ( const auto x : edge_values( ) ) CHECK( u( f( x ) ) == u( x ) + 9 );
}

TEST_CASE( "brif into a join block that has another predecessor", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     []( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         const auto side = b.add_block( { } );
                                                         const auto join = b.add_block( { Type::i64 } );
                                                         b.brif( a[ 1 ], join, Args { a[ 0 ] }, side, { } );
                                                         b.switch_to_block( side );
                                                         b.br( join, Args { b.iadd( a[ 0 ], k64( b, 100 ) ) } );
                                                         b.switch_to_block( join );
                                                         b.ret( b.block_param( join, 0 ) );
                                                     } );
    auto g = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     []( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         const auto side = b.add_block( { } );
                                                         const auto join = b.add_block( { } );
                                                         b.brif( a[ 1 ], join, { }, side, { } );
                                                         b.switch_to_block( side );
                                                         b.br( join );
                                                         b.switch_to_block( join );
                                                         b.ret( b.imul( a[ 0 ], k64( b, 3 ) ) );
                                                     } );
    auto h = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     []( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         const auto side = b.add_block( { } );
                                                         const auto join = b.add_block( { Type::i64 } );
                                                         b.brif( a[ 1 ], side, { }, join, Args { a[ 0 ] } );
                                                         b.switch_to_block( side );
                                                         b.br( join, Args { b.isub( a[ 0 ], k64( b, 100 ) ) } );
                                                         b.switch_to_block( join );
                                                         b.ret( b.block_param( join, 0 ) );
                                                     } );
    for ( const auto x : edge_values( ) )
        for ( const int64_t c : { int64_t { 0 }, int64_t { 1 }, int64_t { -1 } } )
        {
            CAPTURE( x, c );
            CHECK( u( f( x, c ) ) == ( c != 0 ? u( x ) : u( x ) + 100 ) );
            CHECK( u( g( x, c ) ) == u( x ) * 3 );
            CHECK( u( h( x, c ) ) == ( c != 0 ? u( x ) - 100 : u( x ) ) );
        }
}

TEST_CASE( "long linear chain of sixty blocks threading a value", "[jit][control]" )
{
    constexpr int n = 60;
    auto          f = compile< uint64_t( uint64_t ) >( Type::i64, { Type::i64 },
                                                       []( ir::IRBuilder& b, const Args& a )
                                                       {
                                                  std::vector< ir::BlockRef > blocks;
                                                  for ( int k { 0 }; k < n; ++k ) blocks.push_back( b.add_block( { Type::i64 } ) );
                                                  b.br( blocks[ 0 ], Args { a[ 0 ] } );
                                                  for ( int k { 0 }; k < n; ++k )
                                                  {
                                                      b.switch_to_block( blocks[ k ] );
                                                      const auto v    = b.block_param( blocks[ k ], 0 );
                                                      const auto next = b.iadd( b.imul( v, k64( b, 3 ) ), k64( b, k ) );
                                                      if ( k + 1 < n ) b.br( blocks[ k + 1 ], Args { next } );
                                                      else
                                                          b.ret( next );
                                                  }
                                              } );
    for ( const auto x : edge_values( ) )
    {
        uint64_t v = u( x );
        for ( int k { 0 }; k < n; ++k ) v = v * 3 + static_cast< uint64_t >( k );
        CAPTURE( x );
        CHECK( f( u( x ) ) == v );
    }
}

TEST_CASE( "chain of sixty four comparisons acting as a switch", "[jit][control]" )
{
    constexpr int n = 64;
    auto          f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                     []( ir::IRBuilder& b, const Args& a )
                                                     {
                                                std::vector< ir::BlockRef > tests, results;
                                                for ( int k { 0 }; k < n; ++k )
                                                {
                                                    tests.push_back( b.add_block( { } ) );
                                                    results.push_back( b.add_block( { } ) );
                                                }
                                                const auto fallback = b.add_block( { } );
                                                b.br( tests[ 0 ] );
                                                for ( int k { 0 }; k < n; ++k )
                                                {
                                                    b.switch_to_block( tests[ k ] );
                                                    b.brif( b.icmp( CC::eq, a[ 0 ], k64( b, k * 3 ) ), results[ k ], { },
                                                            k + 1 < n ? tests[ k + 1 ] : fallback, { } );
                                                    b.switch_to_block( results[ k ] );
                                                    b.ret( k64( b, k * k + 1 ) );
                                                }
                                                b.switch_to_block( fallback );
                                                b.ret( k64( b, -1 ) );
                                            } );
    for ( int64_t x { -5 }; x < n * 3 + 5; ++x )
    {
        const int64_t expected = x >= 0 && x % 3 == 0 && x / 3 < n ? ( x / 3 ) * ( x / 3 ) + 1 : -1;
        CAPTURE( x );
        CHECK( f( x ) == expected );
    }
}

TEST_CASE( "blocks filled in reverse creation order", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto b1 = b.add_block( { Type::i64 } );
                                                const auto b2 = b.add_block( { Type::i64 } );
                                                const auto b3 = b.add_block( { Type::i64 } );
                                                b.br( b1, Args { a[ 0 ] } );
                                                b.switch_to_block( b3 );
                                                b.ret( b.imul( b.block_param( b3, 0 ), k64( b, 7 ) ) );
                                                b.switch_to_block( b2 );
                                                b.br( b3, Args { b.iadd( b.block_param( b2, 0 ), k64( b, 5 ) ) } );
                                                b.switch_to_block( b1 );
                                                b.br( b2, Args { b.isub( b.block_param( b1, 0 ), k64( b, 2 ) ) } );
                                            } );
    for ( const auto x : edge_values( ) )
    {
        CAPTURE( x );
        CHECK( u( f( x ) ) == ( u( x ) - 2 + 5 ) * 7 );
    }
}

TEST_CASE( "loop swapping two block args each iteration", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t, int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64, Type::i64 },
                                                              []( ir::IRBuilder& b, const Args& a )
                                                              {
                                                                  const auto hdr  = b.add_block( { Type::i64, Type::i64, Type::i64 } );
                                                                  const auto body = b.add_block( { } );
                                                                  const auto exit = b.add_block( { } );
                                                                  b.br( hdr, Args { a[ 0 ], a[ 1 ], a[ 2 ] } );
                                                                  b.switch_to_block( hdr );
                                                                  const auto n = b.block_param( hdr, 0 );
                                                                  const auto x = b.block_param( hdr, 1 );
                                                                  const auto y = b.block_param( hdr, 2 );
                                                                  b.brif( n, body, { }, exit, { } );
                                                                  b.switch_to_block( body );
                                                                  b.br( hdr, Args { b.isub( n, k64( b, 1 ) ), y, x } );
                                                                  b.switch_to_block( exit );
                                                                  b.ret( b.isub( b.imul( x, k64( b, 1000 ) ), y ) );
                                                              } );
    for ( int64_t n { 0 }; n < 9; ++n )
        for ( const int64_t x : { int64_t { 3 }, int64_t { -8 } } )
            for ( const int64_t y : { int64_t { 5 }, int64_t { 77 } } )
            {
                const auto [ p, q ] = n % 2 == 0 ? std::pair { x, y } : std::pair { y, x };
                CAPTURE( n, x, y );
                CHECK( f( n, x, y ) == p * 1000 - q );
            }
}

TEST_CASE( "loop rotating four block args each iteration", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto hdr  = b.add_block( { Type::i64, Type::i64, Type::i64, Type::i64, Type::i64 } );
                                                const auto exit = b.add_block( { Type::i64, Type::i64, Type::i64, Type::i64 } );
                                                b.br( hdr, Args { a[ 0 ], k64( b, 1 ), k64( b, 2 ), k64( b, 3 ), k64( b, 4 ) } );
                                                b.switch_to_block( hdr );
                                                const auto n = b.block_param( hdr, 0 );
                                                const auto p = b.block_param( hdr, 1 );
                                                const auto q = b.block_param( hdr, 2 );
                                                const auto r = b.block_param( hdr, 3 );
                                                const auto s = b.block_param( hdr, 4 );
                                                b.brif( b.icmp( CC::ne, n, k64( b, 0 ) ), hdr,
                                                        Args { b.isub( n, k64( b, 1 ) ), s, p, q, r }, exit, Args { p, q, r, s } );
                                                b.switch_to_block( exit );
                                                const auto e0 = b.block_param( exit, 0 );
                                                const auto e1 = b.block_param( exit, 1 );
                                                const auto e2 = b.block_param( exit, 2 );
                                                const auto e3 = b.block_param( exit, 3 );
                                                b.ret( b.iadd( b.iadd( e0, b.imul( e1, k64( b, 10 ) ) ),
                                                               b.iadd( b.imul( e2, k64( b, 100 ) ), b.imul( e3, k64( b, 1000 ) ) ) ) );
                                            } );
    for ( int64_t n { 0 }; n < 13; ++n )
    {
        std::array< int64_t, 4 > v { 1, 2, 3, 4 };
        for ( int64_t k { 0 }; k < n; ++k ) v = { v[ 3 ], v[ 0 ], v[ 1 ], v[ 2 ] };
        CAPTURE( n );
        CHECK( f( n ) == v[ 0 ] + v[ 1 ] * 10 + v[ 2 ] * 100 + v[ 3 ] * 1000 );
    }
}

TEST_CASE( "block args that duplicate one value into several params", "[jit][control]" )
{
    auto f
        = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                         []( ir::IRBuilder& b, const Args& a )
                                         {
                                             const auto next = b.add_block( { Type::i64, Type::i64, Type::i64 } );
                                             const auto last = b.add_block( { Type::i64, Type::i64 } );
                                             b.br( next, Args { a[ 0 ], a[ 0 ], a[ 0 ] } );
                                             b.switch_to_block( next );
                                             const auto p = b.block_param( next, 0 );
                                             const auto q = b.block_param( next, 1 );
                                             const auto r = b.block_param( next, 2 );
                                             b.br( last, Args { b.iadd( p, q ), r } );
                                             b.switch_to_block( last );
                                             b.ret( b.isub( b.block_param( last, 0 ), b.imul( b.block_param( last, 1 ), k64( b, 3 ) ) ) );
                                         } );
    for ( const auto x : edge_values( ) ) CHECK( u( f( x ) ) == u( x ) * 2 - u( x ) * 3 );
}

TEST_CASE( "loop passing params through unchanged and ignoring unused params", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     []( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         const auto hdr  = b.add_block( { Type::i64, Type::i64, Type::i64, Type::i64 } );
                                                         const auto exit = b.add_block( { Type::i64, Type::i64 } );
                                                         b.br( hdr, Args { k64( b, 0 ), a[ 1 ], k64( b, 0 ), k64( b, 999 ) } );
                                                         b.switch_to_block( hdr );
                                                         const auto i      = b.block_param( hdr, 0 );
                                                         const auto step   = b.block_param( hdr, 1 );
                                                         const auto acc    = b.block_param( hdr, 2 );
                                                         const auto unused = b.block_param( hdr, 3 );
                                                         b.brif( b.icmp( CC::slt, i, a[ 0 ] ), hdr,
                                                                 Args { b.iadd( i, k64( b, 1 ) ), step, b.iadd( acc, step ), unused }, exit,
                                                                 Args { acc, k64( b, 5 ) } );
                                                         b.switch_to_block( exit );
                                                         b.ret( b.iadd( b.block_param( exit, 0 ), b.block_param( exit, 1 ) ) );
                                                     } );
    for ( int64_t n { -2 }; n < 40; ++n )
        for ( const int64_t s : { int64_t { 0 }, int64_t { 3 }, int64_t { -11 } } )
        {
            CAPTURE( n, s );
            CHECK( f( n, s ) == std::max< int64_t >( n, 0 ) * s + 5 );
        }
}

TEST_CASE( "loop carrying nine values with cross dependencies", "[jit][control]" )
{
    constexpr int width = 9;
    auto          f     = compile< uint64_t( uint64_t, uint64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                                     []( ir::IRBuilder& b, const Args& a )
                                                                     {
                                                            std::vector< Type > params( width + 1, Type::i64 );
                                                            const auto          hdr = b.add_block( std::span< const Type > { params } );
                                                            const auto          exit = b.add_block( { Type::i64 } );
                                                            Args                init { a[ 0 ] };
                                                            for ( int j { 0 }; j < width; ++j )
                                                                init.push_back( b.iadd( a[ 1 ], k64( b, j * 17 ) ) );
                                                            b.br( hdr, init );
                                                            b.switch_to_block( hdr );
                                                            const auto n = b.block_param( hdr, 0 );
                                                            Args       v;
                                                            for ( int j { 0 }; j < width; ++j )
                                                                v.push_back( b.block_param( hdr, static_cast< uint16_t >( j + 1 ) ) );
                                                            Args next { b.isub( n, k64( b, 1 ) ) };
                                                            for ( int j { 0 }; j < width; ++j )
                                                                next.push_back(
                                                                    b.iadd( v[ j ], b.imul( v[ ( j + 1 ) % width ], k64( b, j + 1 ) ) ) );
                                                            auto sum = v[ 0 ];
                                                            for ( int j { 1 }; j < width; ++j )
                                                                sum = b.bxor( b.imul( sum, k64( b, 31 ) ), v[ j ] );
                                                            b.brif( b.icmp( CC::ne, n, k64( b, 0 ) ), hdr, next, exit, Args { sum } );
                                                            b.switch_to_block( exit );
                                                            b.ret( b.block_param( exit, 0 ) );
                                                        } );
    for ( uint64_t n { 0 }; n < 30; ++n )
        for ( const uint64_t seed : { uint64_t { 0 }, uint64_t { 1 }, uint64_t { 0xfeedface12345678ull } } )
        {
            std::array< uint64_t, width > v { };
            for ( int j { 0 }; j < width; ++j ) v[ j ] = seed + static_cast< uint64_t >( j * 17 );
            uint64_t k = n;
            uint64_t sum { };
            for ( ;; )
            {
                sum = v[ 0 ];
                for ( int j { 1 }; j < width; ++j ) sum = ( sum * 31 ) ^ v[ j ];
                if ( k == 0 ) break;
                std::array< uint64_t, width > nv { };
                for ( int j { 0 }; j < width; ++j ) nv[ j ] = v[ j ] + v[ ( j + 1 ) % width ] * static_cast< uint64_t >( j + 1 );
                v = nv;
                --k;
            }
            CAPTURE( n, seed );
            CHECK( f( n, seed ) == sum );
        }
}

TEST_CASE( "loop with a constant block arg and a loop invariant value", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >(
        Type::i64, { Type::i64, Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            const auto inv  = b.imul( a[ 1 ], k64( b, 3 ) );
            const auto hdr  = b.add_block( { Type::i64, Type::i64, Type::i64 } );
            const auto exit = b.add_block( { Type::i64 } );
            b.br( hdr, Args { a[ 0 ], k64( b, 0 ), k64( b, 0 ) } );
            b.switch_to_block( hdr );
            const auto n   = b.block_param( hdr, 0 );
            const auto acc = b.block_param( hdr, 1 );
            const auto c   = b.block_param( hdr, 2 );
            b.brif( b.icmp( CC::sgt, n, k64( b, 0 ) ), hdr, Args { b.isub( n, k64( b, 1 ) ), b.iadd( b.iadd( acc, inv ), c ), k64( b, 7 ) },
                    exit, Args { acc } );
            b.switch_to_block( exit );
            b.ret( b.block_param( exit, 0 ) );
        } );
    for ( int64_t n { 0 }; n < 50; ++n )
        for ( const int64_t s : { int64_t { 0 }, int64_t { 2 }, int64_t { -9 } } )
        {
            const int64_t expected = n > 0 ? n * s * 3 + ( n - 1 ) * 7 : 0;
            CAPTURE( n, s );
            CHECK( f( n, s ) == expected );
        }
}

TEST_CASE( "control flow results agree with the interpreter", "[jit][control]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >(
        Type::i64, { Type::i64, Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            const auto hdr  = b.add_block( { Type::i64, Type::i64, Type::i64 } );
            const auto odd  = b.add_block( { } );
            const auto even = b.add_block( { } );
            const auto join = b.add_block( { Type::i64 } );
            const auto exit = b.add_block( { Type::i64 } );
            b.br( hdr, Args { k64( b, 0 ), a[ 0 ], a[ 1 ] } );
            b.switch_to_block( hdr );
            const auto i = b.block_param( hdr, 0 );
            const auto x = b.block_param( hdr, 1 );
            const auto y = b.block_param( hdr, 2 );
            b.brif( b.icmp( CC::uge, i, k64( b, 37 ) ), exit, Args { b.bxor( x, y ) }, odd, { } );
            b.switch_to_block( odd );
            b.brif( b.band( i, k64( b, 1 ) ), join, Args { b.iadd( x, b.sshr( y, k64( b, 3 ) ) ) }, even, { } );
            b.switch_to_block( even );
            b.br( join, Args { b.imul( x, k64( b, 0x9e3779b97f4a7c15LL ) ) } );
            b.switch_to_block( join );
            const auto nx = b.block_param( join, 0 );
            b.br( hdr, Args { b.iadd( i, k64( b, 1 ) ), b.bor( b.ishl( nx, k64( b, 1 ) ), b.ushr( nx, k64( b, 63 ) ) ), b.isub( y, nx ) } );
            b.switch_to_block( exit );
            b.ret( b.block_param( exit, 0 ) );
        } );
    for ( const auto x : edge_values( ) )
        for ( const auto y : edge_values( ) )
        {
            CAPTURE( x, y );
            CHECK( u( f( x, y ) ) == oracle( f, { u( x ), u( y ) } ) );
        }
}
