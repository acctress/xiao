#include "support.hh"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

using namespace xt;

namespace
{
    using BinBuild = ValueRef ( ir::IRBuilder::* )( ValueRef, ValueRef );

    constexpr auto i64_min = std::numeric_limits< int64_t >::min( );

    struct BinOp
    {
        std::string_view name;
        BinBuild         build;
        uint64_t ( *eval )( uint64_t, uint64_t );
        bool ( *traps )( uint64_t, uint64_t );
    };

    bool never( uint64_t, uint64_t )
    { return false; }

    bool signed_trap( const uint64_t a, const uint64_t b )
    { return b == 0 || ( static_cast< int64_t >( b ) == -1 && static_cast< int64_t >( a ) == i64_min ); }

    bool unsigned_trap( uint64_t, const uint64_t b )
    { return b == 0; }

    const std::vector< BinOp >& binops( )
    {
        static const std::vector< BinOp > ops {
            {"iadd", &ir::IRBuilder::iadd,
             []( uint64_t a, uint64_t b )
 {
 return a + b;
 },         never},
            {"isub", &ir::IRBuilder::isub,
             []( uint64_t a, uint64_t b )
 {
 return a - b;
 },         never},
            {"imul", &ir::IRBuilder::imul,
             []( uint64_t a, uint64_t b )
 {
 return a * b;
 },         never},
            {"band", &ir::IRBuilder::band,
             []( uint64_t a, uint64_t b )
 {
 return a & b;
 },         never},
            { "bor",  &ir::IRBuilder::bor,
             []( uint64_t a, uint64_t b )
 {
 return a | b;
 },         never},
            {"bxor", &ir::IRBuilder::bxor,
             []( uint64_t a, uint64_t b )
 {
 return a ^ b;
 },         never},
            {"idiv", &ir::IRBuilder::idiv,
             []( uint64_t a, uint64_t b )
 {
 return static_cast< uint64_t >( static_cast< int64_t >( a ) / static_cast< int64_t >( b ) );
 },   signed_trap},
            {"srem", &ir::IRBuilder::srem,
             []( uint64_t a, uint64_t b )
 {
 return static_cast< uint64_t >( static_cast< int64_t >( a ) % static_cast< int64_t >( b ) );
 },   signed_trap},
            {"udiv", &ir::IRBuilder::udiv,
             []( uint64_t a, uint64_t b )
 {
 return a / b;
 }, unsigned_trap},
            {"urem", &ir::IRBuilder::urem,
             []( uint64_t a, uint64_t b )
 {
 return a % b;
 }, unsigned_trap},
            {"ishl", &ir::IRBuilder::ishl,
             []( uint64_t a, uint64_t b )
 {
 return a << ( b & 63 );
 },         never},
            {"ushr", &ir::IRBuilder::ushr,
             []( uint64_t a, uint64_t b )
 {
 return a >> ( b & 63 );
 },         never},
            {"sshr", &ir::IRBuilder::sshr,
             []( uint64_t a, uint64_t b )
 {
 return static_cast< uint64_t >( static_cast< int64_t >( a ) >> ( b & 63 ) );
 },         never},
        };
        return ops;
    }

    const std::vector< BinOp >& narrow_binops( )
    {
        static const std::vector< BinOp > ops = []
        {
            std::vector< BinOp > out;
            for ( const auto& op : binops( ) )
                if ( op.name == "iadd" || op.name == "isub" || op.name == "imul" || op.name == "band" || op.name == "bor"
                     || op.name == "bxor" || op.name == "ishl" )
                    out.push_back( op );
            return out;
        }( );
        return ops;
    }

    const std::vector< int64_t >& shift_counts( )
    {
        static const std::vector< int64_t > counts { 0, 1, 2, 7, 8, 15, 16, 31, 32, 33, 62, 63, 64, 65, 127, 128, 255, 256, -1, -63, -64 };
        return counts;
    }

    const std::vector< int64_t >& small_constants( )
    {
        static const std::vector< int64_t > values {
            0, 1, -1, 5, -128, 127, 128, 0x7fffffff, -0x80000000LL, 0x80000000LL, 0xffffffffLL, std::numeric_limits< int64_t >::min( ) };
        return values;
    }

    uint64_t u( const int64_t v )
    { return static_cast< uint64_t >( v ); }

    const std::array< Type, 4 > int_types { Type::i8, Type::i16, Type::i32, Type::i64 };

