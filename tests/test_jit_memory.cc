#include "support.hh"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <numeric>
#include <vector>

using namespace xt;

namespace
{
    using CC = ir::ICmpCC;

    constexpr std::array< Type, 5 > mem_types { Type::i8, Type::i16, Type::i32, Type::i64, Type::ptr };

    uint64_t u( const int64_t v )
    { return static_cast< uint64_t >( v ); }

    ValueRef k64( ir::IRBuilder& b, const int64_t v )
    { return b.iconst( Type::i64, u( v ) ); }

    std::size_t bytes_of( const Type ty )
    { return ir::bit_width( ty ) / 8; }

    uint64_t read( const uint8_t* p, const std::size_t n )
    {
        uint64_t v { 0 };
        std::memcpy( &v, p, n );
        return v;
    }

    std::vector< uint8_t > pattern( const std::size_t n, const uint8_t seed = 0x5b )
    {
        std::vector< uint8_t > out( n );
        for ( std::size_t i { 0 }; i < n; ++i ) out[ i ] = static_cast< uint8_t >( i * 151 + seed + ( i >> 8 ) * 7 );
        return out;
    }

    Compiled< uint64_t( const void* ) > loader( const Type ty, const int32_t offset )
    {
        return compile< uint64_t( const void* ) >( ty == Type::ptr ? Type::ptr : Type::i64, { Type::ptr },
                                                   [ & ]( ir::IRBuilder& b, const Args& a )
                                                   {
                                                       const auto v = b.load( ty, a[ 0 ], offset );
                                                       b.ret( ty == Type::ptr || ty == Type::i64 ? v : b.uextend( Type::i64, v ) );
                                                   } );
    }

    Compiled< void( void*, uint64_t ) > storer( const Type ty, const int32_t offset )
    {
        return compile< void( void*, uint64_t ) >( Type::unit, { Type::ptr, ty == Type::ptr ? Type::ptr : Type::i64 },
                                                   [ & ]( ir::IRBuilder& b, const Args& a )
                                                   {
                                                       const auto v = ty == Type::ptr || ty == Type::i64 ? a[ 1 ] : b.ireduce( ty, a[ 1 ] );
                                                       b.store( v, a[ 0 ], offset );
                                                       b.ret( );
                                                   } );
    }

    const std::vector< int32_t >& offsets( )
    {
        static const std::vector< int32_t > values { -4096, -1000, -256, -129, -128, -127, -9,  -8,  -1,  0,   1,    2,
                                                     3,     7,     8,    63,   64,   127,  128, 129, 255, 256, 1000, 4000 };
        return values;
    }
}

TEST_CASE( "loads of every width at offset zero", "[jit][memory]" )
{
    const auto buf = pattern( 64 );
    for ( const auto ty : mem_types )
    {
        auto f = loader( ty, 0 );
        for ( std::size_t at { 0 }; at + 8 <= buf.size( ); ++at )
        {
            CAPTURE( ty, at );
            CHECK( f( buf.data( ) + at ) == read( buf.data( ) + at, bytes_of( ty ) ) );
        }
    }
}

TEST_CASE( "loads of every width at small positive and negative offsets", "[jit][memory]" )
{
    const auto buf  = pattern( 16384 );
    const auto base = buf.data( ) + 8192;
    for ( const auto ty : mem_types )
        for ( const auto off : offsets( ) )
        {
            auto f = loader( ty, off );
            CAPTURE( ty, off );
            CHECK( f( base ) == read( base + off, bytes_of( ty ) ) );
            CHECK( f( base + 3 ) == read( base + 3 + off, bytes_of( ty ) ) );
        }
}

