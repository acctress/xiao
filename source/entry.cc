#include <xiao-jit/module.hh>
#include <xiao/isa/host.hh>
#include <xiao/ir/builder.hh>
#include <xiao/ir/module.hh>
#include <xiao/ir/verifier.hh>

#include <cstdint>
#include <iterator>
#include <print>
#include <string>

using namespace xiao;
using ir::Type;

extern "C" int64_t sqr_func( const int64_t v )
{
    std::println( "  sqr_func({}) called from jit code", v );
    return v * v;
}

int main( )
{
    jit::JITBuilder builder { isa::lookup_host( ) };
    builder.symbol( "sqr_func", reinterpret_cast< const void* >( &sqr_func ) );
    jit::JITModule jit { std::move( builder ) };

    const auto square_id = *jit.declare_function( "sqr_func", module::Linkage::Import, { { Type::i64 }, Type::i64, codegen::host_call_conv( ) } );

    ir::IRModule irm;
    const auto   process_ref = irm.declare( "process", Type::i64, { Type::ptr, Type::i64 } );
    const auto   process_id  = *jit.declare_function( "process", module::Linkage::Export, module::signature_of( *irm.get( process_ref ) ) );

    {
        ir::IRBuilder b { *irm.get( process_ref ) };
        const auto    square = jit.declare_func_in_func( square_id, b );

        const auto entry  = b.add_block( { Type::ptr, Type::i64 } );
        const auto header = b.add_block( { Type::i64, Type::i64 } );
        const auto body   = b.add_block( { } );
        const auto join   = b.add_block( { Type::i64 } );
        const auto exit   = b.add_block( { Type::i64 } );

        b.switch_to_block( entry );
        const auto         p       = b.param( 0 );
        const auto         n       = b.param( 1 );
        const auto         zero    = b.iconst( Type::i64, 0 );
        const ir::ValueRef start[] = { zero, zero };
        b.br( header, start );

        b.switch_to_block( header );
        const auto         i      = b.block_param( header, 0 );
        const auto         acc    = b.block_param( header, 1 );
        const ir::ValueRef done[] = { acc };
        b.brif( b.icmp( ir::ICmpCC::slt, i, n ), body, { }, exit, done );

        b.switch_to_block( body );
        const auto         addr   = b.iadd( p, b.imul( i, b.iconst( Type::i64, 8 ) ) );
        const ir::ValueRef args[] = { b.load( Type::i64, addr ) };
        const auto         sq     = b.call( square, args );
        const ir::ValueRef half[] = { b.idiv( sq, b.iconst( Type::i64, 2 ) ) };
        const ir::ValueRef less[] = { b.isub( sq, b.iconst( Type::i64, 1 ) ) };
        b.brif( b.icmp( ir::ICmpCC::sgt, sq, b.iconst( Type::i64, 10 ) ), join, half, join, less );

        b.switch_to_block( join );
        const auto v = b.block_param( join, 0 );
        b.store( v, addr );
        const ir::ValueRef next[] = { b.iadd( i, b.iconst( Type::i64, 1 ) ), b.iadd( acc, v ) };
        b.br( header, next );

        b.switch_to_block( exit );
        b.ret( b.block_param( exit, 0 ) );
        b.finalise( );
    }

    ir::IRVerifier verifier;
    verifier.visit_module( irm );
    if ( !verifier.ok( ) )
    {
        for ( const auto& [ msg ] : verifier.errs ) std::println( "[error] {}", msg );
        return 1;
    }

    std::string text;
    irm.fmt( std::back_inserter( text ) );
    std::println( "~~ ir ~~\n{}", text );

    codegen::Context ctx { *irm.get( process_ref ) };
    if ( !jit.define_function( process_id, ctx ) || !jit.finalise_definitions( ) )
    {
        std::println( "[error] jit failed" );
        return 1;
    }

    auto* const process = jit.get_finalised_function< int64_t( int64_t*, int64_t ) >( process_id );

    int64_t data[] = { 1, 2, 3, 4, 5 };

    std::println( "~~ run ~~" );
    const auto total = process( data, 5 );

    std::println( "~~ result ~~" );
    std::println( "  process(data, 5) = {}  {}", total, total == 31 ? "ok" : "nuh uh, want 31" );
    std::println( "  data = {{ {}, {}, {}, {}, {} }}  {}", data[ 0 ], data[ 1 ], data[ 2 ], data[ 3 ], data[ 4 ],
                  data[ 0 ] == 0 && data[ 1 ] == 3 && data[ 2 ] == 8 && data[ 3 ] == 8 && data[ 4 ] == 12 ? "ok" : "nuh uh, want { 0, 3, 8, 8, 12 }" );

    return total == 31 ? 0 : 1;
}
