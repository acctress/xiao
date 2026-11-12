#include <xiao-jit/module.hh>
#include <xiao/codegen/context.hh>
#include <xiao/isa/host.hh>
#include <xiao/ir/builder.hh>
#include <xiao/ir/module.hh>

#ifdef XIAO_HAS_ZYDIS
#include <xiao-disasm/disasm.hh>
#endif

#include <cstdint>
#include <iterator>
#include <print>
#include <string>

using namespace xiao;
using ir::Type;

int main( )
{
    jit::JITModule jit { jit::JITBuilder { isa::lookup_host( ) } };
    ir::IRModule   irm;

    constexpr Type params[] = { Type::i64 };
    const auto     fn_ref   = irm.declare( "f", Type::i64, params );
    const auto     fn_id    = *jit.declare_function( "f", module::Linkage::Export, module::signature_of( *irm.get( fn_ref ) ) );
    {
        ir::IRBuilder b { *irm.get( fn_ref ) };
        const auto    slot = b.create_stack_slot( 8, 8 );
        b.switch_to_block( b.add_block( params ) );

        const auto addr = b.stack_addr( Type::ptr, slot );
        b.store( b.param( 0 ), addr );
        const auto x = b.load( Type::i64, addr );
        b.ret( b.iadd( x, b.iconst( Type::i64, 1 ) ) );
        b.finalise( );
    }

    std::string text;
    irm.fmt( std::back_inserter( text ) );
    std::println( "{}", text );

    codegen::Context ctx { *irm.get( fn_ref ) };
    if ( !jit.define_function( fn_id, ctx ) || !jit.finalise_definitions( ) ) return 1;

#ifdef XIAO_HAS_ZYDIS
    std::println( "{}", disasm::dump( *ctx.compiled( ) ) );
#endif

    const auto f = jit.get_finalised_function< int64_t( int64_t ) >( fn_id );
    std::println( "f(41) = {}", f( 41 ) );
}