TEST_CASE( "loads with displacements around a mebibyte", "[jit][memory]" )
{
    const auto buf  = pattern( std::size_t { 3 } << 20, 0x11 );
    const auto base = buf.data( ) + ( std::size_t { 3 } << 19 );
    for ( const auto ty : mem_types )
        for ( const int32_t off :
              { -( 1 << 20 ), -( 1 << 20 ) + 1, -( 1 << 19 ) - 3, -65536, 65535, 65536, ( 1 << 19 ) + 5, ( 1 << 20 ) - 8, 1 << 20 } )
        {
            auto f = loader( ty, off );
            CAPTURE( ty, off );
            CHECK( f( base ) == read( base + off, bytes_of( ty ) ) );
        }
}

TEST_CASE( "narrow loads sign extend correctly when requested", "[jit][memory]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32 } )
    {
        auto f = compile< int64_t( const void* ) >( Type::i64, { Type::ptr },
                                                    [ & ]( ir::IRBuilder& b, const Args& a )
                                                    {
                                                        b.ret( b.sextend( Type::i64, b.load( ty, a[ 0 ] ) ) );
                                                    } );
        for ( const auto v : edge_values( ) )
        {
            const auto raw = u( v );
            CAPTURE( ty, v );
            CHECK( u( f( &raw ) ) == sext( ty, mask( ty, raw ) ) );
        }
    }
}

TEST_CASE( "narrow loads zero the upper bits of the register", "[jit][memory]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32 } )
    {
        auto           f   = compile< uint64_t( const void* ) >( ty, { Type::ptr },
                                                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                                                 {
                                                         b.ret( b.load( ty, a[ 0 ] ) );
                                                                 } );
        const uint64_t all = ~uint64_t { 0 };
        CAPTURE( ty );
        CHECK( f( &all ) == mask( ty, all ) );
    }
}

TEST_CASE( "narrow loads returned with narrow native types", "[jit][memory]" )
{
    auto f8  = compile< int8_t( const void* ) >( Type::i8, { Type::ptr },
                                                 []( ir::IRBuilder& b, const Args& a )
                                                 {
                                                    b.ret( b.load( Type::i8, a[ 0 ], 1 ) );
                                                 } );
    auto f16 = compile< int16_t( const void* ) >( Type::i16, { Type::ptr },
                                                  []( ir::IRBuilder& b, const Args& a )
                                                  {
                                                      b.ret( b.load( Type::i16, a[ 0 ], 2 ) );
                                                  } );
    auto f32 = compile< int32_t( const void* ) >( Type::i32, { Type::ptr },
                                                  []( ir::IRBuilder& b, const Args& a )
                                                  {
                                                      b.ret( b.load( Type::i32, a[ 0 ], 4 ) );
                                                  } );
    for ( const auto v : edge_values( ) )
    {
        const std::array< uint64_t, 2 > raw { u( v ), ~u( v ) };
        const auto*                     p = reinterpret_cast< const uint8_t* >( raw.data( ) );
        CAPTURE( v );
        CHECK( f8( p ) == static_cast< int8_t >( read( p + 1, 1 ) ) );
        CHECK( f16( p ) == static_cast< int16_t >( read( p + 2, 2 ) ) );
        CHECK( f32( p ) == static_cast< int32_t >( read( p + 4, 4 ) ) );
    }
}

TEST_CASE( "stores write exactly their width and leave neighbours untouched", "[jit][memory]" )
{
    for ( const auto ty : mem_types )
        for ( const int32_t off : { 0, 1, 5, 8, 13 } )
        {
            auto f = storer( ty, off );
            for ( const auto v : edge_values( ) )
            {
                std::array< uint8_t, 40 > buf;
                buf.fill( 0xaa );
                f( buf.data( ) + 8, u( v ) );
                const auto n = bytes_of( ty );
                CAPTURE( ty, off, v );
                for ( std::size_t i { 0 }; i < buf.size( ); ++i )
                {
                    const auto rel = static_cast< int64_t >( i ) - 8 - off;
                    if ( rel >= 0 && rel < static_cast< int64_t >( n ) )
                        CHECK( buf[ i ] == static_cast< uint8_t >( u( v ) >> ( rel * 8 ) ) );
                    else
                        CHECK( buf[ i ] == 0xaa );
                }
            }
        }
}

