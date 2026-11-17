#include "support.hh"

#include <algorithm>
#include <array>
#include <functional>
#include <span>
#include <string>

using namespace xt;
using codegen::PReg;
using codegen::RegClass;
using regalloc::Allocation;

namespace
{
    using x86_64::X86Inst;

    int64_t mix( const int64_t a, const int64_t b )
    { return static_cast< int64_t >( static_cast< uint64_t >( a ) * 31u + ( static_cast< uint64_t >( b ) ^ 0x5bd1e995u ) ); }

    int64_t mix4( const int64_t a, const int64_t b, const int64_t c, const int64_t d )
    { return mix( mix( a, b ), mix( c, d ) ) ^ d; }

    int64_t seven( )
    { return 7; }

    struct Funcs
    {
        ir::FuncRef mix { };
        ir::FuncRef mix4 { };
        ir::FuncRef seven { };
    };

    using Body = std::function< void( ir::IRBuilder&, const Args&, const Funcs& ) >;

    ValueRef k64( ir::IRBuilder& b, const int64_t v )
    { return b.iconst( Type::i64, static_cast< uint64_t >( v ) ); }

    ValueRef fold_all( ir::IRBuilder& b, const std::vector< ValueRef >& v, ValueRef acc )
    {
        for ( std::size_t i { v.size( ) }; i-- > 0; )
        {
            switch ( i % 4 )
            {
                case 0: acc = b.bxor( acc, v[ i ] ); break;
                case 1: acc = b.iadd( acc, v[ i ] ); break;
                case 2: acc = b.isub( acc, v[ i ] ); break;
                default: acc = b.iadd( b.imul( acc, k64( b, 3 ) ), v[ i ] ); break;
            }
        }
        return acc;
    }

    void wide( ir::IRBuilder& b, const Args& a, const int n )
    {
        std::vector< ValueRef > v;
        for ( int i { 0 }; i < n; ++i )
        {
            const auto t = b.imul( a[ 0 ], k64( b, 2 * i + 3 ) );
            if ( i % 4 == 3 ) v.push_back( b.iadd( t, k64( b, 0x100000000LL * ( i + 1 ) + i ) ) );
            else
                v.push_back( b.iadd( t, a[ 1 ] ) );
        }
        b.ret( fold_all( b, v, v.back( ) ) );
    }

    void across_calls( ir::IRBuilder& b, const Args& a, const Funcs& f, const int n )
    {
        std::vector< ValueRef > v;
        for ( int i { 0 }; i < n; ++i ) v.push_back( b.bxor( b.imul( a[ 0 ], k64( b, i + 1 ) ), b.iadd( a[ 1 ], k64( b, i ) ) ) );

        const ValueRef first[] = { a[ 0 ], a[ 1 ] };
        auto           c       = b.call( f.mix, first );
        for ( int k { 0 }; k < n; k += 2 )
        {
            if ( k % 6 == 4 && k + 1 < n )
            {
                const ValueRef args[] = { v[ k ], v[ k + 1 ], c, a[ 0 ] };
                c                     = b.call( f.mix4, args );
            }
            else
            {
                const ValueRef args[] = { c, v[ k ] };
                c                     = b.call( f.mix, args );
            }
        }
        c = b.iadd( c, b.call( f.seven, { } ) );

        std::vector< ValueRef > scaled;
        for ( int i { 0 }; i < n; ++i ) scaled.push_back( b.imul( v[ i ], k64( b, i + 5 ) ) );
        b.ret( fold_all( b, scaled, c ) );
    }

    void calls_in_chain( ir::IRBuilder& b, const Args& a, const Funcs& f, const int n )
    {
        auto x = a[ 0 ];
        auto y = a[ 1 ];
        for ( int i { 0 }; i < n; ++i )
        {
            const ValueRef args[] = { x, y };
            const auto     r      = b.call( f.mix, args );
            y                     = x;
            x                     = b.bxor( r, k64( b, i ) );
        }
        b.ret( b.iadd( x, y ) );
    }

    void loop_rotate( ir::IRBuilder& b, const Args& a, const int k, const bool swap )
    {
        const std::vector< Type > head_tys( k + 1, Type::i64 );
        const std::vector< Type > exit_tys( k, Type::i64 );
        const auto                head = b.add_block( std::span< const Type > { head_tys } );
        const auto                exit = b.add_block( std::span< const Type > { exit_tys } );

        Args init { k64( b, 0 ) };
        for ( int i { 0 }; i < k; ++i ) init.push_back( b.iadd( b.imul( a[ 1 ], k64( b, i + 1 ) ), k64( b, i ) ) );
        b.br( head, init );

        b.switch_to_block( head );
        const auto i = b.block_param( head, 0 );
        Args       xs;
        for ( int j { 0 }; j < k; ++j ) xs.push_back( b.block_param( head, static_cast< uint16_t >( j + 1 ) ) );

        const auto t    = b.bxor( b.iadd( b.imul( xs[ k - 1 ], k64( b, 3 ) ), xs[ 0 ] ), i );
        const auto i1   = b.iadd( i, k64( b, 1 ) );
        const auto cond = b.icmp( ir::ICmpCC::slt, i1, a[ 0 ] );

        Args next { i1 };
        if ( swap )
        {
            for ( int j { 0 }; j + 1 < k; j += 2 )
            {
                next.push_back( xs[ j + 1 ] );
                next.push_back( xs[ j ] );
            }
            if ( k % 2 ) next.push_back( t );
            else
                next.back( ) = t;
        }
        else
        {
            for ( int j { 1 }; j < k; ++j ) next.push_back( xs[ j ] );
            next.push_back( t );
        }
        b.brif( cond, head, next, exit, xs );

        b.switch_to_block( exit );
        std::vector< ValueRef > ys;
        for ( int j { 0 }; j < k; ++j ) ys.push_back( b.block_param( exit, static_cast< uint16_t >( j ) ) );
        b.ret( fold_all( b, ys, k64( b, 17 ) ) );
    }

