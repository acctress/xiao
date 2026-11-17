#include "support.hh"

#include <algorithm>
#include <map>
#include <random>
#include <set>

using namespace xt;
using codegen::BlockId;
using codegen::PReg;
using codegen::Reg;
using codegen::RegClass;
using codegen::VReg;
using regalloc::Allocation;
using regalloc::LiveRange;
using regalloc::Move;
using regalloc::Segment;

namespace
{
    namespace xi = x86_64::inst;
    using x86_64::X86Inst;

    Allocation R( const PReg p )
    { return Allocation { .preg = p }; }

    Allocation S( const int32_t s )
    { return Allocation { .slot = s }; }

    std::size_t key( const Allocation& a )
    { return a.is_spill( ) ? 1000 + static_cast< std::size_t >( a.slot ) : a.preg.index( ); }

    void check_moves( const std::vector< Move >& in, const Allocation temp )
    {
        const auto out = regalloc::sequence_parallel_moves( in, temp );

        std::map< std::size_t, std::size_t > st;
        const auto                           val = [ & ]( const Allocation& a )
        {
            const auto it = st.find( key( a ) );
            return it == st.end( ) ? key( a ) + 1 : it->second;
        };

        bool temp_written { false };
        for ( const auto& m : out )
        {
            CHECK( !( m.from == m.to ) );
            if ( m.from == temp ) CHECK( temp_written );
            if ( m.to == temp ) temp_written = true;
            st[ key( m.to ) ] = val( m.from );
        }

        std::set< std::size_t > dests;
        std::size_t             real { 0 };
        for ( const auto& m : in )
        {
            if ( m.from == m.to ) continue;
            ++real;
            dests.insert( key( m.to ) );
            CHECK( val( m.to ) == key( m.from ) + 1 );
        }

        for ( const auto& [ k, v ] : st )
            if ( !dests.contains( k ) && k != key( temp ) ) CHECK( v == k + 1 );

        CHECK( out.size( ) >= real );
        CHECK( out.size( ) <= real + real / 2 + 1 );
    }

    constexpr PReg pool[] = { x86_64::RAX, x86_64::RCX, x86_64::RDX, x86_64::RBX, x86_64::RSI, x86_64::RDI, x86_64::R8,
                              x86_64::R9,  x86_64::R11, x86_64::R12, x86_64::R13, x86_64::R14, x86_64::R15 };

    VReg vr( const uint32_t n, const RegClass rc = RegClass::Int )
    { return VReg::make( Reg::pinned_count + n, rc ); }

    Reg r( const uint32_t n )
    { return Reg::from_vreg( vr( n ) ); }

    uint32_t vi( const uint32_t n )
    { return Reg::pinned_count + n; }

    struct Hand
    {
        codegen::VCodeBuilder< X86Inst > b;
        std::vector< BlockId >           blocks;

        Hand( const uint32_t nblocks, const uint32_t nvregs, const std::set< uint32_t >& floats = { } )
        {
            for ( uint32_t v { 0 }; v < nvregs; ++v ) b.register_vreg( vr( v, floats.contains( v ) ? RegClass::Float : RegClass::Int ) );
            for ( uint32_t k { 0 }; k < nblocks; ++k ) blocks.push_back( b.start_block( ) );
        }

        void begin( const uint32_t k )
        { b.begin_block( blocks[ k ] ); }

        void end( const std::initializer_list< uint32_t > succs )
        {
            std::vector< BlockId > s;
            for ( const auto k : succs ) s.push_back( blocks[ k ] );
            b.end_block( s );
        }

        void cfg_only( const uint32_t k, const std::initializer_list< uint32_t > succs )
        {
            begin( k );
            end( succs );
        }

        codegen::VCode< X86Inst > done( )
        { return std::move( b ).finalise( ); }
    };

    codegen::VCode< X86Inst > straight_line( )
    {
        Hand h { 1, 2 };
        h.begin( 0 );
        h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 1 } );
        h.b.push( xi::MovImm { .dst = r( 1 ), .imm = 2 } );
        h.b.push( xi::addq_mr { x86_64::GprMem { r( 0 ) }, r( 1 ) } );
        h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 0 ) } );
        h.b.push( xi::ret { } );
        h.end( { } );
        return h.done( );
    }

    codegen::VCode< X86Inst > nested_loops( )
    {
        Hand h { 6, 0 };
        h.cfg_only( 0, { 1 } );
        h.cfg_only( 1, { 2 } );
        h.cfg_only( 2, { 3 } );
        h.cfg_only( 3, { 2, 4 } );
        h.cfg_only( 4, { 1, 5 } );
        h.cfg_only( 5, { } );
        return h.done( );
    }
}

TEST_CASE( "parallel moves: empty input yields nothing", "[regalloc][moves]" )
{ CHECK( regalloc::sequence_parallel_moves( { }, R( x86_64::R10 ) ).empty( ) ); }

TEST_CASE( "parallel moves: self moves are dropped", "[regalloc][moves]" )
{
    CHECK( regalloc::sequence_parallel_moves(
               {
                   { R( x86_64::RAX ), R( x86_64::RAX ) }
    },
               R( x86_64::R10 ) )
               .empty( ) );
    CHECK( regalloc::sequence_parallel_moves(
               {
                   {          S( 3 ),           S( 3 )},
                   {R( x86_64::RBX ), R( x86_64::RBX )}
    },
               R( x86_64::R10 ) )
               .empty( ) );
    const auto out = regalloc::sequence_parallel_moves(
        {
            {R( x86_64::RAX ), R( x86_64::RAX )},
            {R( x86_64::RCX ), R( x86_64::RDX )}
    },
        R( x86_64::R10 ) );
    REQUIRE( out.size( ) == 1 );
    CHECK( out[ 0 ].from == R( x86_64::RCX ) );
    CHECK( out[ 0 ].to == R( x86_64::RDX ) );
}

TEST_CASE( "parallel moves: independent moves are kept as is", "[regalloc][moves]" )
{
    const std::vector< Move > in {
        {R( x86_64::RAX ), R( x86_64::RBX )},
        {R( x86_64::RCX ), R( x86_64::RSI )},
        {          S( 0 ), R( x86_64::RDI )},
        {R( x86_64::RDX ),           S( 1 )}
    };
    const auto out = regalloc::sequence_parallel_moves( in, R( x86_64::R10 ) );
    CHECK( out.size( ) == in.size( ) );
    check_moves( in, R( x86_64::R10 ) );
}