    ValueRef narrowed( ir::IRBuilder& b, const Type ty, const ValueRef v )
    { return ty == Type::i64 ? v : b.ireduce( ty, v ); }

    ValueRef widened( ir::IRBuilder& b, const Type ty, const ValueRef v, const bool is_signed )
    {
        if ( ty == Type::i64 ) return v;
        return is_signed ? b.sextend( Type::i64, v ) : b.uextend( Type::i64, v );
    }
} // namespace

TEST_CASE( "binary ops with both operands in registers match the reference", "[jit][arith]" )
{
    for ( const auto& op : binops( ) )
    {
        DYNAMIC_SECTION( op.name )
        {
            auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                             [ & ]( ir::IRBuilder& b, const Args& a )
                                                             {
                                                                 b.ret( ( b.*op.build )( a[ 0 ], a[ 1 ] ) );
                                                             } );
            for ( const auto x : edge_values( ) )
                for ( const auto y : edge_values( ) )
                {
                    if ( op.traps( u( x ), u( y ) ) ) continue;
                    CAPTURE( x, y );
                    CHECK( u( f( x, y ) ) == op.eval( u( x ), u( y ) ) );
                }
        }
    }
}

TEST_CASE( "binary ops with a constant right operand match the reference", "[jit][arith]" )
{
    for ( const auto& op : binops( ) )
    {
        DYNAMIC_SECTION( op.name )
        {
            for ( const auto c : edge_values( ) )
            {
                if ( op.traps( 1, u( c ) ) ) continue;
                auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                        [ & ]( ir::IRBuilder& b, const Args& a )
                                                        {
                                                            b.ret( ( b.*op.build )( a[ 0 ], b.iconst( Type::i64, u( c ) ) ) );
                                                        } );
                for ( const auto x : edge_values( ) )
                {
                    if ( op.traps( u( x ), u( c ) ) ) continue;
                    CAPTURE( x, c );
                    CHECK( u( f( x ) ) == op.eval( u( x ), u( c ) ) );
                }
            }
        }
    }
}

TEST_CASE( "binary ops with a constant left operand match the reference", "[jit][arith]" )
{
    for ( const auto& op : binops( ) )
    {
        DYNAMIC_SECTION( op.name )
        {
            for ( const auto c : edge_values( ) )
            {
                auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                        [ & ]( ir::IRBuilder& b, const Args& a )
                                                        {
                                                            b.ret( ( b.*op.build )( b.iconst( Type::i64, u( c ) ), a[ 0 ] ) );
                                                        } );
                for ( const auto y : edge_values( ) )
                {
                    if ( op.traps( u( c ), u( y ) ) ) continue;
                    CAPTURE( c, y );
                    CHECK( u( f( y ) ) == op.eval( u( c ), u( y ) ) );
                }
            }
        }
    }
}

TEST_CASE( "binary ops on two constants match the reference", "[jit][arith]" )
{
    for ( const auto& op : binops( ) )
    {
        DYNAMIC_SECTION( op.name )
        {
            std::vector< std::pair< int64_t, int64_t > > pairs;
            for ( const auto x : small_constants( ) )
                for ( const auto y : small_constants( ) )
                    if ( !op.traps( u( x ), u( y ) ) ) pairs.emplace_back( x, y );

            auto f = compile< void( uint64_t* ) >( Type::unit, { Type::ptr },
                                                   [ & ]( ir::IRBuilder& b, const Args& a )
                                                   {
                                                       for ( std::size_t k { 0 }; k < pairs.size( ); ++k )
                                                       {
                                                           const auto v = ( b.*op.build )( b.iconst( Type::i64, u( pairs[ k ].first ) ),
                                                                                           b.iconst( Type::i64, u( pairs[ k ].second ) ) );
                                                           b.store( v, a[ 0 ], static_cast< int32_t >( k * 8 ) );
                                                       }
                                                       b.ret( );
                                                   } );

            std::vector< uint64_t > out( pairs.size( ), 0xdeadbeefdeadbeefull );
            f( out.data( ) );
            for ( std::size_t k { 0 }; k < pairs.size( ); ++k )
            {
                CAPTURE( pairs[ k ].first, pairs[ k ].second );
                CHECK( out[ k ] == op.eval( u( pairs[ k ].first ), u( pairs[ k ].second ) ) );
            }
        }
    }
}