    void live_across_loop( ir::IRBuilder& b, const Args& a, const int m )
    {
        const auto head = b.add_block( { Type::i64, Type::i64 } );
        const auto exit = b.add_block( { Type::i64 } );

        std::vector< ValueRef > v;
        for ( int i { 0 }; i < m; ++i ) v.push_back( b.iadd( b.imul( a[ 1 ], k64( b, 3 * i + 1 ) ), k64( b, i ) ) );
        const auto limit = b.iadd( b.band( a[ 0 ], k64( b, 15 ) ), k64( b, 1 ) );
        const Args start { k64( b, 0 ), k64( b, 0 ) };
        b.br( head, start );

        b.switch_to_block( head );
        const auto i    = b.block_param( head, 0 );
        const auto s    = b.block_param( head, 1 );
        const auto s1   = b.iadd( s, b.imul( i, i ) );
        const auto i1   = b.iadd( i, k64( b, 1 ) );
        const auto cond = b.icmp( ir::ICmpCC::slt, i1, limit );
        const Args back { i1, s1 };
        const Args out { s1 };
        b.brif( cond, head, back, exit, out );

        b.switch_to_block( exit );
        b.ret( fold_all( b, v, b.block_param( exit, 0 ) ) );
    }

    void loop_with_calls( ir::IRBuilder& b, const Args& a, const Funcs& f, const int m )
    {
        const auto head = b.add_block( { Type::i64, Type::i64 } );
        const auto body = b.add_block( { } );
        const auto exit = b.add_block( { Type::i64 } );

        std::vector< ValueRef > v;
        for ( int i { 0 }; i < m; ++i ) v.push_back( b.bxor( b.imul( a[ 1 ], k64( b, i + 2 ) ), k64( b, i * 11 ) ) );
        const auto limit = b.band( a[ 0 ], k64( b, 7 ) );
        const Args start { k64( b, 0 ), a[ 1 ] };
        b.br( head, start );

        b.switch_to_block( head );
        const auto i    = b.block_param( head, 0 );
        const auto s    = b.block_param( head, 1 );
        const auto cond = b.icmp( ir::ICmpCC::slt, i, limit );
        const Args out { s };
        b.brif( cond, body, { }, exit, out );

        b.switch_to_block( body );
        const ValueRef args[] = { s, v[ 0 ] };
        auto           r      = b.call( f.mix, args );
        r                     = b.iadd( r, v[ m - 1 ] );
        const Args back { b.iadd( i, k64( b, 1 ) ), r };
        b.br( head, back );

        b.switch_to_block( exit );
        b.ret( fold_all( b, v, b.block_param( exit, 0 ) ) );
    }

    void divrem_shift( ir::IRBuilder& b, const Args& a, const int n )
    {
        std::vector< ValueRef > v;
        for ( int i { 0 }; i < n; ++i ) v.push_back( b.iadd( b.imul( a[ 0 ], k64( b, 2 * i + 1 ) ), b.iadd( a[ 1 ], k64( b, i ) ) ) );

        std::vector< ValueRef > w;
        for ( int i { 0 }; i < n; ++i )
        {
            const auto d = b.bor( b.ushr( v[ ( i + 1 ) % n ], k64( b, 1 ) ), k64( b, 1 ) );
            const auto q = i % 2 ? b.udiv( v[ i ], d ) : b.idiv( v[ i ], d );
            const auto r = i % 2 ? b.urem( v[ ( i + 2 ) % n ], d ) : b.srem( v[ ( i + 2 ) % n ], d );
            const auto s = b.ishl( q, v[ ( i + 3 ) % n ] );
            const auto u = i % 3 ? b.sshr( r, v[ i ] ) : b.ushr( r, v[ ( i + 4 ) % n ] );
            w.push_back( b.bxor( s, u ) );
        }
        b.ret( fold_all( b, v, fold_all( b, w, k64( b, 5 ) ) ) );
    }

    void dependency_chain( ir::IRBuilder& b, const Args& a, const int len )
    {
        auto x = a[ 0 ];
        for ( int i { 0 }; i < len; ++i )
        {
            switch ( i % 7 )
            {
                case 0: x = b.iadd( x, k64( b, i ) ); break;
                case 1: x = b.bxor( x, a[ 1 ] ); break;
                case 2: x = b.imul( x, k64( b, 3 ) ); break;
                case 3: x = b.isub( x, k64( b, i * 13 ) ); break;
                case 4: x = b.ishl( x, k64( b, i % 5 ) ); break;
                case 5: x = b.ushr( x, k64( b, 1 ) ); break;
                default: x = b.iadd( x, a[ 1 ] ); break;
            }
        }
        b.ret( x );
    }