TEST_CASE( "parallel moves: chains are ordered so no source is clobbered", "[regalloc][moves]" )
{
    std::vector< Move > chain {
        {R( x86_64::RAX ), R( x86_64::RCX )},
        {R( x86_64::RCX ), R( x86_64::RDX )},
        {R( x86_64::RDX ), R( x86_64::RBX )},
        {R( x86_64::RBX ),           S( 0 )},
        {          S( 0 ),           S( 1 )}
    };

    std::sort( chain.begin( ), chain.end( ),
               []( const Move& a, const Move& b )
               {
                   return key( a.to ) < key( b.to );
               } );
    do
    {
        const auto out = regalloc::sequence_parallel_moves( chain, R( x86_64::R10 ) );
        CHECK( out.size( ) == chain.size( ) );
        check_moves( chain, R( x86_64::R10 ) );
    } while ( std::next_permutation( chain.begin( ), chain.end( ),
                                     []( const Move& a, const Move& b )
                                     {
                                         return key( a.to ) < key( b.to );
                                     } ) );
}

TEST_CASE( "parallel moves: a swap uses the temp exactly once", "[regalloc][moves]" )
{
    const std::vector< Move > in {
        {R( x86_64::RAX ), R( x86_64::RCX )},
        {R( x86_64::RCX ), R( x86_64::RAX )}
    };
    const auto out = regalloc::sequence_parallel_moves( in, R( x86_64::R10 ) );
    REQUIRE( out.size( ) == 3 );
    CHECK( out[ 0 ].to == R( x86_64::R10 ) );
    CHECK( std::ranges::count_if( out,
                                  []( const Move& m )
                                  {
                                      return m.from == R( x86_64::R10 );
                                  } )
           == 1 );
    check_moves( in, R( x86_64::R10 ) );
}

TEST_CASE( "parallel moves: swaps between registers and spill slots", "[regalloc][moves]" )
{
    check_moves(
        {
            {R( x86_64::RAX ),           S( 0 )},
            {          S( 0 ), R( x86_64::RAX )}
    },
        R( x86_64::R10 ) );
    check_moves(
        {
            {S( 2 ), S( 5 )},
            {S( 5 ), S( 2 )}
    },
        R( x86_64::R10 ) );
    check_moves(
        {
            {S( 0 ), S( 1 )},
            {S( 1 ), S( 2 )},
            {S( 2 ), S( 0 )}
    },
        R( x86_64::R10 ) );
}

TEST_CASE( "parallel moves: n-cycles need exactly one extra move", "[regalloc][moves]" )
{
    for ( std::size_t n { 2 }; n <= std::size( pool ); ++n )
    {
        std::vector< Move > in;
        for ( std::size_t k { 0 }; k < n; ++k ) in.push_back( { R( pool[ k ] ), R( pool[ ( k + 1 ) % n ] ) } );
        const auto out = regalloc::sequence_parallel_moves( in, R( x86_64::R10 ) );
        CHECK( out.size( ) == n + 1 );
        check_moves( in, R( x86_64::R10 ) );

        std::reverse( in.begin( ), in.end( ) );
        check_moves( in, R( x86_64::R10 ) );
    }
}

TEST_CASE( "parallel moves: cycles with attached chains", "[regalloc][moves]" )
{
    const auto a = R( x86_64::RAX ), b = R( x86_64::RCX ), c = R( x86_64::RDX ), d = R( x86_64::RBX ), e = R( x86_64::RSI );
    check_moves(
        {
            {a, b},
            {b, a},
            {a, c}
    },
        R( x86_64::R10 ) );
    check_moves(
        {
            {a, c},
            {a, b},
            {b, a}
    },
        R( x86_64::R10 ) );
    check_moves(
        {
            {a, b},
            {b, c},
            {c, a},
            {c, d},
            {d, e}
    },
        R( x86_64::R10 ) );
    check_moves(
        {
            {d, e},
            {c, d},
            {c, a},
            {b, c},
            {a, b}
    },
        R( x86_64::R10 ) );
    check_moves(
        {
            {a, b},
            {b, a},
            {e, d},
            {d, c}
    },
        R( x86_64::R10 ) );
    check_moves(
        {
            {a, b},
            {b, c},
            {c, a},
            {a, d},
            {b, e}
    },
        R( x86_64::R10 ) );
}

TEST_CASE( "parallel moves: multiple disjoint cycles reuse the temp", "[regalloc][moves]" )
{
    const auto                a = R( x86_64::RAX ), b = R( x86_64::RCX ), c = R( x86_64::RDX ), d = R( x86_64::RBX ), e = R( x86_64::RSI ),
                              f = R( x86_64::RDI );
    const std::vector< Move > in {
        {     a,      b},
        {     b,      a},
        {     c,      d},
        {     d,      e},
        {     e,      c},
        {     f, S( 0 )},
        {S( 0 ),      f}
    };
    const auto out = regalloc::sequence_parallel_moves( in, R( x86_64::R10 ) );
    CHECK( out.size( ) == in.size( ) + 3 );
    check_moves( in, R( x86_64::R10 ) );
}

TEST_CASE( "parallel moves: one source fanning out to many destinations", "[regalloc][moves]" )
{
    const auto a = R( x86_64::RAX ), b = R( x86_64::RCX ), c = R( x86_64::RDX ), d = R( x86_64::RBX );
    check_moves(
        {
            {a,      b},
            {a,      c},
            {a,      d},
            {a, S( 0 )}
    },
        R( x86_64::R10 ) );
    check_moves(
        {
            {a, b},
            {a, c},
            {b, a}
    },
        R( x86_64::R10 ) );
    check_moves(
        {
            {a, b},
            {b, a},
            {b, c},
            {a, d}
    },
        R( x86_64::R10 ) );
    check_moves(
        {
            {a, a},
            {a, b},
            {a, c}
    },
        R( x86_64::R10 ) );
}

TEST_CASE( "parallel moves: the temp can be a spill slot", "[regalloc][moves]" )
{
    const auto a = R( x86_64::RAX ), b = R( x86_64::RCX ), c = R( x86_64::RDX );
    check_moves(
        {
            {a, b},
            {b, c},
            {c, a}
    },
        S( 7 ) );
    check_moves(
        {
            {S( 0 ), S( 1 )},
            {S( 1 ), S( 0 )}
    },
        S( 9 ) );
}

TEST_CASE( "parallel moves: randomised permutations and fan outs", "[regalloc][moves]" )
{
    std::vector< Allocation > locs;
    for ( const auto p : pool ) locs.push_back( R( p ) );
    for ( int32_t s { 0 }; s < 6; ++s ) locs.push_back( S( s ) );

    std::mt19937 rng { 1234 };
    for ( int iter { 0 }; iter < 2000; ++iter )
    {
        auto dsts = locs;
        std::shuffle( dsts.begin( ), dsts.end( ), rng );
        const auto n = 1 + rng( ) % dsts.size( );

        std::vector< Move > in;
        for ( std::size_t k { 0 }; k < n; ++k )
        {
            Allocation src;
            if ( iter % 2 == 0 ) src = dsts[ ( k + 1 + rng( ) % 2 ) % n ];
            else
                src = locs[ rng( ) % locs.size( ) ];
            in.push_back( { src, dsts[ k ] } );
        }
        std::shuffle( in.begin( ), in.end( ), rng );
        check_moves( in, R( x86_64::R10 ) );
    }
}