TEST_CASE( "binary ops with the same value as both operands", "[jit][arith]" )
{
    for ( const auto& op : binops( ) )
    {
        DYNAMIC_SECTION( op.name )
        {
            auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                    [ & ]( ir::IRBuilder& b, const Args& a )
                                                    {
                                                        b.ret( ( b.*op.build )( a[ 0 ], a[ 0 ] ) );
                                                    } );
            for ( const auto x : edge_values( ) )
            {
                if ( op.traps( u( x ), u( x ) ) ) continue;
                CAPTURE( x );
                CHECK( u( f( x ) ) == op.eval( u( x ), u( x ) ) );
            }
        }
    }
}

TEST_CASE( "binary ops leave their source operands intact", "[jit][arith]" )
{
    for ( const auto& op : binops( ) )
    {
        DYNAMIC_SECTION( op.name )
        {
            auto f = compile< int64_t( int64_t, int64_t ) >(
                Type::i64, { Type::i64, Type::i64 },
                [ & ]( ir::IRBuilder& b, const Args& a )
                {
                    const auto r = ( b.*op.build )( a[ 0 ], a[ 1 ] );
                    b.ret( b.iadd( b.imul( r, b.iconst( Type::i64, 1000003 ) ),
                                   b.bxor( a[ 0 ], b.imul( a[ 1 ], b.iconst( Type::i64, 31 ) ) ) ) );
                } );
            for ( const auto x : edge_values( ) )
                for ( const auto y : edge_values( ) )
                {
                    if ( op.traps( u( x ), u( y ) ) ) continue;
                    CAPTURE( x, y );
                    CHECK( u( f( x, y ) ) == op.eval( u( x ), u( y ) ) * 1000003 + ( u( x ) ^ ( u( y ) * 31 ) ) );
                }
        }
    }
}

TEST_CASE( "division by small constant divisors", "[jit][arith]" )
{
    for ( const int64_t d : { 1, 2, 3, 7, 10, 16, -1, -2, -3, -10, 1000000007, -1000000007 } )
    {
        auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                [ & ]( ir::IRBuilder& b, const Args& a )
                                                {
                                                    const auto c = b.iconst( Type::i64, u( d ) );
                                                    const auto q = b.idiv( a[ 0 ], c );
                                                    const auto r = b.srem( a[ 0 ], c );
                                                    b.ret( b.iadd( b.imul( q, b.iconst( Type::i64, 1 << 20 ) ), r ) );
                                                } );
        for ( const auto x : edge_values( ) )
        {
            if ( d == -1 && x == i64_min ) continue;
            CAPTURE( x, d );
            CHECK( u( f( x ) ) == u( x / d ) * ( 1u << 20 ) + u( x % d ) );
        }
    }
}

TEST_CASE( "quotient and remainder recombine to the dividend", "[jit][arith]" )
{
    auto s = compile< int64_t( int64_t, int64_t ) >(
        Type::i64, { Type::i64, Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            b.ret( b.iadd( b.imul( b.idiv( a[ 0 ], a[ 1 ] ), a[ 1 ] ), b.srem( a[ 0 ], a[ 1 ] ) ) );
        } );
    auto uu = compile< uint64_t( uint64_t, uint64_t ) >(
        Type::i64, { Type::i64, Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            b.ret( b.iadd( b.imul( b.udiv( a[ 0 ], a[ 1 ] ), a[ 1 ] ), b.urem( a[ 0 ], a[ 1 ] ) ) );
        } );
    for ( const auto x : edge_values( ) )
        for ( const auto y : edge_values( ) )
        {
            CAPTURE( x, y );
            if ( y != 0 ) CHECK( uu( u( x ), u( y ) ) == u( x ) );
            if ( !signed_trap( u( x ), u( y ) ) ) CHECK( s( x, y ) == x );
        }
}

TEST_CASE( "unary bnot and ineg match the reference", "[jit][arith]" )
{
    auto n    = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                               []( ir::IRBuilder& b, const Args& a )
                                               {
                                                b.ret( b.bnot( a[ 0 ] ) );
                                            } );
    auto g    = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                               []( ir::IRBuilder& b, const Args& a )
                                               {
                                                b.ret( b.ineg( a[ 0 ] ) );
                                            } );
    auto both = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                               []( ir::IRBuilder& b, const Args& a )
                                               {
                                                   b.ret( b.iadd( b.ineg( b.bnot( a[ 0 ] ) ), a[ 0 ] ) );
                                               } );
    for ( const auto x : edge_values( ) )
    {
        CAPTURE( x );
        CHECK( u( n( x ) ) == ~u( x ) );
        CHECK( u( g( x ) ) == 0 - u( x ) );
        CHECK( u( both( x ) ) == ( 0 - ~u( x ) ) + u( x ) );
    }
    CHECK( g( i64_min ) == i64_min );
    CHECK( n( 0 ) == -1 );
}