TEST_CASE( "stores at small positive and negative offsets", "[jit][memory]" )
{
    for ( const auto ty : mem_types )
        for ( const auto off : offsets( ) )
        {
            auto                   f = storer( ty, off );
            std::vector< uint8_t > buf( 16384, 0xcd );
            const auto             base  = buf.data( ) + 8192;
            const uint64_t         value = 0x8877665544332211ull;
            f( base, value );
            CAPTURE( ty, off );
            CHECK( read( base + off, bytes_of( ty ) ) == mask( ty, value ) );
            CHECK( std::count( buf.begin( ), buf.end( ), uint8_t { 0xcd } )
                   == static_cast< std::ptrdiff_t >( buf.size( ) - bytes_of( ty ) ) );
        }
}

TEST_CASE( "stores with displacements around a mebibyte", "[jit][memory]" )
{
    std::vector< uint8_t > buf( std::size_t { 3 } << 20, 0 );
    const auto             base = buf.data( ) + ( std::size_t { 3 } << 19 );
    for ( const auto ty : mem_types )
        for ( const int32_t off : { -( 1 << 20 ), -65537, 65536, ( 1 << 20 ) - 8, 1 << 20 } )
        {
            auto           f     = storer( ty, off );
            const uint64_t value = 0x0123456789abcdefull ^ static_cast< uint64_t >( off );
            f( base, value );
            CAPTURE( ty, off );
            CHECK( read( base + off, bytes_of( ty ) ) == mask( ty, value ) );
            std::memset( base + off, 0, bytes_of( ty ) );
        }
    CHECK( std::all_of( buf.begin( ), buf.end( ),
                        []( const uint8_t c )
                        {
                            return c == 0;
                        } ) );
}

TEST_CASE( "store of constants of every width", "[jit][memory]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32, Type::i64 } )
        for ( const auto c : edge_values( ) )
        {
            auto                      f = compile< void( void* ) >( Type::unit, { Type::ptr },
                                                                    [ & ]( ir::IRBuilder& b, const Args& a )
                                                                    {
                                                   b.store( b.iconst( ty, mask( ty, u( c ) ) ), a[ 0 ], 3 );
                                                   b.ret( );
                                                                    } );
            std::array< uint8_t, 16 > buf;
            buf.fill( 0x77 );
            f( buf.data( ) );
            CAPTURE( ty, c );
            CHECK( read( buf.data( ) + 3, bytes_of( ty ) ) == mask( ty, u( c ) ) );
            CHECK( buf[ 2 ] == 0x77 );
            CHECK( buf[ 3 + bytes_of( ty ) ] == 0x77 );
        }
}

TEST_CASE( "store then load round trips through memory", "[jit][memory]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32, Type::i64 } )
    {
        auto f = compile< int64_t( void*, int64_t ) >( Type::i64, { Type::ptr, Type::i64 },
                                                       [ & ]( ir::IRBuilder& b, const Args& a )
                                                       {
                                                           const auto v = ty == Type::i64 ? a[ 1 ] : b.ireduce( ty, a[ 1 ] );
                                                           b.store( v, a[ 0 ], 16 );
                                                           const auto back = b.load( ty, a[ 0 ], 16 );
                                                           b.ret( ty == Type::i64 ? back : b.sextend( Type::i64, back ) );
                                                       } );
        for ( const auto v : edge_values( ) )
        {
            std::array< uint64_t, 4 > buf { };
            CAPTURE( ty, v );
            CHECK( u( f( buf.data( ), v ) ) == sext( ty, mask( ty, u( v ) ) ) );
        }
    }
}