TEST_CASE( "parallel moves: random permutations are fully resolved", "[regalloc][moves]" )
{
    std::vector< Allocation > locs;
    for ( const auto p : pool ) locs.push_back( R( p ) );
    for ( int32_t s { 0 }; s < 4; ++s ) locs.push_back( S( s ) );

    std::mt19937 rng { 99 };
    for ( int iter { 0 }; iter < 500; ++iter )
    {
        auto perm = locs;
        std::shuffle( perm.begin( ), perm.end( ), rng );
        std::vector< Move > in;
        for ( std::size_t k { 0 }; k < locs.size( ); ++k ) in.push_back( { locs[ k ], perm[ k ] } );
        std::shuffle( in.begin( ), in.end( ), rng );
        check_moves( in, R( x86_64::R10 ) );
    }
}

TEST_CASE( "segments overlap only when half open ranges intersect", "[regalloc][ranges]" )
{
    CHECK( Segment { 0, 4 }.overlaps( { 3, 5 } ) );
    CHECK( Segment { 3, 5 }.overlaps( { 0, 4 } ) );
    CHECK_FALSE( Segment { 0, 4 }.overlaps( { 4, 8 } ) );
    CHECK_FALSE( Segment { 4, 8 }.overlaps( { 0, 4 } ) );
    CHECK( Segment { 0, 10 }.overlaps( { 4, 5 } ) );
    CHECK( Segment { 4, 5 }.overlaps( { 0, 10 } ) );
    CHECK( Segment { 2, 3 }.overlaps( { 2, 3 } ) );
    CHECK_FALSE( Segment { 2, 3 }.overlaps( { 5, 9 } ) );
}

TEST_CASE( "program points interleave before and after", "[regalloc][ranges]" )
{
    CHECK( regalloc::before( 0 ) == 0 );
    CHECK( regalloc::after( 0 ) == 1 );
    CHECK( regalloc::before( 1 ) == 2 );
    CHECK( regalloc::after( 1 ) == 3 );
    CHECK( regalloc::before( 100 ) == 200 );
    CHECK( regalloc::after( 100 ) == 201 );
    CHECK( regalloc::Mention { .point = regalloc::after( 7 ) }.inst( ) == 7 );
    CHECK( regalloc::Mention { .point = regalloc::before( 7 ) }.inst( ) == 7 );
}

TEST_CASE( "live range basics on an empty range", "[regalloc][ranges]" )
{
    LiveRange lr;
    CHECK( lr.empty( ) );
    CHECK( lr.length( ) == 0 );
    CHECK_FALSE( lr.covers( 0 ) );
    CHECK_FALSE( lr.overlaps( LiveRange { { { 0, 10 } } } ) );
    CHECK_FALSE( LiveRange { { { 0, 10 } } }.overlaps( lr ) );
}

TEST_CASE( "live range add keeps disjoint segments sorted", "[regalloc][ranges]" )
{
    LiveRange lr;
    lr.add( { 20, 24 } );
    lr.add( { 0, 4 } );
    lr.add( { 10, 12 } );
    REQUIRE( lr.segments.size( ) == 3 );
    CHECK( lr.segments[ 0 ].from == 0 );
    CHECK( lr.segments[ 1 ].from == 10 );
    CHECK( lr.segments[ 2 ].from == 20 );
    CHECK( lr.start( ) == 0 );
    CHECK( lr.end( ) == 24 );
    CHECK( lr.length( ) == 4 + 2 + 4 );
    CHECK_FALSE( lr.empty( ) );
}

TEST_CASE( "live range add merges adjacent segments on both sides", "[regalloc][ranges]" )
{
    LiveRange lr;
    lr.add( { 0, 4 } );
    lr.add( { 4, 8 } );
    REQUIRE( lr.segments.size( ) == 1 );
    CHECK( lr.segments[ 0 ].from == 0 );
    CHECK( lr.segments[ 0 ].to == 8 );

    lr.add( { 12, 16 } );
    lr.add( { 8, 12 } );
    REQUIRE( lr.segments.size( ) == 1 );
    CHECK( lr.segments[ 0 ].to == 16 );
}

TEST_CASE( "live range add absorbs contained and spanning segments", "[regalloc][ranges]" )
{
    LiveRange lr;
    lr.add( { 0, 10 } );
    lr.add( { 2, 3 } );
    REQUIRE( lr.segments.size( ) == 1 );
    CHECK( lr.segments[ 0 ].from == 0 );
    CHECK( lr.segments[ 0 ].to == 10 );

    LiveRange many;
    many.add( { 0, 2 } );
    many.add( { 4, 6 } );
    many.add( { 8, 10 } );
    many.add( { 12, 14 } );
    many.add( { 1, 9 } );
    REQUIRE( many.segments.size( ) == 2 );
    CHECK( many.segments[ 0 ].from == 0 );
    CHECK( many.segments[ 0 ].to == 10 );
    CHECK( many.segments[ 1 ].from == 12 );

    many.add( { 5, 20 } );
    REQUIRE( many.segments.size( ) == 1 );
    CHECK( many.segments[ 0 ].to == 20 );
    CHECK( many.length( ) == 20 );
}

TEST_CASE( "live range add between two segments without touching", "[regalloc][ranges]" )
{
    LiveRange lr;
    lr.add( { 0, 4 } );
    lr.add( { 8, 10 } );
    lr.add( { 5, 6 } );
    REQUIRE( lr.segments.size( ) == 3 );
    CHECK( lr.segments[ 1 ].from == 5 );
    CHECK( lr.segments[ 1 ].to == 6 );
    CHECK( std::ranges::is_sorted( lr.segments, { }, &Segment::from ) );
}

TEST_CASE( "live range add with randomised segments matches a bitmap", "[regalloc][ranges]" )
{
    std::mt19937 rng { 7 };
    for ( int iter { 0 }; iter < 300; ++iter )
    {
        LiveRange           lr;
        std::vector< bool > bits( 128, false );
        const auto          n = 1 + rng( ) % 12;
        for ( uint32_t k { 0 }; k < n; ++k )
        {
            const uint32_t from = rng( ) % 120;
            const uint32_t to   = from + 1 + rng( ) % 8;
            lr.add( { from, to } );
            for ( auto p { from }; p < to; ++p ) bits[ p ] = true;
        }

        for ( uint32_t p { 0 }; p < 128; ++p ) CHECK( lr.covers( p ) == bits[ p ] );
        CHECK( lr.length( ) == static_cast< uint32_t >( std::ranges::count( bits, true ) ) );
        for ( std::size_t k { 1 }; k < lr.segments.size( ); ++k ) CHECK( lr.segments[ k - 1 ].to < lr.segments[ k ].from );
    }
}

