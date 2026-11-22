#include "support.hh"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <cpuid.h>
#include <cstdint>
#include <format>
#include <memory>
#include <string_view>
#include <vector>

using namespace xt;

namespace
{
    using BinBuild   = ValueRef ( ir::IRBuilder::* )( ValueRef, ValueRef );
    using UnaryBuild = ValueRef ( ir::IRBuilder::* )( ValueRef );

    constexpr std::array< Type, 4 > int_types { Type::i8, Type::i16, Type::i32, Type::i64 };

    uint64_t u( const int64_t v )
    { return static_cast< uint64_t >( v ); }

    ValueRef narrowed( ir::IRBuilder& b, const Type ty, const ValueRef v )
    { return ty == Type::i64 ? v : b.ireduce( ty, v ); }

    ValueRef widened( ir::IRBuilder& b, const Type ty, const ValueRef v )
    { return ty == Type::i64 ? v : b.uextend( Type::i64, v ); }

    const std::vector< int64_t >& counts( )
    {
        static const std::vector< int64_t > values { 0, 1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 255, -1, -8 };
        return values;
    }

    std::vector< uint64_t > bit_values( const Type ty )
    {
        std::vector< uint64_t > out;
        for ( const auto x : edge_values( ) ) out.push_back( mask( ty, u( x ) ) );
        for ( uint32_t k { }; k < ir::bit_width( ty ); ++k )
        {
            out.push_back( uint64_t { 1 } << k );
            out.push_back( mask( ty, ~( uint64_t { 1 } << k ) ) );
            out.push_back( mask( ty, ~uint64_t { 0 } << k ) );
        }
        return out;
    }

    uint64_t rotate_ref( const bool left, const Type ty, const uint64_t value, const uint64_t count )
    {
        const auto w = ir::bit_width( ty );
        const auto n = count & ( w - 1 );
        const auto v = mask( ty, value );
        if ( n == 0 ) return v;
        return mask( ty, left ? ( v << n ) | ( v >> ( w - n ) ) : ( v >> n ) | ( v << ( w - n ) ) );
    }

    uint64_t bswap_ref( const Type ty, const uint64_t v )
    {
        uint64_t r { };
        for ( uint32_t k { }; k < ir::bit_width( ty ) / 8u; ++k ) r = ( r << 8 ) | ( ( v >> ( 8 * k ) ) & 0xFF );
        return r;
    }

    uint64_t clz_ref( const Type ty, const uint64_t v )
    {
        const auto w = ir::bit_width( ty );
        return static_cast< uint64_t >( std::countl_zero( mask( ty, v ) ) ) - ( 64 - w );
    }

    uint64_t ctz_ref( const Type ty, const uint64_t v )
    {
        const auto m = mask( ty, v );
        return m == 0 ? ir::bit_width( ty ) : static_cast< uint64_t >( std::countr_zero( m ) );
    }

    uint64_t popcnt_ref( const Type ty, const uint64_t v )
    { return static_cast< uint64_t >( std::popcount( mask( ty, v ) ) ); }

    uint64_t umulhi_ref( const Type ty, const uint64_t a, const uint64_t b )
    {
        const auto w = ir::bit_width( ty );
        return mask( ty, static_cast< uint64_t >( ( static_cast< unsigned __int128 >( mask( ty, a ) ) * mask( ty, b ) ) >> w ) );
    }

    uint64_t smulhi_ref( const Type ty, const uint64_t a, const uint64_t b )
    {
        const auto w  = ir::bit_width( ty );
        const auto sa = static_cast< int64_t >( sext( ty, mask( ty, a ) ) );
        const auto sb = static_cast< int64_t >( sext( ty, mask( ty, b ) ) );
        return mask( ty, static_cast< uint64_t >( ( static_cast< __int128 >( sa ) * sb ) >> w ) );
    }

    struct Features
    {
        bool popcnt { };
        bool lzcnt { };
        bool bmi1 { };
    };

    Features host_features( )
    {
        Features f;
        unsigned a { }, b { }, c { }, d { };
        if ( __get_cpuid( 1, &a, &b, &c, &d ) ) f.popcnt = ( c >> 23 ) & 1;
        if ( __get_cpuid( 0x80000001u, &a, &b, &c, &d ) ) f.lzcnt = ( c >> 5 ) & 1;
        if ( __get_cpuid_count( 7, 0, &a, &b, &c, &d ) ) f.bmi1 = ( b >> 3 ) & 1;
        return f;
    }

