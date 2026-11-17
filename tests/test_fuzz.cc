#include "support.hh"

#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <random>

using namespace xt;

namespace
{
    constexpr std::array int_types { Type::i8, Type::i16, Type::i32, Type::i64 };

    class Gen
    {
    public:
        Gen( ir::IRBuilder& b, const uint64_t seed, const bool memory ) : m_b { b }, m_rng { seed }, m_memory { memory } { }

        void add( const ValueRef v, const Type ty )
        { m_pool.push_back( { v, ty } ); }

        [[nodiscard]] uint64_t pick( const uint64_t n )
        { return std::uniform_int_distribution< uint64_t > { 0, n - 1 }( m_rng ); }

        [[nodiscard]] ValueRef any_of( const Type ty )
        {
            std::vector< ValueRef > c;
            for ( const auto& [ v, t ] : m_pool )
                if ( t == ty ) c.push_back( v );
            if ( c.empty( ) ) return as( ty, m_pool[ pick( m_pool.size( ) ) ] );
            return c[ pick( c.size( ) ) ];
        }

        [[nodiscard]] ValueRef as( const Type ty, const std::pair< ValueRef, Type > src )
        {
            const auto [ v, t ] = src;
            if ( t == ty ) return v;
            if ( ir::bit_width( t ) > ir::bit_width( ty ) ) return m_b.ireduce( ty, v );
            return pick( 2 ) ? m_b.uextend( ty, v ) : m_b.sextend( ty, v );
        }

        [[nodiscard]] ValueRef i64( )
        { return any_of( Type::i64 ); }

        [[nodiscard]] uint64_t constant( )
        {
            const auto& e = edge_values( );
            if ( pick( 3 ) == 0 ) return m_rng( );
            return static_cast< uint64_t >( e[ pick( e.size( ) ) ] );
        }

        void step( const ValueRef buf )
        {
            const auto ty = int_types[ pick( int_types.size( ) ) ];
            switch ( pick( m_memory ? 22 : 20 ) )
            {
                case 0: add( m_b.iconst( ty, mask( ty, constant( ) ) ), ty ); break;
                case 1: add( m_b.iadd( any_of( ty ), pick( 3 ) ? any_of( ty ) : m_b.iconst( ty, mask( ty, constant( ) ) ) ), ty ); break;
                case 2: add( m_b.isub( any_of( ty ), any_of( ty ) ), ty ); break;
                case 3: add( m_b.imul( any_of( ty ), any_of( ty ) ), ty ); break;
                case 4: add( m_b.band( any_of( ty ), any_of( ty ) ), ty ); break;
                case 5: add( m_b.bor( any_of( ty ), any_of( ty ) ), ty ); break;
                case 6: add( m_b.bxor( any_of( ty ), any_of( ty ) ), ty ); break;
                case 7: add( m_b.bnot( any_of( ty ) ), ty ); break;
                case 8: add( m_b.ineg( any_of( ty ) ), ty ); break;
                case 9:
                {
                    const auto count = pick( 2 ) ? any_of( int_types[ pick( 4 ) ] ) : m_b.iconst( Type::i64, constant( ) );
                    add( m_b.ishl( any_of( ty ), count ), ty );
                    break;
                }
                case 10:
                {
                    const auto count = pick( 2 ) ? any_of( int_types[ pick( 4 ) ] ) : m_b.iconst( Type::i64, constant( ) );
                    add( pick( 2 ) ? m_b.ushr( i64( ), count ) : m_b.sshr( i64( ), count ), Type::i64 );
                    break;
                }
                case 11:
                case 12:
                {
                    const auto d = m_b.bor( m_b.ushr( i64( ), m_b.iconst( Type::i64, 1 + pick( 40 ) ) ), m_b.iconst( Type::i64, 1 ) );
                    const auto n = i64( );
                    ValueRef   r { };
                    switch ( pick( 4 ) )
                    {
                        case 0: r = m_b.idiv( n, d ); break;
                        case 1: r = m_b.udiv( n, d ); break;
                        case 2: r = m_b.srem( n, d ); break;
                        default: r = m_b.urem( n, d ); break;
                    }
                    add( r, Type::i64 );
                    break;
                }
                case 13:
                case 14:
                {
                    const auto cc  = static_cast< ir::ICmpCC >( pick( 10 ) );
                    const auto rhs = pick( 3 ) ? i64( ) : m_b.iconst( Type::i64, constant( ) );
                    const auto c   = pick( 4 ) ? m_b.icmp( cc, i64( ), rhs ) : m_b.icmp( cc, rhs, i64( ) );
                    add( c, Type::i8 );
                    break;
                }
                case 15:
                {
                    const auto cond = any_of( int_types[ pick( 4 ) ] );
                    add( m_b.select( cond, any_of( ty ), any_of( ty ) ), ty );
                    break;
                }
                case 16:
                case 17:
                {
                    const auto src = m_pool[ pick( m_pool.size( ) ) ];
                    if ( src.second != ty ) add( as( ty, src ), ty );
                    break;
                }
                case 18:
                case 19: add( m_b.iadd( i64( ), m_b.iconst( Type::i64, constant( ) ) ), Type::i64 ); break;
                case 20:
                {
                    const auto off = static_cast< int32_t >( pick( 8 ) * 8 + pick( 8 - ir::bit_width( ty ) / 8 + 1 ) );
                    add( m_b.load( ty, buf, off ), ty );
                    break;
                }
                default:
                {
                    const auto off = static_cast< int32_t >( pick( 8 ) * 8 + pick( 8 - ir::bit_width( ty ) / 8 + 1 ) );
                    m_b.store( any_of( ty ), buf, off );
                    break;
                }
            }
        }