    void fib_chain( ir::IRBuilder& b, const Args& a, const int len )
    {
        auto p = a[ 0 ];
        auto q = a[ 1 ];
        for ( int i { 0 }; i < len; ++i )
        {
            const auto r = b.bxor( b.iadd( p, q ), k64( b, i ) );
            p            = q;
            q            = r;
        }
        b.ret( b.isub( q, p ) );
    }

    void diamonds( ir::IRBuilder& b, const Args& a, const int n, const int live )
    {
        std::vector< ValueRef > v;
        for ( int i { 0 }; i < live; ++i ) v.push_back( b.iadd( b.imul( a[ 0 ], k64( b, i + 7 ) ), a[ 1 ] ) );

        auto x = a[ 0 ];
        for ( int d { 0 }; d < n; ++d )
        {
            const auto then_b = b.add_block( { } );
            const auto else_b = b.add_block( { } );
            const auto join   = b.add_block( { Type::i64 } );
            const auto c      = b.icmp( d % 2 ? ir::ICmpCC::ult : ir::ICmpCC::sgt, x, v[ d % live ] );
            b.brif( c, then_b, { }, else_b, { } );

            b.switch_to_block( then_b );
            const Args ta { b.iadd( x, v[ ( d + 1 ) % live ] ) };
            b.br( join, ta );

            b.switch_to_block( else_b );
            const Args ea { b.bxor( x, v[ ( d + 2 ) % live ] ) };
            b.br( join, ea );

            b.switch_to_block( join );
            x = b.block_param( join, 0 );
        }
        b.ret( fold_all( b, v, x ) );
    }

    struct Case
    {
        std::string name;
        Body        body;
        bool        loop { };
        bool        calls { };
    };

    const std::vector< Case >& cases( )
    {
        static const std::vector< Case > all = []
        {
            std::vector< Case > out;
            for ( const int n : { 1, 2, 4, 8, 12, 16, 20, 30, 40 } )
                out.push_back( { "wide " + std::to_string( n ), [ n ]( auto& b, auto& a, auto& )
                                 {
                                     wide( b, a, n );
                                 } } );
            for ( const int n : { 1, 3, 6, 8, 12, 24 } )
                out.push_back( { "across calls " + std::to_string( n ),
                                 [ n ]( auto& b, auto& a, auto& f )
                                 {
                                     across_calls( b, a, f, n );
                                 },
                                 false, true } );
            out.push_back( { "calls in chain",
                             []( auto& b, auto& a, auto& f )
                             {
                                 calls_in_chain( b, a, f, 12 );
                             },
                             false, true } );
            for ( const int k : { 1, 2, 3, 5, 6, 7, 8, 12 } )
                for ( const bool swap : { false, true } )
                    out.push_back( { "loop rotate " + std::to_string( k ) + ( swap ? " swap" : "" ),
                                     [ k, swap ]( auto& b, auto& a, auto& )
                                     {
                                         loop_rotate( b, a, k, swap );
                                     },
                                     true } );
            for ( const int m : { 1, 6, 14, 24 } )
                out.push_back( { "live across loop " + std::to_string( m ), [ m ]( auto& b, auto& a, auto& )
                                 {
                                     live_across_loop( b, a, m );
                                 } } );
            for ( const int m : { 1, 5, 12, 20 } )
                out.push_back( { "loop with calls " + std::to_string( m ),
                                 [ m ]( auto& b, auto& a, auto& f )
                                 {
                                     loop_with_calls( b, a, f, m );
                                 },
                                 false, true } );
            for ( const int n : { 1, 2, 5, 12, 20 } )
                out.push_back( { "divrem shift " + std::to_string( n ), [ n ]( auto& b, auto& a, auto& )
                                 {
                                     divrem_shift( b, a, n );
                                 } } );
            for ( const int n : { 1, 50, 500 } )
                out.push_back( { "dependency chain " + std::to_string( n ), [ n ]( auto& b, auto& a, auto& )
                                 {
                                     dependency_chain( b, a, n );
                                 } } );
            out.push_back( { "fib chain", []( auto& b, auto& a, auto& )
                             {
                                 fib_chain( b, a, 150 );
                             } } );
            for ( const int live : { 2, 10, 20 } )
                out.push_back( { "diamonds " + std::to_string( live ), [ live ]( auto& b, auto& a, auto& )
                                 {
                                     diamonds( b, a, 12, live );
                                 } } );
            return out;
        }( );
        return all;
    }

    const Case& find_case( const std::string_view name )
    {
        const auto it = std::ranges::find( cases( ), name, &Case::name );
        REQUIRE( it != cases( ).end( ) );
        return *it;
    }

    ir::IRFunction& make_fn( ir::IRModule& m, const codegen::CallConv cc, const bool colocated, const Body& body )
    {
        const Type ps[] = { Type::i64, Type::i64 };
        auto&      fn   = *m.get( m.declare( "f", Type::i64, std::span< const Type > { ps }, cc ) );
        {
            ir::IRBuilder b { fn };
            b.switch_to_block( b.add_block( std::span< const Type > { ps } ) );
            const Type  p4[] = { Type::i64, Type::i64, Type::i64, Type::i64 };
            const Funcs f {
                b.import_function( { ir::extns::function, 0 }, std::span< const Type > { ps }, Type::i64, cc, colocated ),
                b.import_function( { ir::extns::function, 1 }, std::span< const Type > { p4 }, Type::i64, cc, colocated ),
                b.import_function( { ir::extns::function, 2 }, std::span< const Type > { }, Type::i64, cc, colocated ),
            };
            const Args a { b.param( 0 ), b.param( 1 ) };
            body( b, a, f );
            b.finalise( );
        }
        const auto errs = verify( fn );
        for ( const auto& e : errs ) UNSCOPED_INFO( e );
        REQUIRE( errs.empty( ) );
        return fn;
    }

