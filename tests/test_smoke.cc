#include "support.hh"

using namespace xt;

TEST_CASE( "jit compiles and runs an identity function", "[smoke]" )
{
    auto f = compile< int64_t( int64_t ) >( Type::i64, { Type::i64 },
                                            []( ir::IRBuilder& b, const Args& a )
                                            {
                                                b.ret( a[ 0 ] );
                                            } );
    CHECK( f( 0 ) == 0 );
    CHECK( f( 42 ) == 42 );
    CHECK( f( -1 ) == -1 );
}

TEST_CASE( "interpreter agrees with jit on a small expression", "[smoke]" )
{
    auto f = compile< int64_t( int64_t, int64_t ) >(
        Type::i64, { Type::i64, Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            b.ret( b.isub( b.imul( a[ 0 ], a[ 1 ] ), b.iadd( a[ 0 ], b.iconst( Type::i64, 5 ) ) ) );
        } );
    const auto& fn = *f.jit->irm.get( ir::FunctionRef { 0 } );
    for ( const auto x : edge_values( ) )
        for ( const auto y : { int64_t { 0 }, int64_t { 3 }, int64_t { -9 } } )
        {
            const uint64_t args[] = { static_cast< uint64_t >( x ), static_cast< uint64_t >( y ) };
            CHECK( static_cast< uint64_t >( f( x, y ) ) == Interp { }.run( fn, args ) );
        }
}