        [[nodiscard]] ValueRef fold( )
        {
            auto acc = m_b.iconst( Type::i64, 0x9e3779b97f4a7c15ULL );
            for ( const auto& [ v, t ] : m_pool )
            {
                const auto w = t == Type::i64 ? v : m_b.uextend( Type::i64, v );
                acc          = m_b.bxor( m_b.imul( acc, m_b.iconst( Type::i64, 31 ) ), w );
            }
            return acc;
        }

        std::vector< std::pair< ValueRef, Type > >& pool( )
        { return m_pool; }

    private:
        ir::IRBuilder&                             m_b;
        std::mt19937_64                            m_rng;
        bool                                       m_memory;
        std::vector< std::pair< ValueRef, Type > > m_pool;
    };

    struct Program
    {
        std::unique_ptr< Jit > jit;
        const ir::IRFunction*  fn { };
        int64_t ( *call )( int64_t, int64_t, int64_t, int64_t* ) { };
    };

    template < typename Body >
    [[nodiscard]] Program make( Body&& body )
    {
        Program p;
        p.jit         = std::make_unique< Jit >( );
        const auto id = p.jit->define( "fuzz", Type::i64, { Type::i64, Type::i64, Type::i64, Type::ptr }, std::forward< Body >( body ) );
        p.jit->finalise( );
        p.fn   = p.jit->irm.get( ir::FunctionRef { 0 } );
        p.call = p.jit->get< int64_t( int64_t, int64_t, int64_t, int64_t* ) >( id );
        return p;
    }

    void check( const Program& p, const uint64_t seed )
    {
        std::mt19937_64 rng { seed ^ 0xabcdef };
        const auto&     e = edge_values( );
        for ( int round { 0 }; round < 24; ++round )
        {
            int64_t a[ 3 ];
            for ( auto& x : a ) x = round < 8 ? e[ rng( ) % e.size( ) ] : static_cast< int64_t >( rng( ) );

            std::array< int64_t, 9 > mem_jit { };
            for ( auto& m : mem_jit ) m = static_cast< int64_t >( rng( ) );
            auto mem_ref = mem_jit;

            const uint64_t args[] = { static_cast< uint64_t >( a[ 0 ] ), static_cast< uint64_t >( a[ 1 ] ),
                                      static_cast< uint64_t >( a[ 2 ] ), reinterpret_cast< uint64_t >( mem_ref.data( ) ) };
            const auto     want   = Interp { }.run( *p.fn, args );
            const auto     got    = static_cast< uint64_t >( p.call( a[ 0 ], a[ 1 ], a[ 2 ], mem_jit.data( ) ) );

            INFO( "seed " << seed << " round " << round << " args " << a[ 0 ] << ", " << a[ 1 ] << ", " << a[ 2 ] );
            INFO( print( p.jit->irm ) );
            REQUIRE( got == want );
            REQUIRE( mem_jit == mem_ref );
        }
    }