TEST_CASE( "unary ops on constants", "[jit][arith]" )
{
    for ( const auto c : edge_values( ) )
    {
        auto f = compile< int64_t( ) >(
            Type::i64, { },
            [ & ]( ir::IRBuilder& b, const Args& )
            {
                b.ret( b.bxor( b.bnot( b.iconst( Type::i64, u( c ) ) ), b.ineg( b.iconst( Type::i64, u( c ) ) ) ) );
            } );
        CAPTURE( c );
        CHECK( u( f( ) ) == ( ~u( c ) ^ ( 0 - u( c ) ) ) );
    }
}

TEST_CASE( "iconst materialises every edge value", "[jit][arith]" )
{
    for ( const auto c : edge_values( ) )
    {
        auto f = compile< int64_t( ) >( Type::i64, { },
                                        [ & ]( ir::IRBuilder& b, const Args& )
                                        {
                                            b.ret( b.iconst( Type::i64, u( c ) ) );
                                        } );
        CAPTURE( c );
        CHECK( f( ) == c );
    }
}

TEST_CASE( "iconst of narrow types returns the truncated value", "[jit][arith]" )
{
    for ( const auto c : edge_values( ) )
    {
        CAPTURE( c );
        auto f8  = compile< int8_t( ) >( Type::i8, { },
                                         [ & ]( ir::IRBuilder& b, const Args& )
                                         {
                                            b.ret( b.iconst( Type::i8, u( c ) ) );
                                        } );
        auto f16 = compile< int16_t( ) >( Type::i16, { },
                                          [ & ]( ir::IRBuilder& b, const Args& )
                                          {
                                              b.ret( b.iconst( Type::i16, u( c ) ) );
                                          } );
        auto f32 = compile< int32_t( ) >( Type::i32, { },
                                          [ & ]( ir::IRBuilder& b, const Args& )
                                          {
                                              b.ret( b.iconst( Type::i32, u( c ) ) );
                                          } );
        CHECK( f8( ) == static_cast< int8_t >( c ) );
        CHECK( f16( ) == static_cast< int16_t >( c ) );
        CHECK( f32( ) == static_cast< int32_t >( c ) );
    }
}

TEST_CASE( "shifts by register counts mask the count to six bits", "[jit][arith]" )
{
    for ( const auto* name : { "ishl", "ushr", "sshr" } )
    {
        DYNAMIC_SECTION( name )
        {
            const auto& op = *std::ranges::find( binops( ), std::string_view { name }, &BinOp::name );
            auto        f  = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                                     [ & ]( ir::IRBuilder& b, const Args& a )
                                                                     {
                                                                 b.ret( ( b.*op.build )( a[ 0 ], a[ 1 ] ) );
                                                             } );
            for ( const auto x : edge_values( ) )
                for ( const auto c : shift_counts( ) )
                {
                    CAPTURE( x, c );
                    CHECK( u( f( x, c ) ) == op.eval( u( x ), u( c ) ) );
                }
        }
    }
}

TEST_CASE( "shifts by constant counts mask the count to six bits", "[jit][arith]" )
{
    for ( const auto* name : { "ishl", "ushr", "sshr" } )
    {
        DYNAMIC_SECTION( name )
        {
            const auto& op = *std::ranges::find( binops( ), std::string_view { name }, &BinOp::name );
            for ( const auto c : shift_counts( ) )
            {
                auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                        [ & ]( ir::IRBuilder& b, const Args& a )
                                                        {
                                                            b.ret( ( b.*op.build )( a[ 0 ], b.iconst( Type::i64, u( c ) ) ) );
                                                        } );
                for ( const auto x : edge_values( ) )
                {
                    CAPTURE( x, c );
                    CHECK( u( f( x ) ) == op.eval( u( x ), u( c ) ) );
                }
            }
        }
    }
}