TEST_CASE( "load modify store increments in place for every width", "[jit][memory]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32, Type::i64 } )
    {
        auto f = compile< void( void*, int64_t ) >( Type::unit, { Type::ptr, Type::i64 },
                                                    [ & ]( ir::IRBuilder& b, const Args& a )
                                                    {
                                                        const auto delta = ty == Type::i64 ? a[ 1 ] : b.ireduce( ty, a[ 1 ] );
                                                        const auto v     = b.load( ty, a[ 0 ], 8 );
                                                        b.store( b.iadd( v, delta ), a[ 0 ], 8 );
                                                        b.ret( );
                                                    } );
        for ( const auto start : edge_values( ) )
            for ( const int64_t d : { int64_t { 1 }, int64_t { -1 }, int64_t { 255 }, int64_t { 0x10001 } } )
            {
                std::array< uint64_t, 3 > buf { 0x1111111111111111ull, u( start ), 0x2222222222222222ull };
                f( buf.data( ), d );
                CAPTURE( ty, start, d );
                const auto n = ir::bit_width( ty );
                const auto expected
                    = n == 64 ? u( start ) + u( d ) : ( u( start ) & ~mask( ty, ~uint64_t { 0 } ) ) | mask( ty, u( start ) + u( d ) );
                CHECK( buf[ 1 ] == expected );
                CHECK( buf[ 0 ] == 0x1111111111111111ull );
                CHECK( buf[ 2 ] == 0x2222222222222222ull );
            }
    }
}

TEST_CASE( "pointer arithmetic indexes into an array before loading", "[jit][memory]" )
{
    auto f
        = compile< int64_t( const int64_t*, int64_t ) >( Type::i64, { Type::ptr, Type::i64 },
                                                         []( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             b.ret( b.load( Type::i64, b.iadd( a[ 0 ], b.ishl( a[ 1 ], k64( b, 3 ) ) ) ) );
                                                         } );
    auto g = compile< int64_t( const int64_t*, int64_t ) >(
        Type::i64, { Type::ptr, Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            b.ret( b.load( Type::i64, b.iadd( a[ 0 ], b.imul( a[ 1 ], k64( b, 8 ) ) ), -8 ) );
        } );
    std::vector< int64_t > arr( 64 );
    for ( std::size_t i { 0 }; i < arr.size( ); ++i ) arr[ i ] = static_cast< int64_t >( i * i * 1000003 ) - 5;
    for ( int64_t i { 0 }; i < 64; ++i )
    {
        CAPTURE( i );
        CHECK( f( arr.data( ), i ) == arr[ static_cast< std::size_t >( i ) ] );
        if ( i > 0 ) CHECK( g( arr.data( ), i ) == arr[ static_cast< std::size_t >( i - 1 ) ] );
    }
    CHECK( f( arr.data( ) + 32, -32 ) == arr[ 0 ] );
    CHECK( f( arr.data( ) + 32, -1 ) == arr[ 31 ] );
}

TEST_CASE( "pointers loaded from memory are dereferenced", "[jit][memory]" )
{
    auto                     f = compile< int64_t( const void* ) >( Type::i64, { Type::ptr },
                                                                    []( ir::IRBuilder& b, const Args& a )
                                                                    {
                                                    const auto p = b.load( Type::ptr, a[ 0 ], 8 );
                                                    const auto q = b.load( Type::ptr, p, 0 );
                                                    b.ret( b.load( Type::i64, q, 16 ) );
                                                                    } );
    std::array< int64_t, 4 > target { 1, 2, 777, 4 };
    std::array< void*, 1 >   middle { target.data( ) };
    std::array< void*, 2 >   outer { nullptr, middle.data( ) };
    CHECK( f( outer.data( ) ) == 777 );
}

TEST_CASE( "pointers are stored and read back by the host", "[jit][memory]" )
{
    auto                      f = compile< void( void*, void* ) >( Type::unit, { Type::ptr, Type::ptr },
                                                                   []( ir::IRBuilder& b, const Args& a )
                                                                   {
                                                  b.store( a[ 1 ], a[ 0 ] );
                                                  b.store( b.iadd( a[ 1 ], k64( b, 24 ) ), a[ 0 ], 8 );
                                                  b.ret( );
                                                                   } );
    std::array< void*, 2 >    slots { };
    std::array< uint8_t, 32 > target { };
    f( slots.data( ), target.data( ) );
    CHECK( slots[ 0 ] == target.data( ) );
    CHECK( slots[ 1 ] == target.data( ) + 24 );
}

