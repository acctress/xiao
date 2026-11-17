#include "support.hh"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

using namespace xt;

namespace
{
    uint64_t u( const int64_t v )
    { return static_cast< uint64_t >( v ); }

    ValueRef k64( ir::IRBuilder& b, const int64_t v )
    { return b.iconst( Type::i64, u( v ) ); }

    void h_write( int64_t* const p, const int64_t v )
    { *p = v; }

    void h_fill( uint8_t* const p, const int64_t n, const int64_t seed )
    {
        for ( int64_t i { 0 }; i < n; ++i ) p[ i ] = static_cast< uint8_t >( seed + i * 7 );
    }

    int64_t h_scramble( const int64_t a, const int64_t b, const int64_t c, const int64_t d )
    { return a * 3 - b * 5 + c * 7 - d * 11; }

    jit::JITBuilder host_builder( )
    {
        jit::JITBuilder b { isa::lookup_host( ) };
        b.symbol( "h_write", reinterpret_cast< const void* >( &h_write ) );
        b.symbol( "h_fill", reinterpret_cast< const void* >( &h_fill ) );
        b.symbol( "h_scramble", reinterpret_cast< const void* >( &h_scramble ) );
        return b;
    }

    const std::vector< int64_t > values { 0, 1, -1, 42, -1000, 0x7fffffff, -0x80000000LL, INT64_MAX, INT64_MIN };

    const ir::IRFunction& ir_of( const Jit& j )
    { return *j.irm.get( ir::FunctionRef { 0 } ); }
}

TEST_CASE( "stack slot store and load round trip", "[jit][stack]" )
{
    const auto c = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                  []( ir::IRBuilder& b, const Args& a )
                                                  {
                                                      const auto ss = b.create_stack_slot( 8, 8 );
                                                      const auto p  = b.stack_addr( Type::ptr, ss );
                                                      b.store( a[ 0 ], p );
                                                      b.ret( b.load( Type::i64, p ) );
                                                  } );
    for ( const auto v : values ) CHECK( c( v ) == v );
}

TEST_CASE( "stack slot narrow stores land at their offsets", "[jit][stack]" )
{
    const auto c = compile< uint64_t( uint64_t ) >( Type::i64, { Type::i64 },
                                                    []( ir::IRBuilder& b, const Args& a )
                                                    {
                                                        const auto ss = b.create_stack_slot( 16, 8 );
                                                        b.store( k64( b, 0 ), b.stack_addr( Type::ptr, ss ) );
                                                        b.store( k64( b, 0 ), b.stack_addr( Type::ptr, ss, 8 ) );
                                                        b.store( b.ireduce( Type::i8, a[ 0 ] ), b.stack_addr( Type::ptr, ss, 1 ) );
                                                        b.store( b.ireduce( Type::i16, a[ 0 ] ), b.stack_addr( Type::ptr, ss, 2 ) );
                                                        b.store( b.ireduce( Type::i32, a[ 0 ] ), b.stack_addr( Type::ptr, ss, 4 ) );
                                                        b.store( b.ireduce( Type::i32, a[ 0 ] ), b.stack_addr( Type::ptr, ss ), 12 );
                                                        const auto lo = b.load( Type::i64, b.stack_addr( Type::ptr, ss ) );
                                                        const auto hi = b.load( Type::i64, b.stack_addr( Type::ptr, ss, 8 ) );
                                                        b.ret( b.bxor( lo, b.ushr( hi, k64( b, 32 ) ) ) );
                                                    } );
    for ( const auto x : { uint64_t { 0x1122334455667788 }, uint64_t { 0xffffffffffffffff }, uint64_t { 0x80 } } )
    {
        const uint64_t b8  = x & 0xff;
        const uint64_t b16 = x & 0xffff;
        const uint64_t b32 = x & 0xffffffff;
        const uint64_t lo  = ( b8 << 8 ) | ( b16 << 16 ) | ( b32 << 32 );
        CHECK( c( x ) == ( lo ^ b32 ) );
    }
}