    struct Lowered
    {
        codegen::MachineEnv       env { };
        codegen::VCode< X86Inst > vc { };
        regalloc::Allocations     a { };
    };

    Lowered lower( ir::IRFunction& fn )
    {
        Lowered           l { .env = x86_64::env_for( fn.sig.cc ) };
        x86_64::IRLowerer lw { l.env, x86_64::host_abi( ) };
        l.vc = lw.lower( fn );
        l.a  = regalloc::allocate( l.vc, l.env );
        return l;
    }

    std::vector< regalloc::CheckerError > check( const Lowered& l, const std::span< const Allocation > ops,
                                                 const std::span< const regalloc::Edit > edits )
    { return regalloc::check_allocations( l.vc, regalloc::Cfg::build( l.vc ), ops, edits, l.a.spill_slots ); }

    void validate( const Lowered& l )
    {
        const auto& vc    = l.vc;
        const auto& a     = l.a;
        const auto  order = regalloc::allocation_order( l.env );

        REQUIRE( a.ops.size( ) == vc.operands.size( ) );
        for ( std::size_t o { 0 }; o < vc.operands.size( ); ++o )
        {
            const auto& op = vc.operands[ o ];
            CHECK( a.ops[ o ].is_assigned( ) );
            if ( op.is_fixed( ) ) CHECK( a.ops[ o ] == Allocation { .preg = *op.fixed_preg( ) } );
            else if ( a.ops[ o ].is_reg( ) )
            {
                const auto& cls = order[ static_cast< std::size_t >( a.ops[ o ].preg.rc( ) ) ];
                CHECK( std::ranges::find( cls, a.ops[ o ].preg ) != cls.end( ) );
                CHECK( a.ops[ o ].preg.rc( ) == op.vreg( ).rc( ) );
            }
            if ( a.ops[ o ].is_spill( ) ) CHECK( static_cast< uint32_t >( a.ops[ o ].slot ) < a.spill_slots );
        }

        CHECK( std::ranges::is_sorted( a.edits,
                                       []( const regalloc::Edit& x, const regalloc::Edit& y )
                                       {
                                           return x.point != y.point ? x.point < y.point : x.prio < y.prio;
                                       } ) );

        for ( const auto& e : a.edits )
        {
            CHECK( e.from.is_assigned( ) );
            CHECK( e.to.is_assigned( ) );
            CHECK_FALSE( e.from == e.to );
            CHECK( e.point % 2 == 0 );
            CHECK( e.point / 2 < vc.insts.size( ) );
            CHECK( e.vreg >= codegen::Reg::pinned_count );
            for ( const auto& loc : { e.from, e.to } )
            {
                if ( loc.is_spill( ) ) CHECK( static_cast< uint32_t >( loc.slot ) < a.spill_slots );
                if ( loc.is_reg( ) ) CHECK( std::ranges::find( order[ 0 ], loc.preg ) != order[ 0 ].end( ) );
            }
        }

        const auto errs = check( l, a.ops, a.edits );
        for ( const auto& e : errs ) UNSCOPED_INFO( "inst " << e.inst << " vreg " << e.vreg << " found " << e.found );
        CHECK( errs.empty( ) );
        CHECK( a.bundles > 0 );
    }

    struct Built
    {
        std::unique_ptr< Jit > jit;
        module::FuncId         id { };

        [[nodiscard]] ir::IRFunction& fn( ) const
        { return *jit->irm.get( ir::FunctionRef { 0 } ); }
    };

    Built build_host( const Body& body )
    {
        jit::JITBuilder jb { isa::lookup_host( ) };
        jb.symbol( "mix", reinterpret_cast< const void* >( &mix ) );
        jb.symbol( "mix4", reinterpret_cast< const void* >( &mix4 ) );
        jb.symbol( "seven", reinterpret_cast< const void* >( &seven ) );

        Built      out { std::make_unique< Jit >( std::move( jb ) ) };
        auto&      j      = *out.jit;
        const auto mix_id = j.import( "mix", Type::i64, { Type::i64, Type::i64 } );
        const auto m4_id  = j.import( "mix4", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 } );
        const auto s_id   = j.import( "seven", Type::i64, { } );