    void seed_pool( Gen& g, ir::IRBuilder& b, const Args& a )
    {
        g.add( a[ 0 ], Type::i64 );
        g.add( a[ 1 ], Type::i64 );
        g.add( a[ 2 ], Type::i64 );
        g.add( b.ireduce( Type::i32, a[ 0 ] ), Type::i32 );
        g.add( b.ireduce( Type::i16, a[ 1 ] ), Type::i16 );
        g.add( b.ireduce( Type::i8, a[ 2 ] ), Type::i8 );
    }
} // namespace

TEST_CASE( "fuzz straight line programs match the interpreter", "[fuzz][straight]" )
{
    const auto seed = GENERATE( range< uint64_t >( 1, 301 ) );
    const auto len  = 4 + seed % 60;

    const auto p = make(
        [ & ]( ir::IRBuilder& b, const Args& a )
        {
            Gen g { b, seed, false };
            seed_pool( g, b, a );
            for ( uint64_t k { 0 }; k < len; ++k ) g.step( a[ 3 ] );
            b.ret( g.fold( ) );
        } );
    check( p, seed );
}

TEST_CASE( "fuzz straight line programs with memory match the interpreter", "[fuzz][memory]" )
{
    const auto seed = GENERATE( range< uint64_t >( 1000, 1201 ) );
    const auto len  = 4 + seed % 50;

    const auto p = make(
        [ & ]( ir::IRBuilder& b, const Args& a )
        {
            Gen g { b, seed, true };
            seed_pool( g, b, a );
            for ( uint64_t k { 0 }; k < len; ++k ) g.step( a[ 3 ] );
            b.ret( g.fold( ) );
        } );
    check( p, seed );
}

TEST_CASE( "fuzz high register pressure programs match the interpreter", "[fuzz][pressure]" )
{
    const auto seed = GENERATE( range< uint64_t >( 2000, 2061 ) );

    const auto p = make(
        [ & ]( ir::IRBuilder& b, const Args& a )
        {
            Gen g { b, seed, seed % 2 == 0 };
            seed_pool( g, b, a );
            for ( int k { 0 }; k < 30; ++k )
                g.add( b.iadd( g.i64( ), b.iconst( Type::i64, static_cast< uint64_t >( k * 7919 ) ) ), Type::i64 );
            for ( uint64_t k { 0 }; k < 80 + seed % 40; ++k ) g.step( a[ 3 ] );
            b.ret( g.fold( ) );
        } );
    check( p, seed );
}