TEST_CASE( "two pointers aliasing the same address observe each other's stores", "[jit][memory]" )
{
    auto                     f = compile< int64_t( void*, void* ) >( Type::i64, { Type::ptr, Type::ptr },
                                                                     []( ir::IRBuilder& b, const Args& a )
                                                                     {
                                                     b.store( k64( b, 5 ), a[ 0 ] );
                                                     b.store( k64( b, 9 ), a[ 1 ] );
                                                     const auto x = b.load( Type::i64, a[ 0 ] );
                                                     b.store( b.iadd( x, k64( b, 100 ) ), a[ 1 ] );
                                                     b.ret( b.load( Type::i64, a[ 0 ] ) );
                                                                     } );
    std::array< int64_t, 2 > buf { };
    CHECK( f( &buf[ 0 ], &buf[ 0 ] ) == 109 );
    CHECK( f( &buf[ 0 ], &buf[ 1 ] ) == 5 );
    CHECK( buf[ 1 ] == 105 );
}

TEST_CASE( "loop summing an i64 array", "[jit][memory]" )
{
    auto                   f = compile< int64_t( const int64_t*, int64_t ) >( Type::i64, { Type::ptr, Type::i64 },
                                                                              []( ir::IRBuilder& b, const Args& a )
                                                                              {
                                                                const auto hdr  = b.add_block( { Type::i64, Type::i64 } );
                                                                const auto body = b.add_block( { } );
                                                                const auto exit = b.add_block( { Type::i64 } );
                                                                b.br( hdr, Args { k64( b, 0 ), k64( b, 0 ) } );
                                                                b.switch_to_block( hdr );
                                                                const auto i   = b.block_param( hdr, 0 );
                                                                const auto acc = b.block_param( hdr, 1 );
                                                                b.brif( b.icmp( CC::slt, i, a[ 1 ] ), body, { }, exit, Args { acc } );
                                                                b.switch_to_block( body );
                                                                const auto v
                                                                    = b.load( Type::i64, b.iadd( a[ 0 ], b.ishl( i, k64( b, 3 ) ) ) );
                                                                b.br( hdr, Args { b.iadd( i, k64( b, 1 ) ), b.iadd( acc, v ) } );
                                                                b.switch_to_block( exit );
                                                                b.ret( b.block_param( exit, 0 ) );
                                                                              } );
    std::vector< int64_t > arr( 500 );
    for ( std::size_t i { 0 }; i < arr.size( ); ++i ) arr[ i ] = static_cast< int64_t >( i * 37 % 101 ) - 50;
    for ( const std::size_t n : { std::size_t { 0 }, std::size_t { 1 }, std::size_t { 2 }, std::size_t { 17 }, std::size_t { 500 } } )
    {
        CAPTURE( n );
        CHECK( f( arr.data( ), static_cast< int64_t >( n ) )
               == std::accumulate( arr.begin( ), arr.begin( ) + static_cast< std::ptrdiff_t >( n ), int64_t { 0 } ) );
    }
}