        out.id = j.define( "f", Type::i64, { Type::i64, Type::i64 },
                           [ & ]( ir::IRBuilder& b, const Args& a )
                           {
                               const Funcs f { j.mod.declare_func_in_func( mix_id, b ), j.mod.declare_func_in_func( m4_id, b ),
                                               j.mod.declare_func_in_func( s_id, b ) };
                               body( b, a, f );
                           } );
        j.finalise( );
        return out;
    }

    Interp host_interp( )
    {
        Interp in;
        in.call = []( const uint32_t f, const std::span< const uint64_t > a ) -> uint64_t
        {
            const auto s = [ & ]( const std::size_t k )
            {
                return static_cast< int64_t >( a[ k ] );
            };
            switch ( f )
            {
                case 0: return static_cast< uint64_t >( mix( s( 0 ), s( 1 ) ) );
                case 1: return static_cast< uint64_t >( mix4( s( 0 ), s( 1 ), s( 2 ), s( 3 ) ) );
                default: return static_cast< uint64_t >( seven( ) );
            }
        };
        return in;
    }

    std::vector< std::array< int64_t, 2 > > inputs( const bool loop )
    {
        std::vector< std::array< int64_t, 2 > > out;
        const auto&                             ev = edge_values( );
        if ( loop )
        {
            for ( int64_t n { -1 }; n <= 12; ++n )
                for ( std::size_t k { 0 }; k < ev.size( ); k += 3 ) out.push_back( { n, ev[ k ] } );
            return out;
        }
        for ( const auto x : ev )
            for ( const int64_t y :
                  { int64_t { 0 }, int64_t { 1 }, int64_t { -3 }, int64_t { 0x7fffffff }, std::numeric_limits< int64_t >::min( ) } )
                out.push_back( { x, y } );
        return out;
    }

    void run_compare( const Case& c )
    {
        INFO( c.name );
        const auto  bt = build_host( c.body );
        auto* const f  = bt.jit->get< int64_t( int64_t, int64_t ) >( bt.id );
        const auto  in = host_interp( );
        for ( const auto& [ x, y ] : inputs( c.loop ) )
        {
            const uint64_t args[] = { static_cast< uint64_t >( x ), static_cast< uint64_t >( y ) };
            INFO( "x=" << x << " y=" << y );
            CHECK( static_cast< uint64_t >( f( x, y ) ) == in.run( bt.fn( ), args ) );
        }
    }
} // namespace

TEST_CASE( "allocations of every pressure case are valid under win64", "[regalloc][alloc][win64]" )
{
    for ( const auto& c : cases( ) )
        for ( const bool colocated : { false, true } )
        {
            INFO( c.name << ( colocated ? " colocated" : " imported" ) );
            ir::IRModule m;
            validate( lower( make_fn( m, codegen::CallConv::WindowsFastcall, colocated, c.body ) ) );
        }
}

TEST_CASE( "allocations of every pressure case are valid under system v", "[regalloc][alloc][sysv]" )
{
    for ( const auto& c : cases( ) )
        for ( const bool colocated : { false, true } )
        {
            INFO( c.name << ( colocated ? " colocated" : " imported" ) );
            ir::IRModule m;
            validate( lower( make_fn( m, codegen::CallConv::SystemV, colocated, c.body ) ) );
        }
}

TEST_CASE( "jit results of every pressure case match the interpreter", "[regalloc][jit]" )
{
    for ( const auto& c : cases( ) ) run_compare( c );
}

TEST_CASE( "low pressure functions need no spill slots", "[regalloc][alloc][spill]" )
{
    for ( const auto* name : { "wide 1", "wide 2", "wide 4", "dependency chain 500", "fib chain", "loop rotate 3" } )
    {
        INFO( name );
        ir::IRModule m;
        const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false, find_case( name ).body ) );
        CHECK( l.a.spill_slots == 0 );
        CHECK( l.a.evictions == 0 );
    }
}

TEST_CASE( "more live values than registers forces spill slots", "[regalloc][alloc][spill]" )
{
    for ( const auto* name : { "wide 16", "wide 20", "wide 30", "wide 40", "across calls 12", "across calls 24", "live across loop 24",
                               "divrem shift 20", "diamonds 20", "loop rotate 12 swap" } )
    {
        INFO( name );
        ir::IRModule m;
        const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false, find_case( name ).body ) );
        CHECK( l.a.spill_slots > 0 );
        validate( l );
    }
}

TEST_CASE( "spill slot counts grow with the number of simultaneously live values", "[regalloc][alloc][spill]" )
{
    uint32_t prev { 0 };
    for ( const auto* name : { "wide 16", "wide 20", "wide 30", "wide 40" } )
    {
        ir::IRModule m;
        const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false, find_case( name ).body ) );
        INFO( name << " slots " << l.a.spill_slots );
        CHECK( l.a.spill_slots >= prev );
        prev = l.a.spill_slots;
    }
    CHECK( prev >= 40 - 12 );
}

TEST_CASE( "values live across calls avoid caller saved registers", "[regalloc][alloc][calls]" )
{
    ir::IRModule m;
    const auto   l = lower( make_fn( m, codegen::CallConv::WindowsFastcall, false, find_case( "across calls 6" ).body ) );
    validate( l );

    const auto& vc = l.vc;
    for ( uint32_t i { 0 }; i < vc.insts.size( ); ++i )
    {
        if ( !vc.insts[ i ].is_call( ) ) continue;
        const auto clobbers = vc.insts[ i ].clobbers( );
        CHECK_FALSE( clobbers.empty( ) );
        for ( const auto pr : { x86_64::RAX, x86_64::RCX, x86_64::RDX, x86_64::R8, x86_64::R9, x86_64::R10, x86_64::R11 } )
            CHECK( std::ranges::find( clobbers, pr ) != clobbers.end( ) );
        for ( const auto pr : { x86_64::RBX, x86_64::RSI, x86_64::RDI, x86_64::R12, x86_64::R13, x86_64::R14, x86_64::R15 } )
            CHECK( std::ranges::find( clobbers, pr ) == clobbers.end( ) );
    }

    bool callee_saved { false };
    for ( const auto& op : l.a.ops )
        if ( op.is_reg( ) )
            for ( const auto pr : { x86_64::RBX, x86_64::RSI, x86_64::RDI, x86_64::R12, x86_64::R13, x86_64::R14, x86_64::R15 } )
                callee_saved |= op.preg == pr;
    CHECK( callee_saved );
}