    template < typename F, typename Body >
    Compiled< F > compile_with( const codegen::Flags& flags, const Type ret, const std::vector< Type >& params, Body&& body )
    {
        Compiled< F > c;
        c.jit         = std::make_unique< Jit >( jit::JITBuilder { isa::lookup_host( flags ) } );
        const auto id = c.jit->define( "f", ret, params, std::forward< Body >( body ) );
        c.jit->finalise( );
        c.fn   = c.jit->template get< F >( id );
        c.code = c.jit->last_code;
        return c;
    }

    struct CountOp
    {
        std::string_view name;
        UnaryBuild       build;
        uint64_t ( *ref )( Type, uint64_t );
    };

    constexpr std::array< CountOp, 3 > count_ops {
        CountOp { "clz", &ir::IRBuilder::clz, clz_ref },
        CountOp { "ctz", &ir::IRBuilder::ctz, ctz_ref },
        CountOp { "popcnt", &ir::IRBuilder::popcnt, popcnt_ref },
    };

    void check_count_ops( const codegen::Flags& flags )
    {
        for ( const auto ty : int_types )
            for ( const auto& op : count_ops )
            {
                DYNAMIC_SECTION( std::format( "{} {}", op.name, ty ) )
                {
                    auto f = compile_with< int64_t( int64_t ) >( flags, Type::i64, { Type::i64 },
                                                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                                                 {
                                                                     b.ret( widened( b, ty, ( b.*op.build )( narrowed( b, ty, a[ 0 ] ) ) ) );
                                                                 } );
                    for ( const auto v : bit_values( ty ) )
                        for ( const auto junk : { uint64_t { 0 }, ~uint64_t { 0 } << ( ir::bit_width( ty ) & 63 ) } )
                        {
                            const auto x = ty == Type::i64 ? v : ( v | junk );
                            CAPTURE( v, x );
                            CHECK( u( f( static_cast< int64_t >( x ) ) ) == op.ref( ty, v ) );
                        }
                }
            }
    }
}

TEST_CASE( "rotl and rotr by register counts at every width", "[jit][bits]" )
{
    for ( const auto ty : int_types )
        for ( const auto left : { true, false } )
        {
            DYNAMIC_SECTION( std::format( "{} {}", left ? "rotl" : "rotr", ty ) )
            {
                const BinBuild op = left ? &ir::IRBuilder::rotl : &ir::IRBuilder::rotr;
                auto           f  = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                                                 {
                                                                     b.ret( widened( b, ty, ( b.*op )( narrowed( b, ty, a[ 0 ] ), a[ 1 ] ) ) );
                                                                 } );
                for ( const auto x : edge_values( ) )
                    for ( const auto c : counts( ) )
                    {
                        CAPTURE( x, c );
                        CHECK( u( f( x, c ) ) == rotate_ref( left, ty, u( x ), u( c ) ) );
                    }
            }
        }
}

TEST_CASE( "rotl and rotr by constant counts at every width", "[jit][bits]" )
{
    for ( const auto ty : int_types )
        for ( const auto left : { true, false } )
        {
            DYNAMIC_SECTION( std::format( "{} {}", left ? "rotl" : "rotr", ty ) )
            {
                const BinBuild op = left ? &ir::IRBuilder::rotl : &ir::IRBuilder::rotr;
                for ( const auto c : counts( ) )
                {
                    auto f = compile< int64_t( int64_t ) >(
                        Type::i64, { Type::i64 },
                        [ & ]( ir::IRBuilder& b, const Args& a )
                        {
                            b.ret( widened( b, ty, ( b.*op )( narrowed( b, ty, a[ 0 ] ), b.iconst( Type::i64, u( c ) ) ) ) );
                        } );
                    for ( const auto x : edge_values( ) )
                    {
                        CAPTURE( x, c );
                        CHECK( u( f( x ) ) == rotate_ref( left, ty, u( x ), u( c ) ) );
                    }
                }
            }
        }
}

TEST_CASE( "rotl by a narrow count ignores its upper bits", "[jit][bits]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     [ & ]( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         b.ret( b.uextend( Type::i64, b.rotl( b.ireduce( Type::i16, a[ 0 ] ),
                                                                                              b.ireduce( Type::i8, a[ 1 ] ) ) ) );
                                                     } );
    for ( const auto x : edge_values( ) )
        for ( const auto c : counts( ) )
        {
            CAPTURE( x, c );
            CHECK( u( f( x, c ) ) == rotate_ref( true, Type::i16, u( x ), mask( Type::i8, u( c ) ) ) );
        }
}