TEST_CASE( "live range covers is inclusive at from and exclusive at to", "[regalloc][ranges]" )
{
    LiveRange lr {
        { { 2, 5 }, { 9, 10 } }
    };
    CHECK_FALSE( lr.covers( 0 ) );
    CHECK_FALSE( lr.covers( 1 ) );
    CHECK( lr.covers( 2 ) );
    CHECK( lr.covers( 4 ) );
    CHECK_FALSE( lr.covers( 5 ) );
    CHECK_FALSE( lr.covers( 8 ) );
    CHECK( lr.covers( 9 ) );
    CHECK_FALSE( lr.covers( 10 ) );
    CHECK_FALSE( lr.covers( 1000 ) );
}

TEST_CASE( "live range start_at truncates a covering segment", "[regalloc][ranges]" )
{
    LiveRange lr {
        { { 0, 10 }, { 20, 30 } }
    };
    lr.start_at( 4 );
    REQUIRE( lr.segments.size( ) == 2 );
    CHECK( lr.segments[ 0 ].from == 4 );
    CHECK( lr.segments[ 0 ].to == 10 );

    lr.start_at( 25 );
    CHECK( lr.segments[ 1 ].from == 25 );
    CHECK( lr.segments[ 1 ].to == 30 );

    lr.start_at( 25 );
    CHECK( lr.segments[ 1 ].from == 25 );
}

TEST_CASE( "live range start_at adds a unit segment when nothing covers the point", "[regalloc][ranges]" )
{
    LiveRange lr;
    lr.start_at( 7 );
    REQUIRE( lr.segments.size( ) == 1 );
    CHECK( lr.segments[ 0 ].from == 7 );
    CHECK( lr.segments[ 0 ].to == 8 );

    LiveRange gap {
        { { 0, 4 }, { 10, 12 } }
    };
    gap.start_at( 4 );
    REQUIRE( gap.segments.size( ) == 2 );
    CHECK( gap.segments[ 0 ].from == 0 );
    CHECK( gap.segments[ 0 ].to == 5 );
    CHECK( gap.segments[ 1 ].from == 10 );

    LiveRange late { { { 0, 4 } } };
    late.start_at( 20 );
    REQUIRE( late.segments.size( ) == 2 );
    CHECK( late.segments[ 1 ].from == 20 );
    CHECK( late.segments[ 1 ].to == 21 );
}

TEST_CASE( "live range overlap scans interleaved segment lists", "[regalloc][ranges]" )
{
    const LiveRange a {
        { { 0, 2 }, { 6, 8 }, { 12, 14 } }
    };
    const LiveRange b {
        { { 2, 6 }, { 8, 12 }, { 14, 20 } }
    };
    const LiveRange c {
        { { 3, 4 }, { 13, 14 } }
    };
    const LiveRange d { { { 30, 40 } } };
    CHECK_FALSE( a.overlaps( b ) );
    CHECK_FALSE( b.overlaps( a ) );
    CHECK( a.overlaps( c ) );
    CHECK( c.overlaps( a ) );
    CHECK( b.overlaps( c ) );
    CHECK_FALSE( a.overlaps( d ) );
    CHECK_FALSE( d.overlaps( c ) );
    CHECK( a.overlaps( a ) );
}

TEST_CASE( "live range merge unions segment lists", "[regalloc][ranges]" )
{
    LiveRange a {
        { { 0, 2 }, { 6, 8 } }
    };
    const LiveRange b {
        { { 2, 6 }, { 10, 12 } }
    };
    a.merge( b );
    REQUIRE( a.segments.size( ) == 2 );
    CHECK( a.segments[ 0 ].from == 0 );
    CHECK( a.segments[ 0 ].to == 8 );
    CHECK( a.segments[ 1 ].from == 10 );
    CHECK( a.length( ) == 10 );

    LiveRange e;
    e.merge( b );
    CHECK( e.segments.size( ) == b.segments.size( ) );
    a.merge( LiveRange { } );
    CHECK( a.segments.size( ) == 2 );
}

TEST_CASE( "allocation equality distinguishes registers, slots and unassigned", "[regalloc][alloc-type]" )
{
    CHECK( R( x86_64::RAX ) == R( x86_64::RAX ) );
    CHECK_FALSE( R( x86_64::RAX ) == R( x86_64::RCX ) );
    CHECK( S( 3 ) == S( 3 ) );
    CHECK_FALSE( S( 3 ) == S( 4 ) );
    CHECK_FALSE( S( 0 ) == R( x86_64::RAX ) );
    CHECK_FALSE( R( x86_64::RAX ) == S( 0 ) );
    CHECK_FALSE( Allocation { }.is_assigned( ) );
    CHECK_FALSE( Allocation { }.is_reg( ) );
    CHECK_FALSE( Allocation { }.is_spill( ) );
    CHECK( S( 0 ).is_spill( ) );
    CHECK_FALSE( S( 0 ).is_reg( ) );
    CHECK( R( x86_64::R15 ).is_reg( ) );
    CHECK( Allocation { .preg = x86_64::RAX, .slot = 2 }.is_spill( ) );
    CHECK_FALSE( Allocation { .preg = x86_64::RAX, .slot = 2 }.is_reg( ) );
}

TEST_CASE( "allocation order lists preferred then non preferred and skips scratch and duplicates", "[regalloc][order]" )
{
    const auto                win = regalloc::allocation_order( x86_64::win64_env( ) );
    const std::vector< PReg > win_int { x86_64::RAX, x86_64::RCX, x86_64::RDX, x86_64::R8,  x86_64::R9,  x86_64::RBX,
                                        x86_64::RSI, x86_64::RDI, x86_64::R12, x86_64::R13, x86_64::R14, x86_64::R15 };
    CHECK( win[ 0 ] == win_int );
    CHECK( win[ 1 ].size( ) == 5 );
    CHECK( std::ranges::find( win[ 1 ], x86_64::XMM4 ) == win[ 1 ].end( ) );

    const auto                sysv = regalloc::allocation_order( x86_64::sysv_env( ) );
    const std::vector< PReg > sysv_int { x86_64::RAX, x86_64::RCX, x86_64::RDX, x86_64::RSI, x86_64::RDI, x86_64::R8,
                                         x86_64::R9,  x86_64::RBX, x86_64::R12, x86_64::R13, x86_64::R14, x86_64::R15 };
    CHECK( sysv[ 0 ] == sysv_int );
    CHECK( sysv[ 1 ].size( ) == 15 );
    CHECK( std::ranges::find( sysv[ 1 ], x86_64::XMM14 ) == sysv[ 1 ].end( ) );

    static constexpr PReg     pref[]    = { x86_64::RAX, x86_64::RCX, x86_64::RAX };
    static constexpr PReg     nonpref[] = { x86_64::RCX, x86_64::RBX, x86_64::R10 };
    static constexpr PReg     scratch[] = { x86_64::R10 };
    const codegen::MachineEnv env {
        .preferred     = {   pref, { }},
        .non_preferred = {nonpref, { }},
        .scratch       = {scratch, { }}
    };
    const auto order = regalloc::allocation_order( env );
    CHECK( order[ 0 ] == std::vector< PReg > { x86_64::RAX, x86_64::RCX, x86_64::RBX } );
    CHECK( order[ 1 ].empty( ) );
}