TEST_CASE( "loop walking a pointer through block args to sum bytes", "[jit][memory]" )
{
    auto f
        = compile< uint64_t( const uint8_t*, const uint8_t* ) >( Type::i64, { Type::ptr, Type::ptr },
                                                                 []( ir::IRBuilder& b, const Args& a )
                                                                 {
                                                                     const auto hdr  = b.add_block( { Type::ptr, Type::i64 } );
                                                                     const auto body = b.add_block( { } );
                                                                     const auto exit = b.add_block( { Type::i64 } );
                                                                     b.br( hdr, Args { a[ 0 ], k64( b, 0 ) } );
                                                                     b.switch_to_block( hdr );
                                                                     const auto p   = b.block_param( hdr, 0 );
                                                                     const auto acc = b.block_param( hdr, 1 );
                                                                     b.brif( b.icmp( CC::ult, p, a[ 1 ] ), body, { }, exit, Args { acc } );
                                                                     b.switch_to_block( body );
                                                                     const auto v = b.uextend( Type::i64, b.load( Type::i8, p ) );
                                                                     b.br( hdr, Args { b.iadd( p, k64( b, 1 ) ), b.iadd( acc, v ) } );
                                                                     b.switch_to_block( exit );
                                                                     b.ret( b.block_param( exit, 0 ) );
                                                                 } );
    const auto buf = pattern( 1000 );
    for ( const std::size_t n : { std::size_t { 0 }, std::size_t { 1 }, std::size_t { 255 }, std::size_t { 1000 } } )
    {
        CAPTURE( n );
        CHECK( f( buf.data( ), buf.data( ) + n )
               == std::accumulate( buf.begin( ), buf.begin( ) + static_cast< std::ptrdiff_t >( n ), uint64_t { 0 } ) );
    }
}

TEST_CASE( "loop reversing an i32 array in place", "[jit][memory]" )
{
    auto f = compile< void( int32_t*, int64_t ) >( Type::unit, { Type::ptr, Type::i64 },
                                                   []( ir::IRBuilder& b, const Args& a )
                                                   {
                                                       const auto hdr  = b.add_block( { Type::ptr, Type::ptr } );
                                                       const auto body = b.add_block( { } );
                                                       const auto exit = b.add_block( { } );
                                                       const auto last
                                                           = b.iadd( a[ 0 ], b.ishl( b.isub( a[ 1 ], k64( b, 1 ) ), k64( b, 2 ) ) );
                                                       b.br( hdr, Args { a[ 0 ], last } );
                                                       b.switch_to_block( hdr );
                                                       const auto lo = b.block_param( hdr, 0 );
                                                       const auto hi = b.block_param( hdr, 1 );
                                                       b.brif( b.icmp( CC::ult, lo, hi ), body, { }, exit, { } );
                                                       b.switch_to_block( body );
                                                       const auto x = b.load( Type::i32, lo );
                                                       const auto y = b.load( Type::i32, hi );
                                                       b.store( y, lo );
                                                       b.store( x, hi );
                                                       b.br( hdr, Args { b.iadd( lo, k64( b, 4 ) ), b.isub( hi, k64( b, 4 ) ) } );
                                                       b.switch_to_block( exit );
                                                       b.ret( );
                                                   } );
    for ( const int64_t n : { 1, 2, 3, 4, 5, 100, 101 } )
    {
        std::vector< int32_t > arr( static_cast< std::size_t >( n ) + 2 );
        for ( std::size_t i { 0 }; i < arr.size( ); ++i ) arr[ i ] = static_cast< int32_t >( i * 2654435761u );
        auto expected = arr;
        std::reverse( expected.begin( ) + 1, expected.end( ) - 1 );
        f( arr.data( ) + 1, n );
        CAPTURE( n );
        CHECK( arr == expected );
    }
}