TEST_CASE( "fuzz diamond programs match the interpreter", "[fuzz][control]" )
{
    const auto seed = GENERATE( range< uint64_t >( 3000, 3201 ) );

    const auto p = make(
        [ & ]( ir::IRBuilder& b, const Args& a )
        {
            Gen  g { b, seed, seed % 3 == 0 };
            auto live = std::vector< ValueRef > { a[ 0 ], a[ 1 ], a[ 2 ] };

            const auto diamonds = 1 + seed % 6;
            for ( uint64_t d { 0 }; d < diamonds; ++d )
            {
                g.pool( ).clear( );
                for ( const auto v : live ) g.add( v, Type::i64 );
                for ( uint64_t k { 0 }; k < 3 + g.pick( 6 ); ++k ) g.step( a[ 3 ] );

                const auto cond = g.pick( 2 ) ? b.icmp( static_cast< ir::ICmpCC >( g.pick( 10 ) ), g.i64( ), g.i64( ) )
                                              : g.any_of( int_types[ g.pick( 4 ) ] );

                const std::vector< Type > tys( live.size( ) + 1, Type::i64 );
                const auto                then_b = b.add_block( std::span< const Type > { tys } );
                const auto                else_b = b.add_block( std::span< const Type > { tys } );
                const auto                join   = b.add_block( std::span< const Type > { tys } );

                Args then_args = live;
                then_args.push_back( g.fold( ) );
                Args else_args = then_args;
                std::shuffle( else_args.begin( ), else_args.end( ), std::mt19937_64 { seed + d } );
                b.brif( cond, then_b, then_args, else_b, else_args );

                for ( const auto blk : { then_b, else_b } )
                {
                    b.switch_to_block( blk );
                    g.pool( ).clear( );
                    for ( uint16_t k { 0 }; k < tys.size( ); ++k ) g.add( b.block_param( blk, k ), Type::i64 );
                    for ( uint64_t k { 0 }; k < 2 + g.pick( 6 ); ++k ) g.step( a[ 3 ] );
                    Args out;
                    for ( std::size_t k { 0 }; k < tys.size( ); ++k ) out.push_back( g.i64( ) );
                    b.br( join, out );
                }

                b.switch_to_block( join );
                live.clear( );
                for ( uint16_t k { 0 }; k < tys.size( ); ++k ) live.push_back( b.block_param( join, k ) );
                if ( live.size( ) > 6 ) live.erase( live.begin( ) );
            }

            g.pool( ).clear( );
            for ( const auto v : live ) g.add( v, Type::i64 );
            b.ret( g.fold( ) );
        } );
    check( p, seed );
}

TEST_CASE( "fuzz loop programs match the interpreter", "[fuzz][loop]" )
{
    const auto seed = GENERATE( range< uint64_t >( 4000, 4151 ) );

    const auto p = make(
        [ & ]( ir::IRBuilder& b, const Args& a )
        {
            Gen        g { b, seed, seed % 2 == 0 };
            const auto carried = 1 + seed % 7;

            std::vector< Type > tys( carried + 1, Type::i64 );
            const auto          header = b.add_block( std::span< const Type > { tys } );
            const auto          body   = b.add_block( std::span< const Type > { tys } );
            const auto          exit   = b.add_block( std::span< const Type > { tys } );

            Args init { b.iconst( Type::i64, 0 ) };
            for ( uint64_t k { 0 }; k < carried; ++k ) init.push_back( a[ k % 3 ] );
            b.br( header, init );

            b.switch_to_block( header );
            Args hp;
            for ( uint16_t k { 0 }; k < tys.size( ); ++k ) hp.push_back( b.block_param( header, k ) );
            const auto trip = b.iconst( Type::i64, 1 + seed % 23 );
            b.brif( b.icmp( ir::ICmpCC::slt, hp[ 0 ], trip ), body, hp, exit, hp );

            b.switch_to_block( body );
            g.pool( ).clear( );
            for ( uint16_t k { 0 }; k < tys.size( ); ++k ) g.add( b.block_param( body, k ), Type::i64 );
            for ( uint64_t k { 0 }; k < 3 + g.pick( 12 ); ++k ) g.step( a[ 3 ] );
            Args next { b.iadd( b.block_param( body, 0 ), b.iconst( Type::i64, 1 ) ) };
            for ( uint64_t k { 0 }; k < carried; ++k )
                next.push_back( g.pick( 4 ) ? g.i64( ) : b.block_param( body, static_cast< uint16_t >( 1 + ( k + 1 ) % carried ) ) );
            b.br( header, next );

            b.switch_to_block( exit );
            g.pool( ).clear( );
            for ( uint16_t k { 0 }; k < tys.size( ); ++k ) g.add( b.block_param( exit, k ), Type::i64 );
            b.ret( g.fold( ) );
        } );
    check( p, seed );
}

namespace
{
    extern "C" int64_t xt_fuzz_mix( const int64_t a, const int64_t b )
    { return ( a * 3 ) ^ ( b >> 7 ) ^ 0x5a5a; }

