#include "support.hh"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <format>
#include <limits>
#include <utility>
#include <vector>

using namespace xt;

namespace
{
    using CC = ir::ICmpCC;

    constexpr CC all_ccs[] = { CC::eq, CC::ne, CC::slt, CC::sle, CC::sgt, CC::sge, CC::ult, CC::ule, CC::ugt, CC::uge };

    uint64_t u( const int64_t v )
    { return static_cast< uint64_t >( v ); }

    bool eval( const CC cc, const int64_t a, const int64_t b )
    {
        const auto ua = u( a );
        const auto ub = u( b );
        switch ( cc )
        {
            case CC::eq: return a == b;
            case CC::ne: return a != b;
            case CC::slt: return a < b;
            case CC::sle: return a <= b;
            case CC::sgt: return a > b;
            case CC::sge: return a >= b;
            case CC::ult: return ua < ub;
            case CC::ule: return ua <= ub;
            case CC::ugt: return ua > ub;
            case CC::uge: return ua >= ub;
        }
        return false;
    }

    const std::vector< int64_t >& cmp_constants( )
    {
        static const std::vector< int64_t > values { 0,
                                                     1,
                                                     -1,
                                                     42,
                                                     127,
                                                     128,
                                                     -128,
                                                     -129,
                                                     0x7fffffff,
                                                     -0x7fffffffLL - 1,
                                                     0x80000000LL,
                                                     0xffffffffLL,
                                                     -0x80000001LL,
                                                     std::numeric_limits< int64_t >::max( ),
                                                     std::numeric_limits< int64_t >::min( ) };
        return values;
    }

    void branch_on( ir::IRBuilder& b, const ValueRef cond, const int64_t yes, const int64_t no )
    {
        const auto t = b.add_block( { } );
        const auto e = b.add_block( { } );
        b.brif( cond, t, { }, e, { } );
        b.switch_to_block( t );
        b.ret( b.iconst( Type::i64, u( yes ) ) );
        b.switch_to_block( e );
        b.ret( b.iconst( Type::i64, u( no ) ) );
    }

    ValueRef narrowed( ir::IRBuilder& b, const Type ty, const ValueRef v )
    { return ty == Type::i64 ? v : b.ireduce( ty, v ); }

    int64_t narrow_value( const Type ty, const int64_t v )
    { return static_cast< int64_t >( sext( ty, mask( ty, u( v ) ) ) ); }

    const std::vector< int64_t >& narrow_constants( )
    {
        static const std::vector< int64_t > values { 0,      1,      -1,     42,     127,        -128,        128,        255,
                                                     300,    -300,   0x7fff, 0x8000, 0xffff,     0x7fffffff,  0x80000000, 0xffffffff };
        return values;
    }

    template < typename T >
    void check_native_narrow_params( const Type ty )
    {
        for ( const auto cc : all_ccs )
        {
            auto v = compile< int64_t( T, T ) >( Type::i64, { ty, ty },
                                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                                 {
                                                     b.ret( b.uextend( Type::i64, b.icmp( cc, a[ 0 ], a[ 1 ] ) ) );
                                                 } );
            auto j = compile< int64_t( T, T ) >( Type::i64, { ty, ty },
                                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                                 {
                                                     branch_on( b, b.icmp( cc, a[ 0 ], a[ 1 ] ), 1, 2 );
                                                 } );
            for ( const auto x : edge_values( ) )
                for ( const auto y : edge_values( ) )
                {
                    const auto tx = static_cast< T >( x );
                    const auto ty_ = static_cast< T >( y );
                    const bool want = eval( cc, static_cast< int64_t >( tx ), static_cast< int64_t >( ty_ ) );
                    CAPTURE( ty, cc, x, y );
                    CHECK( v( tx, ty_ ) == ( want ? 1 : 0 ) );
                    CHECK( j( tx, ty_ ) == ( want ? 1 : 2 ) );
                }
        }
    }
}

TEST_CASE( "icmp as a value for every condition over edge values", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        DYNAMIC_SECTION( std::format( "{}", cc ) )
        {
            auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                             [ & ]( ir::IRBuilder& b, const Args& a )
                                                             {
                                                                 b.ret( b.uextend( Type::i64, b.icmp( cc, a[ 0 ], a[ 1 ] ) ) );
                                                             } );
            for ( const auto x : edge_values( ) )
                for ( const auto y : edge_values( ) )
                {
                    CAPTURE( x, y );
                    CHECK( f( x, y ) == ( eval( cc, x, y ) ? 1 : 0 ) );
                }
        }
    }
}

