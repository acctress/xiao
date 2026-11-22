#include "support.hh"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

using namespace xt;

namespace
{
    bool is_ud2_at( const codegen::CompiledCode& code, const uint32_t offset )
    { return offset + 1 < code.code.size( ) && code.code[ offset ] == 0x0F && code.code[ offset + 1 ] == 0x0B; }

    void check_sites( const codegen::CompiledCode& code, const std::vector< ir::TrapCode >& want )
    {
        REQUIRE( code.traps.size( ) == want.size( ) );
        for ( std::size_t i { }; i < want.size( ); ++i )
        {
            CAPTURE( i, code.traps[ i ].offset );
            CHECK( is_ud2_at( code, code.traps[ i ].offset ) );
            CHECK( code.traps[ i ].code == want[ i ] );
        }
    }
}

TEST_CASE( "trapz does not trap on a non zero condition", "[jit][trap]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            [ & ]( ir::IRBuilder& b, const Args& a )
                                            {
                                                b.trapz( a[ 0 ], ir::TrapCode::integer_division_by_zero );
                                                b.ret( b.iadd( a[ 0 ], b.iconst( Type::i64, 1 ) ) );
                                            } );
    for ( const auto x : edge_values( ) )
    {
        if ( x == 0 ) continue;
        CHECK( f( x ) == static_cast< int64_t >( static_cast< uint64_t >( x ) + 1 ) );
    }
    check_sites( f.code, { ir::TrapCode::integer_division_by_zero } );
}

TEST_CASE( "trapnz does not trap on zero", "[jit][trap]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     [ & ]( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         b.trapnz( a[ 0 ], ir::TrapCode::integer_overflow );
                                                         b.ret( a[ 1 ] );
                                                     } );
    for ( const auto y : edge_values( ) ) CHECK( f( 0, y ) == y );
    check_sites( f.code, { ir::TrapCode::integer_overflow } );
}

TEST_CASE( "trapz and trapnz test only the width of a narrow condition", "[jit][trap]" )
{
    for ( const auto ty : { Type::i8, Type::i16, Type::i32 } )
    {
        auto z = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                [ & ]( ir::IRBuilder& b, const Args& a )
                                                {
                                                    b.trapnz( b.ireduce( ty, a[ 0 ] ), ir::user_trap( 1 ) );
                                                    b.ret( a[ 0 ] );
                                                } );
        auto n = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                [ & ]( ir::IRBuilder& b, const Args& a )
                                                {
                                                    b.trapz( b.ireduce( ty, a[ 0 ] ), ir::user_trap( 2 ) );
                                                    b.ret( a[ 0 ] );
                                                } );
        const auto high = int64_t { 1 } << ir::bit_width( ty );
        CAPTURE( ty );
        CHECK( z( high ) == high );
        CHECK( z( high * 3 ) == high * 3 );
        CHECK( n( high + 1 ) == high + 1 );
        CHECK( n( -high | 1 ) == ( -high | 1 ) );
    }
}

TEST_CASE( "trap ends a block that is never reached", "[jit][trap]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            [ & ]( ir::IRBuilder& b, const Args& a )
                                            {
                                                const auto ok   = b.add_block( { } );
                                                const auto fail = b.add_block( { } );
                                                b.brif( b.icmp( ir::ICmpCC::sge, a[ 0 ], b.iconst( Type::i64, 0 ) ), ok, { }, fail, { } );
                                                b.switch_to_block( fail );
                                                b.trap( ir::TrapCode::unreachable );
                                                b.switch_to_block( ok );
                                                b.ret( b.imul( a[ 0 ], b.iconst( Type::i64, 2 ) ) );
                                            } );
    for ( const int64_t x : { 0, 1, 7, 1000, 0x3fffffff } ) CHECK( f( x ) == x * 2 );
    check_sites( f.code, { ir::TrapCode::unreachable } );
}

TEST_CASE( "a function can end in trap only", "[jit][trap]" )
{
    auto f = compile< void( ) >( Type::unit, { },
                                 [ & ]( ir::IRBuilder& b, const Args& )
                                 {
                                     b.trap( ir::user_trap( 7 ) );
                                 } );
    check_sites( f.code, { ir::user_trap( 7 ) } );
}

TEST_CASE( "trap sites are recorded in code order with their codes", "[jit][trap]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                     [ & ]( ir::IRBuilder& b, const Args& a )
                                                     {
                                                         b.trapz( a[ 1 ], ir::TrapCode::integer_division_by_zero );
                                                         const auto q = b.udiv( a[ 0 ], a[ 1 ] );
                                                         b.trapnz( b.icmp( ir::ICmpCC::ugt, q, b.iconst( Type::i64, 100 ) ), ir::TrapCode::integer_overflow );
                                                         b.ret( q );
                                                     } );
    CHECK( f( 100, 5 ) == 20 );
    CHECK( f( 7, 7 ) == 1 );
    check_sites( f.code, { ir::TrapCode::integer_division_by_zero, ir::TrapCode::integer_overflow } );
}

TEST_CASE( "values live across trapz stay intact", "[jit][trap]" )
{
    auto f = compile< int64_t( int64_t, int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64, Type::i64 },
                                                              [ & ]( ir::IRBuilder& b, const Args& a )
                                                              {
                                                                  const auto s = b.iadd( a[ 0 ], a[ 1 ] );
                                                                  b.trapz( a[ 2 ], ir::TrapCode::unreachable );
                                                                  b.trapnz( b.isub( a[ 2 ], a[ 2 ] ), ir::TrapCode::unreachable );
                                                                  b.ret( b.imul( s, a[ 2 ] ) );
                                                              } );
    CHECK( f( 2, 3, 4 ) == 20 );
    CHECK( f( -5, 1, -1 ) == 4 );
}

TEST_CASE( "interpreter traps where the JIT would", "[jit][trap]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            [ & ]( ir::IRBuilder& b, const Args& a )
                                            {
                                                b.trapz( a[ 0 ], ir::TrapCode::integer_division_by_zero );
                                                b.ret( a[ 0 ] );
                                            } );
    const auto&    fn     = *f.jit->irm.get( ir::FunctionRef { 0 } );
    const uint64_t one[]  = { 1 };
    const uint64_t zero[] = { 0 };
    CHECK( f( 1 ) == 1 );
    CHECK( Interp { }.run( fn, one ) == 1 );
    CHECK_THROWS_AS( Interp { }.run( fn, zero ), InterpError );
}