TEST_CASE( "stack slots do not overlap", "[jit][stack]" )
{
    const auto c = compile< int64_t( int64_t, int64_t, int64_t ) >(
        Type::i64, { Type::i64, Type::i64, Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            const auto s0 = b.create_stack_slot( 8, 8 );
            const auto s1 = b.create_stack_slot( 1, 1 );
            const auto s2 = b.create_stack_slot( 8, 8 );
            b.store( a[ 0 ], b.stack_addr( Type::ptr, s0 ) );
            b.store( b.ireduce( Type::i8, a[ 1 ] ), b.stack_addr( Type::ptr, s1 ) );
            b.store( a[ 2 ], b.stack_addr( Type::ptr, s2 ) );
            const auto x = b.load( Type::i64, b.stack_addr( Type::ptr, s0 ) );
            const auto y = b.sextend( Type::i64, b.load( Type::i8, b.stack_addr( Type::ptr, s1 ) ) );
            const auto z = b.load( Type::i64, b.stack_addr( Type::ptr, s2 ) );
            b.ret( b.iadd( b.imul( x, k64( b, 1000 ) ), b.iadd( b.imul( y, k64( b, 10 ) ), z ) ) );
        } );
    CHECK( c( 1, 2, 3 ) == 1023 );
    CHECK( c( -7, -1, 9 ) == -7001 );
    CHECK( c( 0, 300, 0 ) == 440 );
}

TEST_CASE( "stack slot address escapes to a host function", "[jit][stack]" )
{
    Jit        j { host_builder( ) };
    const auto write = j.import( "h_write", Type::unit, { Type::ptr, Type::i64 } );
    const auto id    = j.define( "f", Type::i64, { Type::i64 },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                  const auto ss = b.create_stack_slot( 16, 8 );
                                  const auto p  = b.stack_addr( Type::ptr, ss );
                                  const auto fr = j.mod.declare_func_in_func( write, b );
                                  ( void ) b.call( fr, Args { p, a[ 0 ] } );
                                  ( void ) b.call( fr, Args { b.stack_addr( Type::ptr, ss, 8 ), b.ineg( a[ 0 ] ) } );
                                  const auto x = b.load( Type::i64, p );
                                  const auto y = b.load( Type::i64, p, 8 );
                                  b.ret( b.isub( b.imul( x, k64( b, 3 ) ), y ) );
                                 } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( id );
    for ( const auto v : { int64_t { 0 }, int64_t { 5 }, int64_t { -12 }, int64_t { 1 } << 40 } ) CHECK( f( v ) == v * 4 );
}

TEST_CASE( "stack slot survives calls that use the shadow space", "[jit][stack]" )
{
    Jit        j { host_builder( ) };
    const auto fill      = j.import( "h_fill", Type::unit, { Type::ptr, Type::i64, Type::i64 } );
    const auto scramble  = j.import( "h_scramble", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 } );
    const auto id        = j.define( "f", Type::i64, { Type::i64 },
                                     [ & ]( ir::IRBuilder& b, const Args& a )
                                     {
                                  const auto ss = b.create_stack_slot( 32, 16 );
                                  const auto p  = b.stack_addr( Type::ptr, ss );
                                  ( void ) b.call( j.mod.declare_func_in_func( fill, b ), Args { p, k64( b, 32 ), a[ 0 ] } );
                                  const auto sc = j.mod.declare_func_in_func( scramble, b );
                                  const auto r  = b.call( sc, Args { a[ 0 ], a[ 0 ], a[ 0 ], a[ 0 ] } );
                                  const auto r2 = b.call( sc, Args { r, r, r, r } );
                                  const auto w0 = b.load( Type::i64, p );
                                  const auto w3 = b.load( Type::i64, p, 24 );
                                  b.ret( b.bxor( b.bxor( w0, w3 ), r2 ) );
                                     } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( id );
    for ( const auto seed : { int64_t { 0 }, int64_t { 3 }, int64_t { 200 } } )
    {
        uint8_t buf[ 32 ];
        h_fill( buf, 32, seed );
        int64_t w0 { }, w3 { };
        std::memcpy( &w0, buf, 8 );
        std::memcpy( &w3, buf + 24, 8 );
        const auto r = h_scramble( seed, seed, seed, seed );
        CHECK( f( seed ) == ( w0 ^ w3 ^ h_scramble( r, r, r, r ) ) );
    }
}