TEST_CASE( "icmp returned directly as i8", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        auto f = compile< int8_t( int64_t, int64_t ) >( Type::i8, { Type::i64, Type::i64 },
                                                        [ & ]( ir::IRBuilder& b, const Args& a )
                                                        {
                                                            b.ret( b.icmp( cc, a[ 0 ], a[ 1 ] ) );
                                                        } );
        for ( const auto x : edge_values( ) )
            for ( const auto y : edge_values( ) )
            {
                CAPTURE( cc, x, y );
                CHECK( f( x, y ) == ( eval( cc, x, y ) ? 1 : 0 ) );
            }
    }
}

TEST_CASE( "icmp fused into brif for every condition over edge values", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        DYNAMIC_SECTION( std::format( "{}", cc ) )
        {
            auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                             [ & ]( ir::IRBuilder& b, const Args& a )
                                                             {
                                                                 branch_on( b, b.icmp( cc, a[ 0 ], a[ 1 ] ), 7777, -3 );
                                                             } );
            for ( const auto x : edge_values( ) )
                for ( const auto y : edge_values( ) )
                {
                    CAPTURE( x, y );
                    CHECK( f( x, y ) == ( eval( cc, x, y ) ? 7777 : -3 ) );
                }
        }
    }
}

TEST_CASE( "icmp against a constant right operand", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        DYNAMIC_SECTION( std::format( "{}", cc ) )
        {
            for ( const auto c : cmp_constants( ) )
            {
                auto v = compile< int64_t( int64_t ) >(
                    Type::i64, { Type::i64 },
                    [ & ]( ir::IRBuilder& b, const Args& a )
                    {
                        b.ret( b.uextend( Type::i64, b.icmp( cc, a[ 0 ], b.iconst( Type::i64, u( c ) ) ) ) );
                    } );
                auto j = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                        [ & ]( ir::IRBuilder& b, const Args& a )
                                                        {
                                                            branch_on( b, b.icmp( cc, a[ 0 ], b.iconst( Type::i64, u( c ) ) ), 1, 2 );
                                                        } );
                for ( const auto x : edge_values( ) )
                {
                    CAPTURE( x, c );
                    CHECK( v( x ) == ( eval( cc, x, c ) ? 1 : 0 ) );
                    CHECK( j( x ) == ( eval( cc, x, c ) ? 1 : 2 ) );
                }
            }
        }
    }
}

TEST_CASE( "icmp against a constant left operand swaps the condition", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        DYNAMIC_SECTION( std::format( "{}", cc ) )
        {
            for ( const auto c : cmp_constants( ) )
            {
                auto v = compile< int64_t( int64_t ) >(
                    Type::i64, { Type::i64 },
                    [ & ]( ir::IRBuilder& b, const Args& a )
                    {
                        b.ret( b.uextend( Type::i64, b.icmp( cc, b.iconst( Type::i64, u( c ) ), a[ 0 ] ) ) );
                    } );
                auto j = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                        [ & ]( ir::IRBuilder& b, const Args& a )
                                                        {
                                                            branch_on( b, b.icmp( cc, b.iconst( Type::i64, u( c ) ), a[ 0 ] ), 1, 2 );
                                                        } );
                for ( const auto y : edge_values( ) )
                {
                    CAPTURE( c, y );
                    CHECK( v( y ) == ( eval( cc, c, y ) ? 1 : 0 ) );
                    CHECK( j( y ) == ( eval( cc, c, y ) ? 1 : 2 ) );
                }
            }
        }
    }
}