TEST_CASE( "pressure with calls evicts or splits", "[regalloc][alloc][calls]" )
{
    for ( const auto* name : { "across calls 12", "across calls 24", "loop with calls 20", "divrem shift 20" } )
    {
        ir::IRModule m;
        const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false, find_case( name ).body ) );
        INFO( name << " evictions " << l.a.evictions << " splits " << l.a.splits << " slots " << l.a.spill_slots );
        CHECK( l.a.evictions > 0 );
        CHECK( l.a.splits > 0 );
        CHECK( l.a.spill_slots > 0 );
        CHECK( l.a.bundles >= l.a.splits );
    }
}

TEST_CASE( "copies between block parameters are coalesced", "[regalloc][alloc][coalesce]" )
{
    for ( const auto* name : { "loop rotate 5", "loop rotate 8 swap", "diamonds 2", "fib chain", "dependency chain 50" } )
    {
        ir::IRModule m;
        const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false, find_case( name ).body ) );
        INFO( name << " merged " << l.a.merged_copies );
        CHECK( l.a.merged_copies > 0 );
    }
}

TEST_CASE( "fixed operands of division and shifts get rax, rdx and rcx", "[regalloc][alloc][fixed]" )
{
    ir::IRModule m;
    const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false, find_case( "divrem shift 5" ).body ) );
    validate( l );

    int divs { 0 }, shifts { 0 };
    for ( uint32_t i { 0 }; i < l.vc.insts.size( ); ++i )
    {
        const auto& in          = l.vc.insts[ i ].inner;
        const auto [ off, cnt ] = l.vc.operand_ranges[ i ];
        if ( in.is< x86_64::inst::idivq_m >( ) || in.is< x86_64::inst::divq_m >( ) )
        {
            ++divs;
            REQUIRE( cnt == 3 );
            CHECK( l.a.ops[ off + 1 ] == Allocation { .preg = x86_64::RAX } );
            CHECK( l.a.ops[ off + 2 ] == Allocation { .preg = x86_64::RDX } );
            CHECK_FALSE( l.a.ops[ off ] == Allocation { .preg = x86_64::RAX } );
            CHECK_FALSE( l.a.ops[ off ] == Allocation { .preg = x86_64::RDX } );
        }
        if ( in.is< x86_64::inst::shlq_mc >( ) || in.is< x86_64::inst::shrq_mc >( ) || in.is< x86_64::inst::sarq_mc >( ) )
        {
            ++shifts;
            REQUIRE( cnt == 2 );
            CHECK( l.a.ops[ off + 1 ] == Allocation { .preg = x86_64::RCX } );
            CHECK_FALSE( l.a.ops[ off ] == Allocation { .preg = x86_64::RCX } );
        }
    }
    CHECK( divs == 10 );
    CHECK( shifts >= 5 );
}

TEST_CASE( "checker reports a use read from a register that never held the value", "[regalloc][checker]" )
{
    ir::IRModule m;
    const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false,
                                     []( ir::IRBuilder& b, const Args& a, const Funcs& )
                                     {
                                       b.ret( b.isub( a[ 0 ], a[ 1 ] ) );
                                   } ) );
    validate( l );

    for ( const auto& op : l.a.ops ) REQUIRE_FALSE( op == Allocation { .preg = x86_64::R14 } );

    bool corrupted { false };
    for ( std::size_t o { 0 }; o < l.vc.operands.size( ) && !corrupted; ++o )
    {
        const auto& op = l.vc.operands[ o ];
        if ( op.is_fixed( ) || op.kind( ) != codegen::OperandKind::Use || !l.a.ops[ o ].is_reg( ) ) continue;
        auto ops        = l.a.ops;
        ops[ o ]        = Allocation { .preg = x86_64::R14 };
        corrupted       = true;
        const auto errs = check( l, ops, l.a.edits );
        REQUIRE_FALSE( errs.empty( ) );
        CHECK( errs[ 0 ].vreg == op.vreg( ).index( ) );
        CHECK( errs[ 0 ].found == ~0u );
        CHECK_FALSE( errs[ 0 ].in_edit );
    }
    CHECK( corrupted );
}

TEST_CASE( "checker reports swapped operand registers", "[regalloc][checker]" )
{
    ir::IRModule m;
    const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false,
                                     []( ir::IRBuilder& b, const Args& a, const Funcs& )
                                     {
                                       b.ret( b.isub( a[ 0 ], a[ 1 ] ) );
                                   } ) );
    validate( l );

    bool found { false };
    for ( uint32_t i { 0 }; i < l.vc.insts.size( ); ++i )
    {
        if ( !l.vc.insts[ i ].inner.is< x86_64::inst::subq_mr >( ) ) continue;
        const auto [ off, cnt ] = l.vc.operand_ranges[ i ];
        REQUIRE( cnt == 2 );
        REQUIRE_FALSE( l.a.ops[ off ] == l.a.ops[ off + 1 ] );
        auto ops = l.a.ops;
        std::swap( ops[ off ], ops[ off + 1 ] );
        CHECK_FALSE( check( l, ops, l.a.edits ).empty( ) );
        found = true;
    }
    CHECK( found );
}