TEST_CASE( "shifts of a constant value by a register count", "[jit][arith]" )
{
    for ( const auto* name : { "ishl", "ushr", "sshr" } )
    {
        DYNAMIC_SECTION( name )
        {
            const auto& op = *std::ranges::find( binops( ), std::string_view { name }, &BinOp::name );
            for ( const int64_t v : { int64_t { 1 }, int64_t { -1 }, i64_min, int64_t { 0x5a5a5a5a5a5a5a5a } } )
            {
                auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                        [ & ]( ir::IRBuilder& b, const Args& a )
                                                        {
                                                            b.ret( ( b.*op.build )( b.iconst( Type::i64, u( v ) ), a[ 0 ] ) );
                                                        } );
                for ( const auto c : shift_counts( ) )
                {
                    CAPTURE( v, c );
                    CHECK( u( f( c ) ) == op.eval( u( v ), u( c ) ) );
                }
            }
        }
    }
}

TEST_CASE( "shift counts of narrow types use only their low bits", "[jit][arith]" )
{
    for ( const auto cty : { Type::i8, Type::i16, Type::i32 } )
    {
        DYNAMIC_SECTION( std::format( "{}", cty ) )
        {
            auto f = compile< int64_t( int64_t, int64_t ) >(
                Type::i64, { Type::i64, Type::i64 },
                [ & ]( ir::IRBuilder& b, const Args& a )
                {
                    const auto c = b.ireduce( cty, a[ 1 ] );
                    const auto l = b.ishl( a[ 0 ], c );
                    const auto r = b.ushr( a[ 0 ], c );
                    const auto s = b.sshr( a[ 0 ], c );
                    b.ret( b.bxor( b.bxor( l, b.imul( r, b.iconst( Type::i64, 3 ) ) ), b.imul( s, b.iconst( Type::i64, 5 ) ) ) );
                } );
            for ( const auto x : edge_values( ) )
                for ( const auto c : shift_counts( ) )
                {
                    const auto n = u( c ) & 63;
                    CAPTURE( x, c );
                    CHECK( u( f( x, c ) ) == ( ( u( x ) << n ) ^ ( ( u( x ) >> n ) * 3 ) ^ ( static_cast< uint64_t >( x >> n ) * 5 ) ) );
                }
        }
    }
}

TEST_CASE( "shift by the value itself", "[jit][arith]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                b.ret( b.iadd( b.ishl( a[ 0 ], a[ 0 ] ), b.sshr( a[ 0 ], a[ 0 ] ) ) );
                                            } );
    for ( const auto x : edge_values( ) )
    {
        CAPTURE( x );
        CHECK( u( f( x ) ) == ( u( x ) << ( u( x ) & 63 ) ) + u( x >> ( u( x ) & 63 ) ) );
    }
}

TEST_CASE( "narrow arithmetic sign and zero extends back to i64", "[jit][arith]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32 } )
        for ( const auto& op : narrow_binops( ) )
        {
            DYNAMIC_SECTION( std::format( "{} {}", op.name, ty ) )
            {
                auto s = compile< int64_t( int64_t, int64_t ) >(
                    Type::i64, { Type::i64, Type::i64 },
                    [ & ]( ir::IRBuilder& b, const Args& a )
                    {
                        b.ret( b.sextend( Type::i64, ( b.*op.build )( b.ireduce( ty, a[ 0 ] ), b.ireduce( ty, a[ 1 ] ) ) ) );
                    } );
                auto z = compile< int64_t( int64_t, int64_t ) >(
                    Type::i64, { Type::i64, Type::i64 },
                    [ & ]( ir::IRBuilder& b, const Args& a )
                    {
                        b.ret( b.uextend( Type::i64, ( b.*op.build )( b.ireduce( ty, a[ 0 ] ), b.ireduce( ty, a[ 1 ] ) ) ) );
                    } );
                for ( const auto x : edge_values( ) )
                    for ( const auto y : edge_values( ) )
                    {
                        const auto r = op.eval( mask( ty, u( x ) ), mask( ty, u( y ) ) );
                        CAPTURE( x, y );
                        CHECK( u( s( x, y ) ) == sext( ty, mask( ty, r ) ) );
                        CHECK( u( z( x, y ) ) == mask( ty, r ) );
                    }
            }
        }
}