TEST_CASE( "icmp between two constants", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
        for ( const auto x : cmp_constants( ) )
            for ( const auto y :
                  { int64_t { 0 }, int64_t { -1 }, int64_t { 128 }, int64_t { 0x80000000LL }, std::numeric_limits< int64_t >::min( ) } )
            {
                auto v = compile< int64_t( ) >(
                    Type::i64, { },
                    [ & ]( ir::IRBuilder& b, const Args& )
                    {
                        b.ret( b.uextend( Type::i64, b.icmp( cc, b.iconst( Type::i64, u( x ) ), b.iconst( Type::i64, u( y ) ) ) ) );
                    } );
                auto j = compile< int64_t( ) >(
                    Type::i64, { },
                    [ & ]( ir::IRBuilder& b, const Args& )
                    {
                        branch_on( b, b.icmp( cc, b.iconst( Type::i64, u( x ) ), b.iconst( Type::i64, u( y ) ) ), 10, 20 );
                    } );
                CAPTURE( cc, x, y );
                CHECK( v( ) == ( eval( cc, x, y ) ? 1 : 0 ) );
                CHECK( j( ) == ( eval( cc, x, y ) ? 10 : 20 ) );
            }
}

TEST_CASE( "icmp of a value with itself", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        const bool expected = cc == CC::eq || cc == CC::sle || cc == CC::sge || cc == CC::ule || cc == CC::uge;
        auto       v        = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                             [ & ]( ir::IRBuilder& b, const Args& a )
                                                             {
                                                    b.ret( b.uextend( Type::i64, b.icmp( cc, a[ 0 ], a[ 0 ] ) ) );
                                                             } );
        auto       j        = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                             [ & ]( ir::IRBuilder& b, const Args& a )
                                                             {
                                                    branch_on( b, b.icmp( cc, a[ 0 ], a[ 0 ] ), 5, 6 );
                                                             } );
        for ( const auto x : edge_values( ) )
        {
            CAPTURE( cc, x );
            CHECK( v( x ) == ( expected ? 1 : 0 ) );
            CHECK( j( x ) == ( expected ? 5 : 6 ) );
        }
    }
}

TEST_CASE( "icmp between a pointer and an i64", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        auto f = compile< int64_t( uint64_t, int64_t ) >( Type::i64, { Type::ptr, Type::i64 },
                                                          [ & ]( ir::IRBuilder& b, const Args& a )
                                                          {
                                                              b.ret( b.uextend( Type::i64, b.icmp( cc, a[ 0 ], a[ 1 ] ) ) );
                                                          } );
        auto g = compile< int64_t( uint64_t, uint64_t ) >( Type::i64, { Type::ptr, Type::ptr },
                                                           [ & ]( ir::IRBuilder& b, const Args& a )
                                                           {
                                                               branch_on( b, b.icmp( cc, a[ 0 ], a[ 1 ] ), 1, 0 );
                                                           } );
        for ( const auto x : edge_values( ) )
            for ( const auto y : edge_values( ) )
            {
                CAPTURE( cc, x, y );
                CHECK( f( u( x ), y ) == ( eval( cc, x, y ) ? 1 : 0 ) );
                CHECK( g( u( x ), u( y ) ) == ( eval( cc, x, y ) ? 1 : 0 ) );
            }
    }
}

TEST_CASE( "icmp of extended narrow values", "[jit][cmp]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32 } )
        for ( const auto cc : all_ccs )
        {
            auto s = compile< int64_t( int64_t, int64_t ) >(
                Type::i64, { Type::i64, Type::i64 },
                [ & ]( ir::IRBuilder& b, const Args& a )
                {
                    b.ret( b.uextend( Type::i64, b.icmp( cc, b.sextend( Type::i64, b.ireduce( ty, a[ 0 ] ) ),
                                                         b.sextend( Type::i64, b.ireduce( ty, a[ 1 ] ) ) ) ) );
                } );
            auto z = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                             [ & ]( ir::IRBuilder& b, const Args& a )
                                                             {
                                                                 branch_on( b,
                                                                            b.icmp( cc, b.uextend( Type::i64, b.ireduce( ty, a[ 0 ] ) ),
                                                                                    b.uextend( Type::i64, b.ireduce( ty, a[ 1 ] ) ) ),
                                                                            1, 0 );
                                                             } );
            for ( const auto x : edge_values( ) )
                for ( const auto y : edge_values( ) )
                {
                    const auto sx = static_cast< int64_t >( sext( ty, mask( ty, u( x ) ) ) );
                    const auto sy = static_cast< int64_t >( sext( ty, mask( ty, u( y ) ) ) );
                    const auto zx = static_cast< int64_t >( mask( ty, u( x ) ) );
                    const auto zy = static_cast< int64_t >( mask( ty, u( y ) ) );
                    CAPTURE( ty, cc, x, y );
                    CHECK( s( x, y ) == ( eval( cc, sx, sy ) ? 1 : 0 ) );
                    CHECK( z( x, y ) == ( eval( cc, zx, zy ) ? 1 : 0 ) );
                }
        }
}