    extern "C" int32_t xt_fuzz_narrow( const int32_t a, const int8_t b, const int16_t c )
    { return a * 5 - b + c; }
} // namespace

TEST_CASE( "fuzz programs with host calls match the interpreter", "[fuzz][calls]" )
{
    const auto seed = GENERATE( range< uint64_t >( 5000, 5121 ) );

    jit::JITBuilder builder { isa::lookup_host( ) };
    builder.symbol( "mix", reinterpret_cast< const void* >( &xt_fuzz_mix ) );
    builder.symbol( "narrow", reinterpret_cast< const void* >( &xt_fuzz_narrow ) );
    auto j = std::make_unique< Jit >( std::move( builder ) );

    const auto mix_id    = j->import( "mix", Type::i64, { Type::i64, Type::i64 } );
    const auto narrow_id = j->import( "narrow", Type::i32, { Type::i32, Type::i8, Type::i16 } );

    const auto id = j->define( "fuzz", Type::i64, { Type::i64, Type::i64, Type::i64, Type::ptr },
                               [ & ]( ir::IRBuilder& b, const Args& a )
                               {
                                   const auto mix    = j->mod.declare_func_in_func( mix_id, b );
                                   const auto narrow = j->mod.declare_func_in_func( narrow_id, b );
                                   Gen        g { b, seed, seed % 2 == 1 };
                                   seed_pool( g, b, a );
                                   for ( uint64_t k { 0 }; k < 10 + seed % 50; ++k )
                                   {
                                       g.step( a[ 3 ] );
                                       if ( g.pick( 4 ) == 0 )
                                       {
                                           const ValueRef args[] = { g.i64( ), g.i64( ) };
                                           g.add( b.call( mix, args ), Type::i64 );
                                       }
                                       if ( g.pick( 8 ) == 0 )
                                       {
                                           const ValueRef args[] = { g.any_of( Type::i32 ), g.any_of( Type::i8 ), g.any_of( Type::i16 ) };
                                           g.add( b.call( narrow, args ), Type::i32 );
                                       }
                                   }
                                   b.ret( g.fold( ) );
                               } );
    j->finalise( );

    Program p;
    p.fn   = j->irm.get( ir::FunctionRef { 0 } );
    p.call = j->get< int64_t( int64_t, int64_t, int64_t, int64_t* ) >( id );
    p.jit  = std::move( j );

    const auto call = []( const uint32_t f, const std::span< const uint64_t > a ) -> uint64_t
    {
        if ( f == 0 ) return static_cast< uint64_t >( xt_fuzz_mix( static_cast< int64_t >( a[ 0 ] ), static_cast< int64_t >( a[ 1 ] ) ) );
        return static_cast< uint64_t >( static_cast< uint32_t >(
            xt_fuzz_narrow( static_cast< int32_t >( a[ 0 ] ), static_cast< int8_t >( a[ 1 ] ), static_cast< int16_t >( a[ 2 ] ) ) ) );
    };

    std::mt19937_64 rng { seed };
    for ( int round { 0 }; round < 16; ++round )
    {
        const int64_t x = static_cast< int64_t >( rng( ) ), y = edge_values( )[ rng( ) % edge_values( ).size( ) ],
                      z = static_cast< int64_t >( rng( ) % 1000 );
        std::array< int64_t, 9 > mem_jit { };
        for ( auto& m : mem_jit ) m = static_cast< int64_t >( rng( ) );
        auto           mem_ref = mem_jit;
        const uint64_t args[]  = { static_cast< uint64_t >( x ), static_cast< uint64_t >( y ), static_cast< uint64_t >( z ),
                                   reinterpret_cast< uint64_t >( mem_ref.data( ) ) };
        const auto     want    = Interp { .call = call }.run( *p.fn, args );
        INFO( "seed " << seed << " round " << round );
        INFO( print( p.jit->irm ) );
        REQUIRE( static_cast< uint64_t >( p.call( x, y, z, mem_jit.data( ) ) ) == want );
        REQUIRE( mem_jit == mem_ref );
    }
}