TEST_CASE( "narrow unary ops sign and zero extend back to i64", "[jit][arith]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32 } )
    {
        DYNAMIC_SECTION( std::format( "{}", ty ) )
        {
            auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                    [ & ]( ir::IRBuilder& b, const Args& a )
                                                    {
                                                        const auto v = b.ireduce( ty, a[ 0 ] );
                                                        const auto n = b.sextend( Type::i64, b.bnot( v ) );
                                                        const auto g = b.uextend( Type::i64, b.ineg( v ) );
                                                        b.ret( b.bxor( n, b.ishl( g, b.iconst( Type::i64, 1 ) ) ) );
                                                    } );
            for ( const auto x : edge_values( ) )
            {
                CAPTURE( x );
                CHECK( u( f( x ) ) == ( sext( ty, mask( ty, ~u( x ) ) ) ^ ( mask( ty, 0 - u( x ) ) << 1 ) ) );
            }
        }
    }
}

namespace
{
    template < typename T >
    void check_native_narrow( const Type ty )
    {
        for ( const auto& op : narrow_binops( ) )
        {
            if ( op.name == "ishl" ) continue;
            auto f = compile< T( T, T ) >( ty, { ty, ty },
                                           [ & ]( ir::IRBuilder& b, const Args& a )
                                           {
                                               b.ret( ( b.*op.build )( a[ 0 ], a[ 1 ] ) );
                                           } );
            for ( const auto x : edge_values( ) )
                for ( const auto y : edge_values( ) )
                {
                    const auto tx  = static_cast< T >( x );
                    const auto ty2 = static_cast< T >( y );
                    CAPTURE( op.name, x, y );
                    CHECK( f( tx, ty2 ) == static_cast< T >( op.eval( u( tx ), u( ty2 ) ) ) );
                }
        }

        auto shl = compile< T( T, int64_t ) >( ty, { ty, Type::i64 },
                                               [ & ]( ir::IRBuilder& b, const Args& a )
                                               {
                                                   b.ret( b.ishl( a[ 0 ], a[ 1 ] ) );
                                               } );
        for ( const auto x : edge_values( ) )
            for ( const auto c : shift_counts( ) )
            {
                CAPTURE( x, c );
                CHECK( shl( static_cast< T >( x ), c ) == static_cast< T >( u( static_cast< T >( x ) ) << ( u( c ) & 63 ) ) );
            }

        auto un = compile< T( T ) >( ty, { ty },
                                     [ & ]( ir::IRBuilder& b, const Args& a )
                                     {
                                         b.ret( b.iadd( b.bnot( a[ 0 ] ), b.ineg( a[ 0 ] ) ) );
                                     } );
        for ( const auto x : edge_values( ) )
        {
            const auto tx = static_cast< T >( x );
            CAPTURE( x );
            CHECK( un( tx ) == static_cast< T >( ~u( tx ) + ( 0 - u( tx ) ) ) );
        }
    }
} // namespace

TEST_CASE( "i8 arithmetic with native i8 parameters and return", "[jit][arith]" )
{ check_native_narrow< int8_t >( Type::i8 ); }

TEST_CASE( "i16 arithmetic with native i16 parameters and return", "[jit][arith]" )
{ check_native_narrow< int16_t >( Type::i16 ); }

TEST_CASE( "i32 arithmetic with native i32 parameters and return", "[jit][arith]" )
{ check_native_narrow< int32_t >( Type::i32 ); }

TEST_CASE( "narrow arithmetic with constants folded into immediates", "[jit][arith]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32 } )
        for ( const auto& op : narrow_binops( ) )
            for ( const auto c : small_constants( ) )
            {
                auto f = compile< int64_t( int64_t ) >(
                    Type::i64, { Type::i64 },
                    [ & ]( ir::IRBuilder& b, const Args& a )
                    {
                        b.ret( b.uextend( Type::i64, ( b.*op.build )( b.ireduce( ty, a[ 0 ] ), b.iconst( ty, mask( ty, u( c ) ) ) ) ) );
                    } );
                for ( const auto x : edge_values( ) )
                {
                    CAPTURE( op.name, ty, x, c );
                    CHECK( u( f( x ) ) == mask( ty, op.eval( mask( ty, u( x ) ), mask( ty, u( c ) ) ) ) );
                }
            }
}