TEST_CASE( "icmp on reduced narrow operands ignores the upper bits", "[jit][cmp]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32 } )
        for ( const auto cc : all_ccs )
        {
            auto v = compile< int64_t( int64_t, int64_t ) >(
                Type::i64, { Type::i64, Type::i64 },
                [ & ]( ir::IRBuilder& b, const Args& a )
                {
                    b.ret( b.uextend( Type::i64, b.icmp( cc, b.ireduce( ty, a[ 0 ] ), b.ireduce( ty, a[ 1 ] ) ) ) );
                } );
            auto j = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                             [ & ]( ir::IRBuilder& b, const Args& a )
                                                             {
                                                                 branch_on( b, b.icmp( cc, b.ireduce( ty, a[ 0 ] ), b.ireduce( ty, a[ 1 ] ) ), 1, 2 );
                                                             } );
            for ( const auto x : edge_values( ) )
                for ( const auto y : edge_values( ) )
                {
                    const bool want = eval( cc, narrow_value( ty, x ), narrow_value( ty, y ) );
                    CAPTURE( ty, cc, x, y );
                    CHECK( v( x, y ) == ( want ? 1 : 0 ) );
                    CHECK( j( x, y ) == ( want ? 1 : 2 ) );
                }
        }
}

TEST_CASE( "icmp on native narrow parameters", "[jit][cmp]" )
{
    SECTION( "i8" ) { check_native_narrow_params< int8_t >( Type::i8 ); }
    SECTION( "i16" ) { check_native_narrow_params< int16_t >( Type::i16 ); }
    SECTION( "i32" ) { check_native_narrow_params< int32_t >( Type::i32 ); }
}

TEST_CASE( "icmp narrow against a constant", "[jit][cmp]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32 } )
        for ( const auto cc : all_ccs )
        {
            DYNAMIC_SECTION( std::format( "{} {}", ty, cc ) )
            {
                for ( const auto c : narrow_constants( ) )
                    for ( const auto raw : { u( c ), mask( ty, u( c ) ) } )
                    {
                        auto r = compile< int64_t( int64_t ) >(
                            Type::i64, { Type::i64 },
                            [ & ]( ir::IRBuilder& b, const Args& a )
                            {
                                b.ret( b.uextend( Type::i64, b.icmp( cc, b.ireduce( ty, a[ 0 ] ), b.iconst( ty, raw ) ) ) );
                            } );
                        auto l = compile< int64_t( int64_t ) >(
                            Type::i64, { Type::i64 },
                            [ & ]( ir::IRBuilder& b, const Args& a )
                            {
                                b.ret( b.uextend( Type::i64, b.icmp( cc, b.iconst( ty, raw ), b.ireduce( ty, a[ 0 ] ) ) ) );
                            } );
                        auto j = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                                [ & ]( ir::IRBuilder& b, const Args& a )
                                                                {
                                                                    branch_on( b, b.icmp( cc, b.ireduce( ty, a[ 0 ] ), b.iconst( ty, raw ) ), 1, 2 );
                                                                } );
                        const auto nc = narrow_value( ty, c );
                        for ( const auto x : edge_values( ) )
                        {
                            const auto nx = narrow_value( ty, x );
                            CAPTURE( c, raw, x );
                            CHECK( r( x ) == ( eval( cc, nx, nc ) ? 1 : 0 ) );
                            CHECK( l( x ) == ( eval( cc, nc, nx ) ? 1 : 0 ) );
                            CHECK( j( x ) == ( eval( cc, nx, nc ) ? 1 : 2 ) );
                        }
                    }
            }
        }
}