TEST_CASE( "checker reports a dropped edit in spilling functions", "[regalloc][checker]" )
{
    for ( const auto* name : { "wide 30", "across calls 12", "live across loop 24", "loop rotate 12 swap", "divrem shift 20" } )
    {
        INFO( name );
        ir::IRModule m;
        const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false, find_case( name ).body ) );
        validate( l );
        REQUIRE_FALSE( l.a.edits.empty( ) );

        std::size_t breaking { 0 };
        for ( std::size_t k { 0 }; k < l.a.edits.size( ); ++k )
        {
            auto edits = l.a.edits;
            edits.erase( edits.begin( ) + static_cast< std::ptrdiff_t >( k ) );
            breaking += !check( l, l.a.ops, edits ).empty( );
        }
        CHECK( breaking > 0 );
    }
}

TEST_CASE( "checker reports a use read from the wrong spill slot", "[regalloc][checker]" )
{
    ir::IRModule m;
    const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false, find_case( "wide 40" ).body ) );
    validate( l );
    REQUIRE( l.a.spill_slots >= 2 );

    std::size_t tried { 0 };
    for ( std::size_t o { 0 }; o < l.vc.operands.size( ); ++o )
    {
        const auto& op = l.vc.operands[ o ];
        if ( op.kind( ) != codegen::OperandKind::Use || !l.a.ops[ o ].is_spill( ) ) continue;
        auto ops = l.a.ops;
        ops[ o ] = Allocation { .slot = static_cast< int32_t >( ( l.a.ops[ o ].slot + 1 ) % l.a.spill_slots ) };
        ++tried;
        CHECK_FALSE( check( l, ops, l.a.edits ).empty( ) );
    }
    CHECK( tried > 0 );
}

TEST_CASE( "checker reports an edit moving from the wrong location", "[regalloc][checker]" )
{
    ir::IRModule m;
    const auto   l = lower( make_fn( m, codegen::host_call_conv( ), false, find_case( "across calls 12" ).body ) );
    validate( l );
    REQUIRE_FALSE( l.a.edits.empty( ) );

    auto edits      = l.a.edits;
    edits[ 0 ].from = Allocation { .preg = x86_64::R10 };
    const auto errs = check( l, l.a.ops, edits );
    REQUIRE_FALSE( errs.empty( ) );
    CHECK( errs[ 0 ].in_edit );
}

TEST_CASE( "allocation is deterministic", "[regalloc][alloc]" )
{
    for ( const auto* name : { "wide 30", "across calls 24", "loop rotate 8 swap", "divrem shift 12" } )
    {
        INFO( name );
        ir::IRModule m1, m2;
        const auto   a = lower( make_fn( m1, codegen::host_call_conv( ), false, find_case( name ).body ) );
        const auto   b = lower( make_fn( m2, codegen::host_call_conv( ), false, find_case( name ).body ) );
        REQUIRE( a.a.ops.size( ) == b.a.ops.size( ) );
        for ( std::size_t o { 0 }; o < a.a.ops.size( ); ++o ) CHECK( a.a.ops[ o ] == b.a.ops[ o ] );
        REQUIRE( a.a.edits.size( ) == b.a.edits.size( ) );
        for ( std::size_t k { 0 }; k < a.a.edits.size( ); ++k )
        {
            CHECK( a.a.edits[ k ].point == b.a.edits[ k ].point );
            CHECK( a.a.edits[ k ].from == b.a.edits[ k ].from );
            CHECK( a.a.edits[ k ].to == b.a.edits[ k ].to );
        }
        CHECK( a.a.spill_slots == b.a.spill_slots );
    }
}

TEST_CASE( "jit sums forty simultaneously live parameters derived values", "[regalloc][jit][spill]" )
{
    auto f = compile< int64_t( int64_t, int64_t, int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 },
                                                                       []( ir::IRBuilder& b, const Args& a )
                                                                       {
                                                                           std::vector< ValueRef > v;
                                                                           for ( int i { 0 }; i < 40; ++i )
                                                                               v.push_back( b.iadd( a[ i % 4 ], k64( b, i ) ) );
                                                                           auto acc = k64( b, 0 );
                                                                           for ( int i { 39 }; i >= 0; --i )
                                                                               acc = b.iadd( acc, b.imul( v[ i ], k64( b, i + 1 ) ) );
                                                                           b.ret( acc );
                                                                       } );

    for ( const auto x : edge_values( ) )
    {
        const int64_t a[ 4 ] = { x, -x, x ^ 0x55, 12345 };
        uint64_t      want { 0 };
        for ( int i { 39 }; i >= 0; --i )
            want += ( static_cast< uint64_t >( a[ i % 4 ] ) + static_cast< uint64_t >( i ) ) * static_cast< uint64_t >( i + 1 );
        CHECK( static_cast< uint64_t >( f( a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ] ) ) == want );
    }
}