TEST_CASE( "bswap reverses the bytes of i16, i32 and i64", "[jit][bits]" )
{
    for ( const auto ty : { Type::i16, Type::i32, Type::i64 } )
    {
        DYNAMIC_SECTION( std::format( "{}", ty ) )
        {
            auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                    [ & ]( ir::IRBuilder& b, const Args& a )
                                                    {
                                                        b.ret( widened( b, ty, b.bswap( narrowed( b, ty, a[ 0 ] ) ) ) );
                                                    } );
            for ( const auto x : edge_values( ) )
            {
                CAPTURE( x );
                CHECK( u( f( x ) ) == bswap_ref( ty, mask( ty, u( x ) ) ) );
            }
            CHECK( u( f( 0x0102030405060708LL ) ) == bswap_ref( ty, mask( ty, 0x0102030405060708ULL ) ) );
        }
    }
}

TEST_CASE( "bswap twice is the identity", "[jit][bits]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            [ & ]( ir::IRBuilder& b, const Args& a )
                                            {
                                                b.ret( b.bswap( b.bswap( a[ 0 ] ) ) );
                                            } );
    for ( const auto x : edge_values( ) ) CHECK( f( x ) == x );
}

TEST_CASE( "clz, ctz and popcnt without CPU features", "[jit][bits]" )
{ check_count_ops( codegen::Flags { } ); }

TEST_CASE( "clz, ctz and popcnt with lzcnt, tzcnt and popcnt", "[jit][bits]" )
{
    const auto host = host_features( );
    if ( !host.popcnt || !host.lzcnt || !host.bmi1 ) SKIP( "the host CPU lacks popcnt, lzcnt or bmi1" );
    check_count_ops( codegen::Flags { .has_popcnt = true, .has_lzcnt = true, .has_bmi1 = true } );
}

TEST_CASE( "clz, ctz and popcnt of native narrow parameters", "[jit][bits]" )
{
    auto f8 = compile< int64_t( int8_t ) >( Type::i64, { Type::i8 },
                                            [ & ]( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto c = b.clz( a[ 0 ] );
                                                const auto t = b.ctz( a[ 0 ] );
                                                const auto p = b.popcnt( a[ 0 ] );
                                                const auto s = b.iadd( b.imul( b.uextend( Type::i64, c ), b.iconst( Type::i64, 10000 ) ),
                                                                       b.imul( b.uextend( Type::i64, t ), b.iconst( Type::i64, 100 ) ) );
                                                b.ret( b.iadd( s, b.uextend( Type::i64, p ) ) );
                                            } );
    for ( int v { -128 }; v < 128; ++v )
    {
        const auto x = static_cast< uint64_t >( static_cast< uint8_t >( v ) );
        CAPTURE( v );
        CHECK( u( f8( static_cast< int8_t >( v ) ) )
               == clz_ref( Type::i8, x ) * 10000 + ctz_ref( Type::i8, x ) * 100 + popcnt_ref( Type::i8, x ) );
    }
}

TEST_CASE( "umulhi and smulhi at every width", "[jit][bits]" )
{
    for ( const auto ty : int_types )
        for ( const auto is_signed : { false, true } )
        {
            DYNAMIC_SECTION( std::format( "{} {}", is_signed ? "smulhi" : "umulhi", ty ) )
            {
                const BinBuild op = is_signed ? &ir::IRBuilder::smulhi : &ir::IRBuilder::umulhi;
                auto           f  = compile< int64_t( int64_t, int64_t ) >(
                    Type::i64, { Type::i64, Type::i64 },
                    [ & ]( ir::IRBuilder& b, const Args& a )
                    {
                        b.ret( widened( b, ty, ( b.*op )( narrowed( b, ty, a[ 0 ] ), narrowed( b, ty, a[ 1 ] ) ) ) );
                    } );
                for ( const auto x : edge_values( ) )
                    for ( const auto y : edge_values( ) )
                    {
                        const auto want = is_signed ? smulhi_ref( ty, u( x ), u( y ) ) : umulhi_ref( ty, u( x ), u( y ) );
                        CAPTURE( x, y );
                        CHECK( u( f( x, y ) ) == want );
                    }
            }
        }
}

TEST_CASE( "umulhi keeps values live across rax and rdx", "[jit][bits]" )
{
    auto f = compile< int64_t( int64_t, int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64, Type::i64 },
                                                              [ & ]( ir::IRBuilder& b, const Args& a )
                                                              {
                                                                  const auto hi = b.umulhi( a[ 0 ], a[ 1 ] );
                                                                  const auto lo = b.imul( a[ 0 ], a[ 1 ] );
                                                                  b.ret( b.bxor( b.bxor( hi, lo ), a[ 2 ] ) );
                                                              } );
    for ( const auto x : edge_values( ) )
        for ( const auto y : edge_values( ) )
        {
            const auto p = static_cast< unsigned __int128 >( u( x ) ) * u( y );
            CAPTURE( x, y );
            CHECK( u( f( x, y, 0x1234 ) ) == ( static_cast< uint64_t >( p >> 64 ) ^ static_cast< uint64_t >( p ) ^ 0x1234 ) );
        }
}