TEST_CASE( "icmp between two narrow constants", "[jit][cmp]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32 } )
        for ( const auto cc : all_ccs )
            for ( const auto& [ c, d ] : { std::pair< int64_t, int64_t > { -1, 1 }, { 0x80, 0x7f }, { 0xffff, 0 }, { 0x80000000, 0x7fffffff } } )
            {
                auto f = compile< int64_t( ) >( Type::i64, { },
                                                [ & ]( ir::IRBuilder& b, const Args& )
                                                {
                                                    b.ret( b.uextend( Type::i64, b.icmp( cc, b.iconst( ty, u( c ) ), b.iconst( ty, u( d ) ) ) ) );
                                                } );
                CAPTURE( ty, cc, c, d );
                CHECK( f( ) == ( eval( cc, narrow_value( ty, c ), narrow_value( ty, d ) ) ? 1 : 0 ) );
            }
}

TEST_CASE( "select with an icmp condition picks the right operand", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                         [ & ]( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             b.ret( b.select( b.icmp( cc, a[ 0 ], a[ 1 ] ), a[ 0 ], a[ 1 ] ) );
                                                         } );
        auto g = compile< int64_t( int64_t, int64_t ) >(
            Type::i64, { Type::i64, Type::i64 },
            [ & ]( ir::IRBuilder& b, const Args& a )
            {
                b.ret( b.select( b.icmp( cc, a[ 0 ], b.iconst( Type::i64, 0 ) ), b.iconst( Type::i64, 0x123456789 ), a[ 1 ] ) );
            } );
        for ( const auto x : edge_values( ) )
            for ( const auto y : edge_values( ) )
            {
                CAPTURE( cc, x, y );
                CHECK( f( x, y ) == ( eval( cc, x, y ) ? x : y ) );
                CHECK( g( x, y ) == ( eval( cc, x, 0 ) ? 0x123456789 : y ) );
            }
    }
}

TEST_CASE( "select tests only the width of its condition", "[jit][cmp]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32, Type::i64 } )
    {
        DYNAMIC_SECTION( std::format( "{}", ty ) )
        {
            auto                   f     = compile< int64_t( int64_t, int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64, Type::i64 },
                                                                                            [ & ]( ir::IRBuilder& b, const Args& a )
                                                                                            {
                                                                          b.ret( b.select( narrowed( b, ty, a[ 0 ] ), a[ 1 ], a[ 2 ] ) );
                                                                                            } );
            std::vector< int64_t > conds = edge_values( );
            for ( const int64_t c : { 0x100LL, 0x10000LL, 0x100000000LL, 0x7f00LL, 0xff00000000LL, 0x0001000000000000LL } )
                conds.push_back( c );
            for ( const auto c : conds )
            {
                CAPTURE( c );
                CHECK( f( c, 11, 22 ) == ( mask( ty, u( c ) ) != 0 ? 11 : 22 ) );
                CHECK( f( c, -1, std::numeric_limits< int64_t >::min( ) )
                       == ( mask( ty, u( c ) ) != 0 ? -1 : std::numeric_limits< int64_t >::min( ) ) );
            }
        }
    }
}

TEST_CASE( "select of narrow values", "[jit][cmp]" )
{
    auto f8  = compile< int8_t( int64_t, int8_t, int8_t ) >( Type::i8, { Type::i64, Type::i8, Type::i8 },
                                                             []( ir::IRBuilder& b, const Args& a )
                                                             {
                                                                b.ret( b.select( a[ 0 ], a[ 1 ], a[ 2 ] ) );
                                                             } );
    auto f16 = compile< int16_t( int64_t, int16_t, int16_t ) >( Type::i16, { Type::i64, Type::i16, Type::i16 },
                                                                []( ir::IRBuilder& b, const Args& a )
                                                                {
                                                                    b.ret( b.select( a[ 0 ], a[ 1 ], a[ 2 ] ) );
                                                                } );
    auto f32 = compile< int32_t( int64_t, int32_t, int32_t ) >( Type::i32, { Type::i64, Type::i32, Type::i32 },
                                                                []( ir::IRBuilder& b, const Args& a )
                                                                {
                                                                    b.ret( b.select( a[ 0 ], a[ 1 ], a[ 2 ] ) );
                                                                } );
    auto p   = compile< uint64_t( int64_t, uint64_t, uint64_t ) >( Type::ptr, { Type::i64, Type::ptr, Type::ptr },
                                                                   []( ir::IRBuilder& b, const Args& a )
                                                                   {
                                                                     b.ret( b.select( a[ 0 ], a[ 1 ], a[ 2 ] ) );
                                                                   } );
    for ( const auto c : edge_values( ) )
        for ( const auto x : edge_values( ) )
        {
            const auto y = ~x;
            CAPTURE( c, x );
            CHECK( f8( c, static_cast< int8_t >( x ), static_cast< int8_t >( y ) ) == static_cast< int8_t >( c != 0 ? x : y ) );
            CHECK( f16( c, static_cast< int16_t >( x ), static_cast< int16_t >( y ) ) == static_cast< int16_t >( c != 0 ? x : y ) );
            CHECK( f32( c, static_cast< int32_t >( x ), static_cast< int32_t >( y ) ) == static_cast< int32_t >( c != 0 ? x : y ) );
            CHECK( p( c, u( x ), u( y ) ) == u( c != 0 ? x : y ) );
        }
}