TEST_CASE( "uextend sextend and ireduce between every pair of widths", "[jit][arith]" )
{
    for ( const auto from : int_types )
        for ( const auto to : int_types )
        {
            if ( from == to ) continue;
            DYNAMIC_SECTION( std::format( "{} to {}", from, to ) )
            {
                if ( ir::bit_width( to ) > ir::bit_width( from ) )
                {
                    auto zu
                        = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                         [ & ]( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             b.ret( widened( b, to, b.uextend( to, narrowed( b, from, a[ 0 ] ) ), false ) );
                                                         } );
                    auto sx
                        = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                         [ & ]( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             b.ret( widened( b, to, b.sextend( to, narrowed( b, from, a[ 0 ] ) ), false ) );
                                                         } );
                    for ( const auto x : edge_values( ) )
                    {
                        CAPTURE( x );
                        CHECK( u( zu( x ) ) == mask( from, u( x ) ) );
                        CHECK( u( sx( x ) ) == mask( to, sext( from, mask( from, u( x ) ) ) ) );
                    }
                }
                else
                {
                    auto rz
                        = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                         [ & ]( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             b.ret( widened( b, to, b.ireduce( to, narrowed( b, from, a[ 0 ] ) ), false ) );
                                                         } );
                    auto rs
                        = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                         [ & ]( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             b.ret( widened( b, to, b.ireduce( to, narrowed( b, from, a[ 0 ] ) ), true ) );
                                                         } );
                    for ( const auto x : edge_values( ) )
                    {
                        CAPTURE( x );
                        CHECK( u( rz( x ) ) == mask( to, u( x ) ) );
                        CHECK( u( rs( x ) ) == sext( to, mask( to, u( x ) ) ) );
                    }
                }
            }
        }
}

TEST_CASE( "extends of native narrow parameters ignore garbage upper bits", "[jit][arith]" )
{
    auto f8 = compile< int64_t( int8_t, int8_t ) >( Type::i64, { Type::i8, Type::i8 },
                                                    []( ir::IRBuilder& b, const Args& a )
                                                    {
                                                        b.ret( b.isub( b.sextend( Type::i64, a[ 0 ] ), b.uextend( Type::i64, a[ 1 ] ) ) );
                                                    } );
    auto f16
        = compile< int64_t( int16_t, int16_t ) >( Type::i64, { Type::i16, Type::i16 },
                                                  []( ir::IRBuilder& b, const Args& a )
                                                  {
                                                      b.ret( b.isub( b.sextend( Type::i64, a[ 0 ] ), b.uextend( Type::i64, a[ 1 ] ) ) );
                                                  } );
    auto f32
        = compile< int64_t( int32_t, int32_t ) >( Type::i64, { Type::i32, Type::i32 },
                                                  []( ir::IRBuilder& b, const Args& a )
                                                  {
                                                      b.ret( b.isub( b.sextend( Type::i64, a[ 0 ] ), b.uextend( Type::i64, a[ 1 ] ) ) );
                                                  } );
    for ( const auto x : edge_values( ) )
        for ( const auto y : edge_values( ) )
        {
            CAPTURE( x, y );
            CHECK( f8( static_cast< int8_t >( x ), static_cast< int8_t >( y ) )
                   == static_cast< int64_t >( static_cast< int8_t >( x ) ) - static_cast< int64_t >( static_cast< uint8_t >( y ) ) );
            CHECK( f16( static_cast< int16_t >( x ), static_cast< int16_t >( y ) )
                   == static_cast< int64_t >( static_cast< int16_t >( x ) ) - static_cast< int64_t >( static_cast< uint16_t >( y ) ) );
            CHECK( f32( static_cast< int32_t >( x ), static_cast< int32_t >( y ) )
                   == static_cast< int64_t >( static_cast< int32_t >( x ) ) - static_cast< int64_t >( static_cast< uint32_t >( y ) ) );
        }
}

TEST_CASE( "chains of extends and reductions", "[jit][arith]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto a8  = b.ireduce( Type::i8, a[ 0 ] );
                                                const auto a16 = b.sextend( Type::i16, a8 );
                                                const auto a32 = b.uextend( Type::i32, a16 );
                                                const auto a64 = b.sextend( Type::i64, a32 );
                                                const auto r16 = b.ireduce( Type::i16, a64 );
                                                b.ret( b.iadd( a64, b.sextend( Type::i64, r16 ) ) );
                                            } );
    for ( const auto x : edge_values( ) )
    {
        const auto s16 = static_cast< uint16_t >( static_cast< int16_t >( static_cast< int8_t >( x ) ) );
        const auto e64 = static_cast< int64_t >( static_cast< int32_t >( static_cast< uint32_t >( s16 ) ) );
        CAPTURE( x );
        CHECK( f( x ) == e64 + static_cast< int64_t >( static_cast< int16_t >( e64 ) ) );
    }
}