TEST_CASE( "byte copy loop behaves like memcpy", "[jit][memory]" )
{
    auto f
        = compile< void( uint8_t*, const uint8_t*, int64_t ) >( Type::unit, { Type::ptr, Type::ptr, Type::i64 },
                                                                []( ir::IRBuilder& b, const Args& a )
                                                                {
                                                                    const auto hdr  = b.add_block( { Type::i64 } );
                                                                    const auto body = b.add_block( { } );
                                                                    const auto exit = b.add_block( { } );
                                                                    b.br( hdr, Args { k64( b, 0 ) } );
                                                                    b.switch_to_block( hdr );
                                                                    const auto i = b.block_param( hdr, 0 );
                                                                    b.brif( b.icmp( CC::ult, i, a[ 2 ] ), body, { }, exit, { } );
                                                                    b.switch_to_block( body );
                                                                    b.store( b.load( Type::i8, b.iadd( a[ 1 ], i ) ), b.iadd( a[ 0 ], i ) );
                                                                    b.br( hdr, Args { b.iadd( i, k64( b, 1 ) ) } );
                                                                    b.switch_to_block( exit );
                                                                    b.ret( );
                                                                } );
    const auto src = pattern( 777, 0x3c );
    for ( const std::size_t n : { std::size_t { 0 }, std::size_t { 1 }, std::size_t { 7 }, std::size_t { 776 } } )
    {
        std::vector< uint8_t > dst( 777, 0xee );
        f( dst.data( ), src.data( ), static_cast< int64_t >( n ) );
        CAPTURE( n );
        CHECK( std::equal( dst.begin( ), dst.begin( ) + static_cast< std::ptrdiff_t >( n ), src.begin( ) ) );
        CHECK( std::all_of( dst.begin( ) + static_cast< std::ptrdiff_t >( n ), dst.end( ),
                            []( const uint8_t c )
                            {
                                return c == 0xee;
                            } ) );
    }
}

TEST_CASE( "loop finding the signed maximum of an i32 array", "[jit][memory]" )
{
    auto f = compile< int64_t( const int32_t*, int64_t ) >(
        Type::i64, { Type::ptr, Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            const auto hdr  = b.add_block( { Type::i64, Type::i64 } );
            const auto body = b.add_block( { } );
            const auto exit = b.add_block( { Type::i64 } );
            b.br( hdr, Args { k64( b, 0 ), k64( b, std::numeric_limits< int64_t >::min( ) ) } );
            b.switch_to_block( hdr );
            const auto i  = b.block_param( hdr, 0 );
            const auto mx = b.block_param( hdr, 1 );
            b.brif( b.icmp( CC::slt, i, a[ 1 ] ), body, { }, exit, Args { mx } );
            b.switch_to_block( body );
            const auto v = b.sextend( Type::i64, b.load( Type::i32, b.iadd( a[ 0 ], b.imul( i, k64( b, 4 ) ) ) ) );
            b.br( hdr, Args { b.iadd( i, k64( b, 1 ) ), b.select( b.icmp( CC::sgt, v, mx ), v, mx ) } );
            b.switch_to_block( exit );
            b.ret( b.block_param( exit, 0 ) );
        } );
    std::vector< int32_t > arr { -5, -100, std::numeric_limits< int32_t >::min( ), -7, 3, -2,
                                 99, 42,   std::numeric_limits< int32_t >::max( ), 0 };
    for ( std::size_t n { 0 }; n <= arr.size( ); ++n )
    {
        const int64_t expected = n == 0 ? std::numeric_limits< int64_t >::min( )
                                        : *std::max_element( arr.begin( ), arr.begin( ) + static_cast< std::ptrdiff_t >( n ) );
        CAPTURE( n );
        CHECK( f( arr.data( ), static_cast< int64_t >( n ) ) == expected );
    }
}

TEST_CASE( "loop computing prefix sums in place", "[jit][memory]" )
{
    auto                   f = compile< void( int64_t*, int64_t ) >( Type::unit, { Type::ptr, Type::i64 },
                                                                     []( ir::IRBuilder& b, const Args& a )
                                                                     {
                                                       const auto hdr  = b.add_block( { Type::ptr, Type::i64, Type::i64 } );
                                                       const auto body = b.add_block( { } );
                                                       const auto exit = b.add_block( { } );
                                                       b.br( hdr, Args { a[ 0 ], a[ 1 ], k64( b, 0 ) } );
                                                       b.switch_to_block( hdr );
                                                       const auto p   = b.block_param( hdr, 0 );
                                                       const auto n   = b.block_param( hdr, 1 );
                                                       const auto acc = b.block_param( hdr, 2 );
                                                       b.brif( n, body, { }, exit, { } );
                                                       b.switch_to_block( body );
                                                       const auto s = b.iadd( acc, b.load( Type::i64, p ) );
                                                       b.store( s, p );
                                                       b.br( hdr, Args { b.iadd( p, k64( b, 8 ) ), b.isub( n, k64( b, 1 ) ), s } );
                                                       b.switch_to_block( exit );
                                                       b.ret( );
                                                                     } );
    std::vector< int64_t > arr( 200 );
    for ( std::size_t i { 0 }; i < arr.size( ); ++i ) arr[ i ] = static_cast< int64_t >( i % 7 ) - 3 + static_cast< int64_t >( i ) * 1000;
    auto expected = arr;
    std::partial_sum( expected.begin( ), expected.end( ), expected.begin( ) );
    f( arr.data( ), static_cast< int64_t >( arr.size( ) ) );
    CHECK( arr == expected );
}