TEST_CASE( "select with the same value on both sides and with constant operands", "[jit][cmp]" )
{
    auto same        = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                               []( ir::IRBuilder& b, const Args& a )
                                                               {
                                                            b.ret( b.select( a[ 0 ], a[ 1 ], a[ 1 ] ) );
                                                               } );
    auto cond_is_val = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                               []( ir::IRBuilder& b, const Args& a )
                                                               {
                                                                   b.ret( b.select( a[ 0 ], a[ 0 ], a[ 1 ] ) );
                                                               } );
    auto consts      = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                      []( ir::IRBuilder& b, const Args& a )
                                                      {
                                                     b.ret( b.select( a[ 0 ], b.iconst( Type::i64, 0xffffffff00000000ull ),
                                                                      b.iconst( Type::i64, static_cast< uint64_t >( -77 ) ) ) );
                                                      } );
    for ( const auto c : edge_values( ) )
        for ( const auto x : edge_values( ) )
        {
            CAPTURE( c, x );
            CHECK( same( c, x ) == x );
            CHECK( cond_is_val( c, x ) == ( c != 0 ? c : x ) );
        }
    for ( const auto c : edge_values( ) ) CHECK( u( consts( c ) ) == ( c != 0 ? 0xffffffff00000000ull : u( -77 ) ) );
}

TEST_CASE( "brif on a non icmp value of every width", "[jit][cmp]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32, Type::i64 } )
    {
        DYNAMIC_SECTION( std::format( "{}", ty ) )
        {
            auto                   f     = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                                          [ & ]( ir::IRBuilder& b, const Args& a )
                                                                          {
                                                        branch_on( b, narrowed( b, ty, a[ 0 ] ), 100, 200 );
                                                                          } );
            std::vector< int64_t > conds = edge_values( );
            for ( const int64_t c : { 0x100LL, 0x10000LL, 0x100000000LL, 0x8000LL, 0x80LL, 0x4000000000000000LL } ) conds.push_back( c );
            for ( const auto c : conds )
            {
                CAPTURE( c );
                CHECK( f( c ) == ( mask( ty, u( c ) ) != 0 ? 100 : 200 ) );
            }
        }
    }
}

TEST_CASE( "brif on native narrow parameters", "[jit][cmp]" )
{
    auto f8  = compile< int64_t( int8_t ) >( Type::i64, { Type::i8 },
                                             []( ir::IRBuilder& b, const Args& a )
                                             {
                                                branch_on( b, a[ 0 ], 1, 0 );
                                             } );
    auto f16 = compile< int64_t( int16_t ) >( Type::i64, { Type::i16 },
                                              []( ir::IRBuilder& b, const Args& a )
                                              {
                                                  branch_on( b, a[ 0 ], 1, 0 );
                                              } );
    auto f32 = compile< int64_t( int32_t ) >( Type::i64, { Type::i32 },
                                              []( ir::IRBuilder& b, const Args& a )
                                              {
                                                  branch_on( b, a[ 0 ], 1, 0 );
                                              } );
    auto fp  = compile< int64_t( uint64_t ) >( Type::i64, { Type::ptr },
                                               []( ir::IRBuilder& b, const Args& a )
                                               {
                                                  branch_on( b, a[ 0 ], 1, 0 );
                                               } );
    for ( const auto c : edge_values( ) )
    {
        CAPTURE( c );
        CHECK( f8( static_cast< int8_t >( c ) ) == ( static_cast< int8_t >( c ) != 0 ? 1 : 0 ) );
        CHECK( f16( static_cast< int16_t >( c ) ) == ( static_cast< int16_t >( c ) != 0 ? 1 : 0 ) );
        CHECK( f32( static_cast< int32_t >( c ) ) == ( static_cast< int32_t >( c ) != 0 ? 1 : 0 ) );
        CHECK( fp( u( c ) ) == ( c != 0 ? 1 : 0 ) );
    }
}