TEST_CASE( "stack slots and spill slots do not overlap", "[jit][stack]" )
{
    const auto c = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                           []( ir::IRBuilder& b, const Args& a )
                                                           {
                                                               const auto            ss = b.create_stack_slot( 24, 8 );
                                                               std::vector< ValueRef > vs;
                                                               for ( int64_t k { 0 }; k < 24; ++k )
                                                                   vs.push_back( b.iadd( b.imul( a[ 0 ], k64( b, k + 3 ) ), a[ 1 ] ) );
                                                               const auto p = b.stack_addr( Type::ptr, ss );
                                                               b.store( vs[ 5 ], p );
                                                               b.store( vs[ 11 ], p, 8 );
                                                               b.store( vs[ 23 ], p, 16 );
                                                               auto sum = b.load( Type::i64, p );
                                                               for ( auto it = vs.rbegin( ); it != vs.rend( ); ++it )
                                                                   sum = b.iadd( b.imul( sum, k64( b, 3 ) ), *it );
                                                               sum = b.iadd( sum, b.load( Type::i64, p, 8 ) );
                                                               b.ret( b.isub( sum, b.load( Type::i64, p, 16 ) ) );
                                                           } );
    for ( const auto x : { int64_t { 0 }, int64_t { 1 }, int64_t { -9 }, int64_t { 123456789 } } )
        for ( const auto y : { int64_t { 0 }, int64_t { 7 }, int64_t { -1 } } )
        {
            const uint64_t args[] = { u( x ), u( y ) };
            CHECK( u( c( x, y ) ) == Interp { }.run( ir_of( *c.jit ), args ) );
        }
}

TEST_CASE( "stack slots get their requested alignment", "[jit][stack]" )
{
    const bool with_call = GENERATE( false, true );
    Jit        j { host_builder( ) };
    const auto write = j.import( "h_write", Type::unit, { Type::ptr, Type::i64 } );
    const auto id    = j.define( "f", Type::i64, { Type::i64 },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                  const auto s0 = b.create_stack_slot( 3, 1 );
                                  const auto s1 = b.create_stack_slot( 16, 16 );
                                  const auto s2 = b.create_stack_slot( 5, 1 );
                                  const auto s3 = b.create_stack_slot( 8, 8 );
                                  const auto s4 = b.create_stack_slot( 4, 4 );
                                  b.store( b.ireduce( Type::i8, a[ 0 ] ), b.stack_addr( Type::ptr, s0 ) );
                                  b.store( b.ireduce( Type::i8, a[ 0 ] ), b.stack_addr( Type::ptr, s2 ) );
                                  if ( with_call )
                                      ( void ) b.call( j.mod.declare_func_in_func( write, b ), Args { b.stack_addr( Type::ptr, s3 ), a[ 0 ] } );
                                  const auto m1 = b.band( b.stack_addr( Type::ptr, s1 ), k64( b, 15 ) );
                                  const auto m3 = b.band( b.stack_addr( Type::ptr, s3 ), k64( b, 7 ) );
                                  const auto m4 = b.band( b.stack_addr( Type::ptr, s4 ), k64( b, 3 ) );
                                  b.ret( b.bor( b.bor( m1, m3 ), m4 ) );
                                 } );
    j.finalise( );
    CHECK( j.get< int64_t( int64_t ) >( id )( 0x55 ) == 0 );
}

TEST_CASE( "stack slots larger than a byte displacement", "[jit][stack]" )
{
    const auto c = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                  []( ir::IRBuilder& b, const Args& a )
                                                  {
                                                      const auto small = b.create_stack_slot( 8, 8 );
                                                      const auto big   = b.create_stack_slot( 400, 16 );
                                                      b.store( a[ 0 ], b.stack_addr( Type::ptr, small ) );
                                                      for ( int32_t off { 0 }; off < 400; off += 96 )
                                                          b.store( b.iadd( a[ 0 ], k64( b, off ) ), b.stack_addr( Type::ptr, big, off ) );
                                                      auto sum = b.load( Type::i64, b.stack_addr( Type::ptr, small ) );
                                                      for ( int32_t off { 0 }; off < 400; off += 96 )
                                                          sum = b.iadd( sum, b.load( Type::i64, b.stack_addr( Type::ptr, big, off ) ) );
                                                      b.ret( sum );
                                                  } );
    for ( const auto v : { int64_t { 0 }, int64_t { 10 }, int64_t { -77 } } ) CHECK( c( v ) == v * 6 + ( 0 + 96 + 192 + 288 + 384 ) );
}