TEST_CASE( "liveness of a straight line block has no live in or out", "[regalloc][liveness]" )
{
    const auto vc = straight_line( );
    const auto lv = regalloc::Liveness::compute( vc );
    REQUIRE( lv.blocks.size( ) == 1 );
    CHECK( lv.blocks[ 0 ].live_in.empty( ) );
    CHECK( lv.blocks[ 0 ].live_out.empty( ) );
}

TEST_CASE( "liveness propagates across a jump and ignores pinned registers", "[regalloc][liveness]" )
{
    Hand h { 2, 3 };
    h.begin( 0 );
    h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 1 } );
    h.b.push( xi::MovImm { .dst = r( 1 ), .imm = 2 } );
    h.b.push( xi::MovImm { .dst = r( 2 ), .imm = 3 } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RCX ) }, r( 2 ) } );
    h.b.push( xi::Jmp { .target = h.blocks[ 1 ] } );
    h.end( { 1 } );
    h.begin( 1 );
    h.b.push( xi::addq_mr { x86_64::GprMem { r( 0 ) }, r( 1 ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 0 ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc = h.done( );

    const auto lv = regalloc::Liveness::compute( vc );
    CHECK( lv.blocks[ 0 ].live_in.empty( ) );
    CHECK( lv.blocks[ 0 ].live_out.to_vector( ) == std::vector< uint32_t > { vi( 0 ), vi( 1 ) } );
    CHECK( lv.blocks[ 1 ].live_in.to_vector( ) == std::vector< uint32_t > { vi( 0 ), vi( 1 ) } );
    CHECK( lv.blocks[ 1 ].live_out.empty( ) );
}

TEST_CASE( "liveness through a loop keeps loop carried values live around the back edge", "[regalloc][liveness]" )
{
    Hand h { 3, 3 };
    h.begin( 0 );
    h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 0 } );
    h.b.push( xi::MovImm { .dst = r( 1 ), .imm = 10 } );
    h.b.push( xi::MovImm { .dst = r( 2 ), .imm = 99 } );
    h.b.push( xi::Jmp { .target = h.blocks[ 1 ] } );
    h.end( { 1 } );
    h.begin( 1 );
    h.b.push( xi::addq_mi8 { x86_64::GprMem { r( 0 ) }, 1 } );
    h.b.push( xi::cmpq_mr { x86_64::GprMem { r( 0 ) }, r( 1 ) } );
    h.b.push( xi::Jcc { .cc = x86_64::CC::L, .taken = h.blocks[ 1 ], .not_taken = h.blocks[ 2 ] } );
    h.end( { 1, 2 } );
    h.begin( 2 );
    h.b.push( xi::addq_mr { x86_64::GprMem { r( 0 ) }, r( 2 ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 0 ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc = h.done( );

    const auto lv  = regalloc::Liveness::compute( vc );
    const auto all = std::vector< uint32_t > { vi( 0 ), vi( 1 ), vi( 2 ) };
    CHECK( lv.blocks[ 0 ].live_out.to_vector( ) == all );
    CHECK( lv.blocks[ 1 ].live_in.to_vector( ) == all );
    CHECK( lv.blocks[ 1 ].live_out.to_vector( ) == all );
    CHECK( lv.blocks[ 2 ].live_in.to_vector( ) == std::vector< uint32_t > { vi( 0 ), vi( 2 ) } );

    const auto  ranges = regalloc::LiveRanges::build( vc, lv );
    const auto& r2     = ranges.vregs[ vi( 2 ) ].range;
    CHECK( r2.covers( regalloc::before( 4 ) ) );
    CHECK( r2.covers( regalloc::before( 6 ) ) );
    CHECK( r2.covers( regalloc::before( 7 ) ) );
    CHECK_FALSE( r2.covers( regalloc::before( 8 ) ) );
    const auto& m2 = ranges.vregs[ vi( 2 ) ].mentions;
    REQUIRE( m2.size( ) == 2 );
    CHECK( m2[ 0 ].write );
    CHECK( m2[ 1 ].read );
    CHECK( m2[ 0 ].weight == 1.f );
    CHECK( ranges.vregs[ vi( 1 ) ].use_weight == 1.f + 10.f );
}

TEST_CASE( "liveness treats an unused definition as dead", "[regalloc][liveness]" )
{
    Hand h { 2, 2 };
    h.begin( 0 );
    h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 1 } );
    h.b.push( xi::MovImm { .dst = r( 1 ), .imm = 2 } );
    h.b.push( xi::Jmp { .target = h.blocks[ 1 ] } );
    h.end( { 1 } );
    h.begin( 1 );
    h.b.push( xi::MovImm { .dst = r( 1 ), .imm = 5 } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 1 ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc = h.done( );

    const auto lv = regalloc::Liveness::compute( vc );
    CHECK( lv.blocks[ 0 ].live_out.empty( ) );
    CHECK( lv.blocks[ 1 ].live_in.empty( ) );

    const auto  ranges = regalloc::LiveRanges::build( vc, lv );
    const auto& r0     = ranges.vregs[ vi( 0 ) ].range;
    REQUIRE( r0.segments.size( ) == 1 );
    CHECK( r0.segments[ 0 ].from == regalloc::after( 0 ) );
    CHECK( r0.segments[ 0 ].to == regalloc::after( 0 ) + 1 );
    CHECK( ranges.vregs[ vi( 1 ) ].range.segments.size( ) == 2 );
}

TEST_CASE( "live ranges of a straight line block", "[regalloc][ranges][build]" )
{
    const auto vc     = straight_line( );
    const auto ranges = regalloc::LiveRanges::build( vc, regalloc::Liveness::compute( vc ) );

    const auto& v0 = ranges.vregs[ vi( 0 ) ];
    REQUIRE( v0.range.segments.size( ) == 1 );
    CHECK( v0.range.segments[ 0 ].from == regalloc::after( 0 ) );
    CHECK( v0.range.segments[ 0 ].to == regalloc::before( 3 ) + 1 );
    REQUIRE( v0.mentions.size( ) == 3 );
    CHECK( v0.mentions[ 0 ].point == regalloc::after( 0 ) );
    CHECK( v0.mentions[ 0 ].write );
    CHECK_FALSE( v0.mentions[ 0 ].read );
    CHECK( v0.mentions[ 1 ].point == regalloc::before( 2 ) );
    CHECK( v0.mentions[ 1 ].read );
    CHECK( v0.mentions[ 1 ].write );
    CHECK( v0.mentions[ 2 ].point == regalloc::before( 3 ) );
    CHECK( v0.mentions[ 2 ].read );
    CHECK_FALSE( v0.mentions[ 2 ].write );
    CHECK( v0.use_weight == 3.f );
    CHECK( v0.cls == RegClass::Int );

    const auto& v1 = ranges.vregs[ vi( 1 ) ];
    REQUIRE( v1.range.segments.size( ) == 1 );
    CHECK( v1.range.segments[ 0 ].from == regalloc::after( 1 ) );
    CHECK( v1.range.segments[ 0 ].to == regalloc::before( 2 ) + 1 );

    const auto& rax = ranges.fixed[ x86_64::RAX.index( ) ];
    REQUIRE( rax.segments.size( ) == 1 );
    CHECK( rax.segments[ 0 ].from == regalloc::after( 3 ) );

    REQUIRE( ranges.hints.size( ) == 1 );
    CHECK( ranges.hints[ 0 ].vreg == vi( 0 ) );
    CHECK( ranges.hints[ 0 ].preg == x86_64::RAX );
    CHECK( ranges.hints[ 0 ].weight == 1.f );
    CHECK( ranges.copies.empty( ) );
}

TEST_CASE( "live ranges record copies and hints in both directions", "[regalloc][ranges][build]" )
{
    Hand h { 1, 3 };
    h.begin( 0 );
    h.b.push( xi::movq_mr { x86_64::GprMem { r( 0 ) }, Reg::from_preg( x86_64::RCX ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { r( 1 ) }, r( 0 ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { r( 2 ) }, r( 1 ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RDX ) }, Reg::from_preg( x86_64::RCX ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 2 ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc     = h.done( );
    const auto ranges = regalloc::LiveRanges::build( vc, regalloc::Liveness::compute( vc ) );

    REQUIRE( ranges.copies.size( ) == 2 );
    const auto has_copy = [ & ]( const uint32_t d, const uint32_t s )
    {
        return std::ranges::any_of( ranges.copies,
                                    [ & ]( const regalloc::CopyEdge& c )
                                    {
                                        return c.dst == d && c.src == s;
                                    } );
    };
    CHECK( has_copy( vi( 1 ), vi( 0 ) ) );
    CHECK( has_copy( vi( 2 ), vi( 1 ) ) );

    REQUIRE( ranges.hints.size( ) == 2 );
    const auto has_hint = [ & ]( const uint32_t v, const PReg p )
    {
        return std::ranges::any_of( ranges.hints,
                                    [ & ]( const regalloc::RegHint& x )
                                    {
                                        return x.vreg == v && x.preg == p;
                                    } );
    };
    CHECK( has_hint( vi( 0 ), x86_64::RCX ) );
    CHECK( has_hint( vi( 2 ), x86_64::RAX ) );
}

TEST_CASE( "live ranges add fixed segments for call clobbers", "[regalloc][ranges][build]" )
{
    Hand h { 1, 1 };
    h.begin( 0 );
    h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 4 } );
    h.b.push( xi::Call { .name     = { }, .info = xiao::support::Box { xi::CallInfo { .args     = { }, .ret      = PReg::invalid( ), .clobbers = { x86_64::R8, x86_64::R11 } } } } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 0 ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc     = h.done( );
    const auto ranges = regalloc::LiveRanges::build( vc, regalloc::Liveness::compute( vc ) );

    CHECK( ranges.fixed[ x86_64::R8.index( ) ].covers( regalloc::after( 1 ) ) );
    CHECK( ranges.fixed[ x86_64::R11.index( ) ].covers( regalloc::after( 1 ) ) );
    CHECK_FALSE( ranges.fixed[ x86_64::R9.index( ) ].covers( regalloc::after( 1 ) ) );
    CHECK( ranges.vregs[ vi( 0 ) ].range.covers( regalloc::after( 1 ) ) );
    CHECK( ranges.vregs[ vi( 0 ) ].range.overlaps( ranges.fixed[ x86_64::R8.index( ) ] ) );
}

TEST_CASE( "loop depths for nested loops", "[regalloc][ranges][loops]" )
{
    const auto vc     = nested_loops( );
    const auto depths = regalloc::LiveRanges::loop_depths( vc );
    CHECK( depths == std::vector< uint32_t > { 0, 1, 2, 2, 1, 0 } );
}

TEST_CASE( "loop depths for self loops, straight lines and unreachable blocks", "[regalloc][ranges][loops]" )
{
    {
        Hand h { 3, 0 };
        h.cfg_only( 0, { 1 } );
        h.cfg_only( 1, { 1, 2 } );
        h.cfg_only( 2, { } );
        CHECK( regalloc::LiveRanges::loop_depths( h.done( ) ) == std::vector< uint32_t > { 0, 1, 0 } );
    }
    {
        Hand h { 4, 0 };
        h.cfg_only( 0, { 1 } );
        h.cfg_only( 1, { 2 } );
        h.cfg_only( 2, { } );
        h.cfg_only( 3, { 3 } );
        CHECK( regalloc::LiveRanges::loop_depths( h.done( ) ) == std::vector< uint32_t > { 0, 0, 0, 0 } );
    }
    {
        Hand h { 1, 0 };
        h.cfg_only( 0, { } );
        CHECK( regalloc::LiveRanges::loop_depths( h.done( ) ) == std::vector< uint32_t > { 0 } );
    }
}

TEST_CASE( "loop depths for five deep nesting and sibling loops", "[regalloc][ranges][loops]" )
{
    {
        Hand h { 12, 0 };
        h.cfg_only( 0, { 1 } );
        h.cfg_only( 1, { 2 } );
        h.cfg_only( 2, { 3 } );
        h.cfg_only( 3, { 4 } );
        h.cfg_only( 4, { 5 } );
        h.cfg_only( 5, { 6 } );
        h.cfg_only( 6, { 5, 7 } );
        h.cfg_only( 7, { 4, 8 } );
        h.cfg_only( 8, { 3, 9 } );
        h.cfg_only( 9, { 2, 10 } );
        h.cfg_only( 10, { 1, 11 } );
        h.cfg_only( 11, { } );
        const auto depths = regalloc::LiveRanges::loop_depths( h.done( ) );
        CHECK( depths == std::vector< uint32_t > { 0, 1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 0 } );
        CHECK( regalloc::LiveRanges::depth_weight( depths[ 5 ] ) == 10000.f );
    }
    {
        Hand h { 5, 0 };
        h.cfg_only( 0, { 1 } );
        h.cfg_only( 1, { 1, 2 } );
        h.cfg_only( 2, { 3 } );
        h.cfg_only( 3, { 3, 4 } );
        h.cfg_only( 4, { } );
        CHECK( regalloc::LiveRanges::loop_depths( h.done( ) ) == std::vector< uint32_t > { 0, 1, 0, 1, 0 } );
    }
}

TEST_CASE( "depth weights grow tenfold and clamp at depth four", "[regalloc][ranges][loops]" )
{
    CHECK( regalloc::LiveRanges::depth_weight( 0 ) == 1.f );
    CHECK( regalloc::LiveRanges::depth_weight( 1 ) == 10.f );
    CHECK( regalloc::LiveRanges::depth_weight( 2 ) == 100.f );
    CHECK( regalloc::LiveRanges::depth_weight( 3 ) == 1000.f );
    CHECK( regalloc::LiveRanges::depth_weight( 4 ) == 10000.f );
    CHECK( regalloc::LiveRanges::depth_weight( 5 ) == 10000.f );
    CHECK( regalloc::LiveRanges::depth_weight( 1000 ) == 10000.f );
    CHECK( regalloc::LiveRanges::depth_weight( ~0u ) == 10000.f );
}

TEST_CASE( "cfg build records predecessors", "[regalloc][cfg]" )
{
    const auto vc  = nested_loops( );
    const auto cfg = regalloc::Cfg::build( vc );
    using V        = std::vector< uint32_t >;
    CHECK( cfg.preds[ 0 ].empty( ) );
    CHECK( cfg.preds[ 1 ] == V { 0, 4 } );
    CHECK( cfg.preds[ 2 ] == V { 1, 3 } );
    CHECK( cfg.preds[ 3 ] == V { 2 } );
    CHECK( cfg.preds[ 4 ] == V { 3 } );
    CHECK( cfg.preds[ 5 ] == V { 4 } );
}

TEST_CASE( "cfg build maps instructions to blocks", "[regalloc][cfg]" )
{
    Hand h { 3, 1 };
    h.begin( 0 );
    h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 1 } );
    h.b.push( xi::testq_mr { x86_64::GprMem { r( 0 ) }, r( 0 ) } );
    h.b.push( xi::Jcc { .cc = x86_64::CC::NE, .taken = h.blocks[ 1 ], .not_taken = h.blocks[ 2 ] } );
    h.end( { 1, 2 } );
    h.begin( 1 );
    h.b.push( xi::ret { } );
    h.end( { } );
    h.begin( 2 );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc  = h.done( );
    const auto cfg = regalloc::Cfg::build( vc );

    CHECK( cfg.block_of == std::vector< uint32_t > { 0, 0, 0, 1, 2 } );
}

TEST_CASE( "forward dataflow meets at joins", "[regalloc][cfg][dataflow]" )
{
    Hand h { 5, 0 };
    h.cfg_only( 0, { 1, 2 } );
    h.cfg_only( 1, { 3 } );
    h.cfg_only( 2, { 3 } );
    h.cfg_only( 3, { } );
    h.cfg_only( 4, { 3 } );
    const auto vc  = h.done( );
    const auto cfg = regalloc::Cfg::build( vc );

    const auto entry = regalloc::forward_dataflow( vc, cfg, 2,
                                                   []( const uint32_t b, auto& st )
                                                   {
                                                       if ( b == 0 )
                                                       {
                                                           st[ 0 ] = 6;
                                                           st[ 1 ] = 1;
                                                       }
                                                       if ( b == 1 ) st[ 0 ] = 7;
                                                   } );

    constexpr uint32_t none = ~0u;
    CHECK( entry[ 0 ] == std::vector< uint32_t > { none, none } );
    CHECK( entry[ 1 ] == std::vector< uint32_t > { 6, 1 } );
    CHECK( entry[ 2 ] == std::vector< uint32_t > { 6, 1 } );
    CHECK( entry[ 3 ] == std::vector< uint32_t > { none, 1 } );
    CHECK( entry[ 4 ].empty( ) );
}

TEST_CASE( "forward dataflow reaches a fixed point around loops", "[regalloc][cfg][dataflow]" )
{
    Hand h { 3, 0 };
    h.cfg_only( 0, { 1 } );
    h.cfg_only( 1, { 1, 2 } );
    h.cfg_only( 2, { } );
    const auto vc  = h.done( );
    const auto cfg = regalloc::Cfg::build( vc );

    int        calls { 0 };
    const auto entry = regalloc::forward_dataflow( vc, cfg, 1,
                                                   [ & ]( const uint32_t b, auto& st )
                                                   {
                                                       ++calls;
                                                       if ( b == 0 ) st[ 0 ] = 4;
                                                       if ( b == 1 ) st[ 0 ] = 9;
                                                   } );
    CHECK( entry[ 1 ] == std::vector< uint32_t > { ~0u } );
    CHECK( entry[ 2 ] == std::vector< uint32_t > { 9 } );
    CHECK( calls < 20 );
}

TEST_CASE( "bundles merge copies whose ranges do not overlap", "[regalloc][bundles]" )
{
    Hand h { 1, 2 };
    h.begin( 0 );
    h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 1 } );
    h.b.push( xi::movq_mr { x86_64::GprMem { r( 1 ) }, r( 0 ) } );
    h.b.push( xi::addq_mi8 { x86_64::GprMem { r( 1 ) }, 3 } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 1 ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc      = h.done( );
    const auto ranges  = regalloc::LiveRanges::build( vc, regalloc::Liveness::compute( vc ) );
    const auto bundles = regalloc::Bundles::build( ranges );

    CHECK( bundles.merged_copies == 1 );
    REQUIRE( bundles.bundles.size( ) == 1 );
    CHECK( bundles.bundle_of[ vi( 0 ) ] == bundles.bundle_of[ vi( 1 ) ] );
    CHECK( bundles.bundles[ 0 ].pieces.size( ) == 2 );
    CHECK( bundles.bundles[ 0 ].mentions.size( ) == 5 );
    CHECK( std::ranges::is_sorted( bundles.bundles[ 0 ].mentions, { }, &regalloc::Mention::point ) );
    REQUIRE( bundles.bundles[ 0 ].hints.size( ) == 1 );
    CHECK( bundles.bundles[ 0 ].hints[ 0 ].first == x86_64::RAX );
    CHECK( bundles.bundle_of[ 0 ] == regalloc::Bundles::none );
}

TEST_CASE( "bundles do not merge copies whose ranges overlap", "[regalloc][bundles]" )
{
    Hand h { 1, 2 };
    h.begin( 0 );
    h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 1 } );
    h.b.push( xi::movq_mr { x86_64::GprMem { r( 1 ) }, r( 0 ) } );
    h.b.push( xi::addq_mi8 { x86_64::GprMem { r( 1 ) }, 3 } );
    h.b.push( xi::addq_mr { x86_64::GprMem { r( 1 ) }, r( 0 ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 1 ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc      = h.done( );
    const auto bundles = regalloc::Bundles::build( regalloc::LiveRanges::build( vc, regalloc::Liveness::compute( vc ) ) );

    CHECK( bundles.merged_copies == 0 );
    CHECK( bundles.bundles.size( ) == 2 );
    CHECK( bundles.bundle_of[ vi( 0 ) ] != bundles.bundle_of[ vi( 1 ) ] );
}

TEST_CASE( "bundles do not merge copies across register classes", "[regalloc][bundles]" )
{
    Hand h { 1, 2, { 1 } };
    h.begin( 0 );
    h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 1 } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_vreg( vr( 1, RegClass::Float ) ) }, r( 0 ) } );
    h.b.push( xi::DummyUse { .reg = Reg::from_vreg( vr( 1, RegClass::Float ) ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc      = h.done( );
    const auto ranges  = regalloc::LiveRanges::build( vc, regalloc::Liveness::compute( vc ) );
    const auto bundles = regalloc::Bundles::build( ranges );

    CHECK( ranges.copies.size( ) == 1 );
    CHECK( bundles.merged_copies == 0 );
    REQUIRE( bundles.bundles.size( ) == 2 );
    CHECK( bundles.bundles[ bundles.bundle_of[ vi( 1 ) ] ].cls == RegClass::Float );
    CHECK( bundles.bundles[ bundles.bundle_of[ vi( 0 ) ] ].cls == RegClass::Int );
}

TEST_CASE( "bundles coalesce a chain of copies into one bundle", "[regalloc][bundles]" )
{
    Hand h { 1, 6 };
    h.begin( 0 );
    h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 1 } );
    for ( uint32_t k { 1 }; k < 6; ++k ) h.b.push( xi::movq_mr { x86_64::GprMem { r( k ) }, r( k - 1 ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 5 ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc      = h.done( );
    const auto bundles = regalloc::Bundles::build( regalloc::LiveRanges::build( vc, regalloc::Liveness::compute( vc ) ) );

    CHECK( bundles.merged_copies == 5 );
    REQUIRE( bundles.bundles.size( ) == 1 );
    CHECK( bundles.bundles[ 0 ].pieces.size( ) == 6 );
    for ( uint32_t k { 0 }; k < 6; ++k ) CHECK( bundles.bundle_of[ vi( k ) ] == 0 );
}

TEST_CASE( "bundles sum hint votes and sort them by weight", "[regalloc][bundles]" )
{
    Hand h { 1, 1 };
    h.begin( 0 );
    h.b.push( xi::movq_mr { x86_64::GprMem { r( 0 ) }, Reg::from_preg( x86_64::RCX ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RDX ) }, r( 0 ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 0 ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 0 ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc      = h.done( );
    const auto bundles = regalloc::Bundles::build( regalloc::LiveRanges::build( vc, regalloc::Liveness::compute( vc ) ) );

    REQUIRE( bundles.bundles.size( ) == 1 );
    const auto& hints = bundles.bundles[ 0 ].hints;
    REQUIRE( hints.size( ) == 3 );
    CHECK( hints[ 0 ].first == x86_64::RAX );
    CHECK( hints[ 0 ].second == 2.f );
    CHECK( hints[ 1 ].second == 1.f );
    CHECK( hints[ 2 ].second == 1.f );
}

TEST_CASE( "vreg locations look up bundle allocations by program point", "[regalloc][locations]" )
{
    std::vector< regalloc::LiveBundle > bundles( 2 );
    bundles[ 0 ].pieces.push_back( { vi( 0 ), LiveRange { { { 2, 6 } } } } );
    bundles[ 0 ].alloc = R( x86_64::RBX );
    bundles[ 1 ].pieces.push_back( { vi( 0 ), LiveRange { { { 6, 9 }, { 12, 14 } } } } );
    bundles[ 1 ].alloc = S( 0 );

    const regalloc::VRegLocations where { vi( 1 ), bundles };
    CHECK( where.size( ) == vi( 1 ) );
    CHECK( where.at( vi( 0 ), 1 ) == nullptr );
    REQUIRE( where.at( vi( 0 ), 2 ) != nullptr );
    CHECK( *where.at( vi( 0 ), 2 ) == R( x86_64::RBX ) );
    CHECK( *where.at( vi( 0 ), 5 ) == R( x86_64::RBX ) );
    CHECK( *where.at( vi( 0 ), 6 ) == S( 0 ) );
    CHECK( *where.at( vi( 0 ), 8 ) == S( 0 ) );
    CHECK( where.at( vi( 0 ), 9 ) == nullptr );
    CHECK( where.at( vi( 0 ), 11 ) == nullptr );
    CHECK( *where.at( vi( 0 ), 13 ) == S( 0 ) );
    CHECK( where.at( vi( 0 ), 14 ) == nullptr );
    CHECK( where.at( vi( 5 ), 3 ) == nullptr );
    CHECK( where.at( 3, 3 ) == nullptr );
    CHECK( where.pieces( vi( 0 ) ).size( ) == 3 );
}

TEST_CASE( "hand built straight line vcode allocates and checks clean", "[regalloc][hand]" )
{
    const auto vc  = straight_line( );
    const auto env = x86_64::win64_env( );
    const auto a   = regalloc::allocate( vc, env );
    REQUIRE( a.ops.size( ) == vc.operands.size( ) );
    for ( const auto& op : a.ops ) CHECK( op.is_assigned( ) );
    CHECK( a.spill_slots == 0 );
    CHECK( a.bundles == 2 );
    CHECK( regalloc::check_allocations( vc, regalloc::Cfg::build( vc ), a.ops, a.edits, a.spill_slots ).empty( ) );
    CHECK( a.ops[ 0 ] == R( x86_64::RAX ) );
}

TEST_CASE( "hand built vcode with a fixed register def honours the constraint", "[regalloc][hand]" )
{
    Hand h { 1, 3 };
    h.begin( 0 );
    h.b.push( xi::MovImm { .dst = r( 0 ), .imm = 100 } );
    h.b.push( xi::MovImm { .dst = r( 1 ), .imm = 7 } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 0 ) } );
    h.b.push( xi::cqo { Reg::from_preg( x86_64::RAX ), Reg::from_preg( x86_64::RDX ) } );
    h.b.push( xi::idivq_m { x86_64::GprMem { r( 1 ) }, Reg::from_preg( x86_64::RAX ), Reg::from_preg( x86_64::RDX ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { r( 2 ) }, Reg::from_preg( x86_64::RDX ) } );
    h.b.push( xi::movq_mr { x86_64::GprMem { Reg::from_preg( x86_64::RAX ) }, r( 2 ) } );
    h.b.push( xi::ret { } );
    h.end( { } );
    const auto vc = h.done( );
    const auto a  = regalloc::allocate( vc, x86_64::win64_env( ) );

    for ( uint32_t o { 0 }; o < vc.operands.size( ); ++o )
    {
        CHECK( a.ops[ o ].is_assigned( ) );
        if ( vc.operands[ o ].is_fixed( ) ) CHECK( a.ops[ o ] == R( *vc.operands[ o ].fixed_preg( ) ) );
    }

    const auto [ off, cnt ] = vc.operand_ranges[ 4 ];
    REQUIRE( cnt == 3 );
    CHECK( a.ops[ off ].is_reg( ) );
    CHECK( a.ops[ off ].preg != x86_64::RAX );
    CHECK( a.ops[ off ].preg != x86_64::RDX );
    CHECK( regalloc::check_allocations( vc, regalloc::Cfg::build( vc ), a.ops, a.edits, a.spill_slots ).empty( ) );
}