TEST_CASE( "brif on a constant condition", "[jit][cmp]" )
{
    for ( const int64_t c : { int64_t { 0 }, int64_t { 1 }, int64_t { -1 }, int64_t { 0x100000000LL } } )
    {
        auto f = compile< int64_t( ) >( Type::i64, { },
                                        [ & ]( ir::IRBuilder& b, const Args& )
                                        {
                                            branch_on( b, b.iconst( Type::i64, u( c ) ), 3, 4 );
                                        } );
        CHECK( f( ) == ( c != 0 ? 3 : 4 ) );
    }
}

TEST_CASE( "icmp used as a branch condition and a value is not fused", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                         [ & ]( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             const auto c = b.icmp( cc, a[ 0 ], a[ 1 ] );
                                                             const auto v = b.uextend( Type::i64, c );
                                                             const auto t = b.add_block( { Type::i64 } );
                                                             const auto e = b.add_block( { Type::i64 } );
                                                             b.brif( c, t, Args { v }, e, Args { v } );
                                                             b.switch_to_block( t );
                                                             b.ret( b.iadd( b.block_param( t, 0 ), b.iconst( Type::i64, 10 ) ) );
                                                             b.switch_to_block( e );
                                                             b.ret( b.iadd( b.block_param( e, 0 ), b.iconst( Type::i64, 20 ) ) );
                                                         } );
        for ( const auto x : edge_values( ) )
            for ( const auto y : edge_values( ) )
            {
                CAPTURE( cc, x, y );
                CHECK( f( x, y ) == ( eval( cc, x, y ) ? 11 : 20 ) );
            }
    }
}

TEST_CASE( "icmp used by two branches in different blocks", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                         [ & ]( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             const auto c  = b.icmp( cc, a[ 0 ], a[ 1 ] );
                                                             const auto m  = b.add_block( { } );
                                                             const auto t1 = b.add_block( { } );
                                                             const auto e1 = b.add_block( { } );
                                                             const auto t2 = b.add_block( { } );
                                                             const auto e2 = b.add_block( { } );
                                                             b.brif( c, m, { }, e1, { } );
                                                             b.switch_to_block( m );
                                                             b.brif( c, t1, { }, t2, { } );
                                                             b.switch_to_block( t1 );
                                                             b.ret( b.iconst( Type::i64, 1 ) );
                                                             b.switch_to_block( e1 );
                                                             b.brif( c, t2, { }, e2, { } );
                                                             b.switch_to_block( t2 );
                                                             b.ret( b.iconst( Type::i64, 2 ) );
                                                             b.switch_to_block( e2 );
                                                             b.ret( b.iconst( Type::i64, 3 ) );
                                                         } );
        for ( const auto x : edge_values( ) )
            for ( const auto y : edge_values( ) )
            {
                CAPTURE( cc, x, y );
                CHECK( f( x, y ) == ( eval( cc, x, y ) ? 1 : 3 ) );
            }
    }
}

TEST_CASE( "icmp defined in an earlier block feeds a brif", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                         [ & ]( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             const auto c   = b.icmp( cc, a[ 0 ], a[ 1 ] );
                                                             const auto mid = b.add_block( { } );
                                                             b.br( mid );
                                                             b.switch_to_block( mid );
                                                             branch_on( b, c, -10, -20 );
                                                         } );
        for ( const auto x : edge_values( ) )
            for ( const auto y : edge_values( ) )
            {
                CAPTURE( cc, x, y );
                CHECK( f( x, y ) == ( eval( cc, x, y ) ? -10 : -20 ) );
            }
    }
}