TEST_CASE( "pointer plus and minus i64 offsets", "[jit][arith]" )
{
    auto add = compile< uint64_t( uint64_t, int64_t ) >( Type::ptr, { Type::ptr, Type::i64 },
                                                         []( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             b.ret( b.iadd( a[ 0 ], a[ 1 ] ) );
                                                         } );
    auto sub = compile< uint64_t( uint64_t, int64_t ) >( Type::ptr, { Type::ptr, Type::i64 },
                                                         []( ir::IRBuilder& b, const Args& a )
                                                         {
                                                             b.ret( b.isub( a[ 0 ], a[ 1 ] ) );
                                                         } );
    for ( const auto p : edge_values( ) )
        for ( const auto k : edge_values( ) )
        {
            CAPTURE( p, k );
            CHECK( add( u( p ), k ) == u( p ) + u( k ) );
            CHECK( sub( u( p ), k ) == u( p ) - u( k ) );
        }

    for ( const auto k : edge_values( ) )
    {
        auto c = compile< uint64_t( uint64_t ) >(
            Type::ptr, { Type::ptr },
            [ & ]( ir::IRBuilder& b, const Args& a )
            {
                b.ret( b.isub( b.iadd( a[ 0 ], b.iconst( Type::i64, u( k ) ) ), b.iconst( Type::i64, 8 ) ) );
            } );
        CAPTURE( k );
        CHECK( c( 0x1000 ) == 0x1000 + u( k ) - 8 );
    }
}

TEST_CASE( "pointer constants and pointer bitwise ops", "[jit][arith]" )
{
    auto f = compile< uint64_t( uint64_t ) >(
        Type::ptr, { Type::ptr },
        []( ir::IRBuilder& b, const Args& a )
        {
            b.ret( b.band( b.bor( a[ 0 ], b.iconst( Type::ptr, 7 ) ), b.iconst( Type::ptr, ~uint64_t { 0xf } ) ) );
        } );
    for ( const auto p : edge_values( ) )
    {
        CAPTURE( p );
        CHECK( f( u( p ) ) == ( ( u( p ) | 7 ) & ~uint64_t { 0xf } ) );
    }
}

TEST_CASE( "long dependent arithmetic chain matches the interpreter", "[jit][arith]" )
{
    auto f = compile< int64_t( int64_t, int64_t, int64_t ) >(
        Type::i64, { Type::i64, Type::i64, Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            auto x = a[ 0 ];
            auto y = a[ 1 ];
            auto z = a[ 2 ];
            for ( int k { 0 }; k < 40; ++k )
            {
                const auto t = b.iadd( b.imul( x, b.iconst( Type::i64, static_cast< uint64_t >( 2 * k + 3 ) ) ), y );
                const auto s = b.bxor( b.ushr( t, b.iconst( Type::i64, static_cast< uint64_t >( k % 13 ) ) ), z );
                const auto r = b.isub( b.ishl( s, b.iconst( Type::i64, static_cast< uint64_t >( k % 7 ) ) ), b.sshr( x, z ) );
                x            = y;
                y            = z;
                z            = b.bor( r, b.band( t, s ) );
            }
            b.ret( b.iadd( b.iadd( x, y ), z ) );
        } );
    const auto& fn = *f.jit->irm.get( ir::FunctionRef { 0 } );
    for ( const auto x : edge_values( ) )
        for ( const int64_t y : { int64_t { 0 }, int64_t { -5 }, int64_t { 99 }, i64_min } )
            for ( const int64_t z : { int64_t { 1 }, int64_t { 17 }, int64_t { 64 } } )
            {
                const uint64_t args[] = { u( x ), u( y ), u( z ) };
                CAPTURE( x, y, z );
                CHECK( u( f( x, y, z ) ) == Interp { }.run( fn, args ) );
            }
}

TEST_CASE( "a block with more than 65535 machine instructions compiles correctly", "[jit]" )
{
    constexpr uint32_t adds { 40000 };
    const auto         c = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                          [ & ]( ir::IRBuilder& b, const Args& a )
                                                          {
                                                      auto x = a[ 0 ];
                                                      for ( uint32_t i { 0 }; i < adds; ++i ) x = b.iadd( x, a[ 0 ] );
                                                      b.ret( x );
                                                  } );
    CHECK( c( 1 ) == static_cast< int64_t >( adds ) + 1 );
}