TEST_CASE( "jit keeps all four parameters live across a call", "[regalloc][jit][calls]" )
{
    jit::JITBuilder jb { isa::lookup_host( ) };
    jb.symbol( "mix4", reinterpret_cast< const void* >( &mix4 ) );
    Jit        j { std::move( jb ) };
    const auto m4 = j.import( "mix4", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 } );
    const auto id = j.define( "f", Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 },
                              [ & ]( ir::IRBuilder& b, const Args& a )
                              {
                                  const auto     fr    = j.mod.declare_func_in_func( m4, b );
                                  const ValueRef rev[] = { a[ 3 ], a[ 2 ], a[ 1 ], a[ 0 ] };
                                  const auto     r1    = b.call( fr, rev );
                                  const ValueRef rot[] = { a[ 1 ], a[ 2 ], a[ 3 ], r1 };
                                  const auto     r2    = b.call( fr, rot );
                                  const auto     sum   = b.iadd( b.iadd( a[ 0 ], a[ 1 ] ), b.iadd( a[ 2 ], a[ 3 ] ) );
                                  b.ret( b.bxor( b.iadd( r2, sum ), r1 ) );
                              } );
    j.finalise( );
    auto* const f = j.get< int64_t( int64_t, int64_t, int64_t, int64_t ) >( id );

    for ( const auto x : edge_values( ) )
    {
        const int64_t a = x, b = x ^ 0x1234, c = -7, d = static_cast< int64_t >( static_cast< uint64_t >( x ) * 3 );
        const auto    r1 = mix4( d, c, b, a );
        const auto    r2 = mix4( b, c, d, r1 );
        const auto    sum
            = static_cast< uint64_t >( a ) + static_cast< uint64_t >( b ) + static_cast< uint64_t >( c ) + static_cast< uint64_t >( d );
        CHECK( f( a, b, c, d ) == static_cast< int64_t >( ( static_cast< uint64_t >( r2 ) + sum ) ^ static_cast< uint64_t >( r1 ) ) );
    }
}

TEST_CASE( "jit rotates eight loop carried values for many iterations", "[regalloc][jit][loops]" )
{
    const auto  bt = build_host( find_case( "loop rotate 8" ).body );
    auto* const f  = bt.jit->get< int64_t( int64_t, int64_t ) >( bt.id );
    const auto  in = host_interp( );
    for ( const int64_t n : { 50, 100, 1000, 4097 } )
        for ( const int64_t seed : { 0, 1, -1, 0x1234567 } )
        {
            const uint64_t args[] = { static_cast< uint64_t >( n ), static_cast< uint64_t >( seed ) };
            CHECK( static_cast< uint64_t >( f( n, seed ) ) == in.run( bt.fn( ), args ) );
        }
}

TEST_CASE( "jit block argument permutations match a direct simulation", "[regalloc][jit][loops]" )
{
    for ( const int k : { 2, 3, 5, 6, 7, 8 } )
    {
        INFO( "k=" << k );
        auto f = compile< int64_t( int64_t, int64_t ) >(
            Type::i64, { Type::i64, Type::i64 },
            [ k ]( ir::IRBuilder& b, const Args& a )
            {
                const std::vector< Type > tys( k + 1, Type::i64 );
                const auto                head = b.add_block( std::span< const Type > { tys } );
                const auto                exit = b.add_block( { Type::i64 } );
                Args                      init { k64( b, 0 ) };
                for ( int j { 0 }; j < k; ++j ) init.push_back( b.iadd( a[ 1 ], k64( b, j * 100 ) ) );
                b.br( head, init );

                b.switch_to_block( head );
                const auto i  = b.block_param( head, 0 );
                const auto i1 = b.iadd( i, k64( b, 1 ) );
                Args       next { i1 };
                for ( int j { 0 }; j < k; ++j ) next.push_back( b.block_param( head, static_cast< uint16_t >( 1 + ( j * 3 + 1 ) % k ) ) );
                auto weighted = k64( b, 0 );
                for ( int j { 0 }; j < k; ++j )
                    weighted = b.iadd( b.imul( weighted, k64( b, 1000 ) ), b.block_param( head, static_cast< uint16_t >( j + 1 ) ) );
                const Args out { weighted };
                b.brif( b.icmp( ir::ICmpCC::slt, i1, a[ 0 ] ), head, next, exit, out );

                b.switch_to_block( exit );
                b.ret( b.block_param( exit, 0 ) );
            } );

        for ( const int64_t n : { 1, 2, 3, 7, 10, 31 } )
        {
            std::vector< uint64_t > xs( k );
            for ( int j { 0 }; j < k; ++j ) xs[ j ] = 5 + static_cast< uint64_t >( j * 100 );
            uint64_t weighted { 0 };
            for ( int64_t it { 0 };; )
            {
                weighted = 0;
                for ( int j { 0 }; j < k; ++j ) weighted = weighted * 1000 + xs[ j ];
                ++it;
                if ( !( it < n ) ) break;
                std::vector< uint64_t > nx( k );
                for ( int j { 0 }; j < k; ++j ) nx[ j ] = xs[ ( j * 3 + 1 ) % k ];
                xs = nx;
            }
            CHECK( static_cast< uint64_t >( f( n, 5 ) ) == weighted );
        }
    }
}

TEST_CASE( "jit computes deep recursive style chains of calls with live values", "[regalloc][jit][calls]" )
{
    run_compare( find_case( "calls in chain" ) );
    run_compare( find_case( "across calls 24" ) );
    run_compare( find_case( "loop with calls 20" ) );
}