TEST_CASE( "fused icmp with flag clobbering instructions before the brif", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                         [ & ]( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             const auto c = b.icmp( cc, a[ 0 ], a[ 1 ] );
                                                             const auto s = b.iadd( a[ 0 ], a[ 1 ] );
                                                             const auto x = b.bxor( s, b.iconst( Type::i64, 0 ) );
                                                             const auto z = b.iconst( Type::i64, 0 );
                                                             const auto t = b.add_block( { Type::i64 } );
                                                             const auto e = b.add_block( { Type::i64 } );
                                                             b.brif( c, t, Args { x }, e, Args { z } );
                                                             b.switch_to_block( t );
                                                             b.ret( b.block_param( t, 0 ) );
                                                             b.switch_to_block( e );
                                                             b.ret( b.isub( b.block_param( e, 0 ), b.iconst( Type::i64, 1 ) ) );
                                                         } );
        for ( const auto x : edge_values( ) )
            for ( const auto y : edge_values( ) )
            {
                CAPTURE( cc, x, y );
                CHECK( f( x, y ) == ( eval( cc, x, y ) ? static_cast< int64_t >( u( x ) + u( y ) ) : -1 ) );
            }
    }
}

TEST_CASE( "fused icmp with arguments on both edges", "[jit][cmp]" )
{
    for ( const auto cc : all_ccs )
    {
        auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                         [ & ]( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             const auto t = b.add_block( { Type::i64, Type::i64 } );
                                                             const auto e = b.add_block( { Type::i64, Type::i64 } );
                                                             b.brif( b.icmp( cc, a[ 0 ], a[ 1 ] ), t, Args { a[ 1 ], a[ 0 ] }, e,
                                                                     Args { a[ 0 ], a[ 1 ] } );
                                                             b.switch_to_block( t );
                                                             b.ret( b.isub( b.block_param( t, 0 ), b.block_param( t, 1 ) ) );
                                                             b.switch_to_block( e );
                                                             b.ret( b.isub( b.block_param( e, 0 ), b.block_param( e, 1 ) ) );
                                                         } );
        for ( const auto x : edge_values( ) )
            for ( const auto y : edge_values( ) )
            {
                CAPTURE( cc, x, y );
                CHECK( u( f( x, y ) ) == ( eval( cc, x, y ) ? u( y ) - u( x ) : u( x ) - u( y ) ) );
            }
    }
}

TEST_CASE( "chained comparisons implement a three way compare", "[jit][cmp]" )
{
    for ( const bool is_signed : { true, false } )
    {
        auto f = compile< int64_t( int64_t, int64_t ) >(
            Type::i64, { Type::i64, Type::i64 },
            [ & ]( ir::IRBuilder& b, const Args& a )
            {
                const auto lt = b.uextend( Type::i64, b.icmp( is_signed ? CC::slt : CC::ult, a[ 0 ], a[ 1 ] ) );
                const auto gt = b.uextend( Type::i64, b.icmp( is_signed ? CC::sgt : CC::ugt, a[ 0 ], a[ 1 ] ) );
                b.ret( b.isub( gt, lt ) );
            } );
        for ( const auto x : edge_values( ) )
            for ( const auto y : edge_values( ) )
            {
                CAPTURE( is_signed, x, y );
                const int64_t expected = is_signed ? ( x > y ) - ( x < y ) : ( u( x ) > u( y ) ) - ( u( x ) < u( y ) );
                CHECK( f( x, y ) == expected );
            }
    }
}

TEST_CASE( "many icmp results combined with bitwise ops", "[jit][cmp]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     []( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         auto acc = b.iconst( Type::i64, 0 );
                                                         for ( std::size_t k { 0 }; k < std::size( all_ccs ); ++k )
                                                         {
                                                             const auto bit
                                                                 = b.uextend( Type::i64, b.icmp( all_ccs[ k ], a[ 0 ], a[ 1 ] ) );
                                                             acc = b.bor( acc, b.ishl( bit, b.iconst( Type::i64, k ) ) );
                                                         }
                                                         b.ret( acc );
                                                     } );
    for ( const auto x : edge_values( ) )
        for ( const auto y : edge_values( ) )
        {
            int64_t expected { 0 };
            for ( std::size_t k { 0 }; k < std::size( all_ccs ); ++k )
                if ( eval( all_ccs[ k ], x, y ) ) expected |= int64_t { 1 } << k;
            CAPTURE( x, y );
            CHECK( f( x, y ) == expected );
        }
}
