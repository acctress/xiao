#include "support.hh"

#include <xiao/ir/dominators.hh>

using namespace xt;

TEST_CASE( "dominator tree of a diamond", "[ir][dom]" )
{
    ir::IRModule irm;
    auto&        fn = *irm.get( irm.declare( "f", Type::i64, { Type::i64 } ) );
    {
        ir::IRBuilder b { fn };
        const auto    entry = b.add_block( { Type::i64 } );
        const auto    left  = b.add_block( { } );
        const auto    right = b.add_block( { } );
        const auto    merge = b.add_block( { } );
        const auto    dead  = b.add_block( { } );
        b.switch_to_block( entry );
        b.brif( b.param( 0 ), left, { }, right, { } );
        b.switch_to_block( left );
        b.br( merge );
        b.switch_to_block( right );
        b.br( merge );
        b.switch_to_block( merge );
        b.ret( b.param( 0 ) );
        b.switch_to_block( dead );
        b.br( merge );
        b.finalise( );
    }

    const ir::DomTree dom { fn };
    CHECK( dom.idom( 1 ) == 0 );
    CHECK( dom.idom( 2 ) == 0 );
    CHECK( dom.idom( 3 ) == 0 );
    CHECK( dom.block_dominates( 0, 3 ) );
    CHECK_FALSE( dom.block_dominates( 1, 3 ) );
    CHECK_FALSE( dom.block_dominates( 2, 3 ) );
    CHECK( dom.block_dominates( 3, 3 ) );
    CHECK_FALSE( dom.is_reachable( 4 ) );
}

TEST_CASE( "dominator tree of a loop", "[ir][dom]" )
{
    ir::IRModule irm;
    auto&        fn = *irm.get( irm.declare( "f", Type::i64, { Type::i64 } ) );
    {
        ir::IRBuilder b { fn };
        const auto    entry  = b.add_block( { Type::i64 } );
        const auto    header = b.add_block( { } );
        const auto    body   = b.add_block( { } );
        const auto    exit   = b.add_block( { } );
        b.switch_to_block( entry );
        b.br( header );
        b.switch_to_block( header );
        b.brif( b.param( 0 ), body, { }, exit, { } );
        b.switch_to_block( body );
        b.br( header );
        b.switch_to_block( exit );
        b.ret( b.param( 0 ) );
        b.finalise( );
    }

    const ir::DomTree dom { fn };
    CHECK( dom.idom( 1 ) == 0 );
    CHECK( dom.idom( 2 ) == 1 );
    CHECK( dom.idom( 3 ) == 1 );
    CHECK( dom.block_dominates( 1, 2 ) );
    CHECK_FALSE( dom.block_dominates( 2, 1 ) );
}