TEST_CASE( "stack slot used as a loop variable", "[jit][stack]" )
{
    const auto c = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                                  []( ir::IRBuilder& b, const Args& a )
                                                  {
                                                      const auto ss   = b.create_stack_slot( 16, 8 );
                                                      const auto loop = b.add_block( { } );
                                                      const auto body = b.add_block( { } );
                                                      const auto done = b.add_block( { } );
                                                      const auto p    = b.stack_addr( Type::ptr, ss );
                                                      b.store( k64( b, 0 ), p );
                                                      b.store( k64( b, 0 ), p, 8 );
                                                      b.br( loop, { } );

                                                      b.switch_to_block( loop );
                                                      const auto i = b.load( Type::i64, p );
                                                      b.brif( b.icmp( ir::ICmpCC::slt, i, a[ 0 ] ), body, { }, done, { } );

                                                      b.switch_to_block( body );
                                                      const auto acc = b.load( Type::i64, p, 8 );
                                                      b.store( b.iadd( acc, b.imul( i, i ) ), p, 8 );
                                                      b.store( b.iadd( i, k64( b, 1 ) ), p );
                                                      b.br( loop, { } );

                                                      b.switch_to_block( done );
                                                      b.ret( b.load( Type::i64, p, 8 ) );
                                                  } );
    for ( const int64_t n : { 0, 1, 5, 100 } ) CHECK( c( n ) == ( n - 1 ) * n * ( 2 * n - 1 ) / 6 );
}

TEST_CASE( "each recursive call gets its own stack slot", "[jit][stack]" )
{
    Jit        j;
    const auto id = j.declare( "f", Type::i64, { Type::i64 } );
    j.build( id,
             [ & ]( ir::IRBuilder& b, const Args& a )
             {
                 const auto ss   = b.create_stack_slot( 8, 8 );
                 const auto self = j.mod.declare_func_in_func( id, b );
                 const auto rec  = b.add_block( { } );
                 const auto base = b.add_block( { } );
                 const auto p    = b.stack_addr( Type::ptr, ss );
                 b.store( a[ 0 ], p );
                 b.brif( b.icmp( ir::ICmpCC::sgt, a[ 0 ], k64( b, 0 ) ), rec, { }, base, { } );

                 b.switch_to_block( rec );
                 const auto inner = b.call( self, Args { b.isub( a[ 0 ], k64( b, 1 ) ) } );
                 b.ret( b.iadd( b.imul( inner, k64( b, 2 ) ), b.load( Type::i64, p ) ) );

                 b.switch_to_block( base );
                 b.ret( b.load( Type::i64, p ) );
             } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( id );

    const auto want = []( int64_t n )
    {
        int64_t r { 0 };
        for ( int64_t k { 0 }; k <= n; ++k ) r = r * 2 + k;
        return r;
    };
    for ( const int64_t n : { 0, 1, 2, 10, 30 } ) CHECK( f( n ) == want( n ) );
}

TEST_CASE( "interpreter agrees with the jit on stack slots", "[jit][stack][interp]" )
{
    const auto c = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                           []( ir::IRBuilder& b, const Args& a )
                                                           {
                                                               const auto s0 = b.create_stack_slot( 2, 2 );
                                                               const auto s1 = b.create_stack_slot( 16, 16 );
                                                               b.store( b.ireduce( Type::i16, a[ 0 ] ), b.stack_addr( Type::ptr, s0 ) );
                                                               b.store( a[ 1 ], b.stack_addr( Type::ptr, s1, 8 ) );
                                                               b.store( a[ 0 ], b.stack_addr( Type::ptr, s1 ) );
                                                               const auto x = b.sextend( Type::i64, b.load( Type::i16, b.stack_addr( Type::ptr, s0 ) ) );
                                                               const auto y = b.load( Type::i64, b.stack_addr( Type::ptr, s1, 8 ) );
                                                               const auto z = b.load( Type::i32, b.stack_addr( Type::ptr, s1, 4 ) );
                                                               b.ret( b.iadd( b.isub( x, y ), b.uextend( Type::i64, z ) ) );
                                                           } );
    for ( const auto x : values )
        for ( const auto y : { int64_t { 0 }, int64_t { -3 }, int64_t { 99 } } )
        {
            const uint64_t args[] = { u( x ), u( y ) };
            CHECK( u( c( x, y ) ) == Interp { }.run( ir_of( *c.jit ), args ) );
        }
}