TEST_CASE( "straight line copy of sixty four words through registers", "[jit][memory]" )
{
    constexpr int          n = 64;
    auto                   f = compile< void( int64_t*, const int64_t* ) >( Type::unit, { Type::ptr, Type::ptr },
                                                                            []( ir::IRBuilder& b, const Args& a )
                                                                            {
                                                              Args loaded;
                                                              for ( int k { 0 }; k < n; ++k )
                                                                  loaded.push_back( b.load( Type::i64, a[ 1 ], k * 8 ) );
                                                              for ( int k { 0 }; k < n; ++k )
                                                                  b.store( b.iadd( loaded[ n - 1 - k ], k64( b, k ) ), a[ 0 ], k * 8 );
                                                              b.ret( );
                                                                            } );
    std::vector< int64_t > src( n ), dst( n, 0 );
    for ( int k { 0 }; k < n; ++k ) src[ k ] = static_cast< int64_t >( k ) * 0x0101010101010101LL - 12345;
    f( dst.data( ), src.data( ) );
    for ( int k { 0 }; k < n; ++k )
    {
        CAPTURE( k );
        CHECK( dst[ k ] == src[ n - 1 - k ] + k );
    }
}

TEST_CASE( "mixed width memory traffic agrees with the interpreter", "[jit][memory]" )
{
    auto        f  = compile< int64_t( void*, int64_t ) >( Type::i64, { Type::ptr, Type::i64 },
                                                           []( ir::IRBuilder& b, const Args& a )
                                                           {
                                                       const auto p = a[ 0 ];
                                                       const auto x = a[ 1 ];
                                                       b.store( x, p, 0 );
                                                       b.store( b.ireduce( Type::i8, b.ushr( x, k64( b, 7 ) ) ), p, 3 );
                                                       b.store( b.ireduce( Type::i16, b.imul( x, k64( b, 3 ) ) ), p, 9 );
                                                       b.store( b.ireduce( Type::i32, b.bnot( x ) ), p, 13 );
                                                       const auto a8  = b.sextend( Type::i64, b.load( Type::i8, p, 1 ) );
                                                       const auto a16 = b.uextend( Type::i64, b.load( Type::i16, p, 2 ) );
                                                       const auto a32 = b.sextend( Type::i64, b.load( Type::i32, p, 11 ) );
                                                       const auto a64 = b.load( Type::i64, p, 8 );
                                                       b.store( b.iadd( a8, a64 ), p, 24 );
                                                       b.ret( b.bxor( b.iadd( a16, a32 ), b.load( Type::i64, p, 24 ) ) );
                                                           } );
    const auto& fn = *f.jit->irm.get( ir::FunctionRef { 0 } );
    for ( const auto x : edge_values( ) )
    {
        std::array< uint8_t, 40 > jb;
        jb.fill( 0x3e );
        auto           ib     = jb;
        const auto     r      = f( jb.data( ), x );
        const uint64_t args[] = { reinterpret_cast< uint64_t >( ib.data( ) ), u( x ) };
        CAPTURE( x );
        CHECK( u( r ) == Interp { }.run( fn, args ) );
        CHECK( jb == ib );
    }
}
