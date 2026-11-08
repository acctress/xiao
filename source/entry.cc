#include <xiao-object/dump.hh>
#include <xiao-object/module.hh>
#include <xiao/isa/host.hh>
#include <xiao/ir/builder.hh>
#include <xiao/ir/module.hh>
#include <xiao/ir/verifier.hh>

#ifdef XIAO_HAS_ZYDIS
#include <xiao-disasm/disasm.hh>
#endif

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <print>
#include <string_view>

using namespace xiao;
using ir::Type;

int main( )
{
    object::ObjectModule obj {
        object::ObjectBuilder { isa::lookup_host( ), "hello" }
    };

    constexpr auto cc = codegen::host_call_conv( );

    const auto puts_id = *obj.declare_function( "puts", module::Linkage::Import, { { Type::ptr }, Type::i32, cc } );
    const auto text_id = *obj.declare_data( "hello", module::Linkage::Local, false );

    static constexpr char   message[] = "hello, world";
    module::DataDescription text;
    text.define( std::string_view { message, sizeof message } );
    if ( !obj.define_data( text_id, text ) ) return 1;

    ir::IRModule irm;
    const auto   main_ref = irm.declare( "main", Type::i32, { } );
    const auto   main_id  = *obj.declare_function( "main", module::Linkage::Export, module::signature_of( *irm.get( main_ref ) ) );
    {
        ir::IRBuilder b { *irm.get( main_ref ) };
        const auto    puts_fn = obj.declare_func_in_func( puts_id, b );
        const auto    text_gv = obj.declare_data_in_func( text_id, b );
        b.switch_to_block( b.add_block( { } ) );

        const ir::ValueRef args[] = { b.symbol_value( Type::ptr, text_gv ) };
        ( void ) b.call( puts_fn, args );
        b.ret( b.iconst( Type::i32, 0 ) );
        b.finalise( );
    }

    ir::IRVerifier verifier;
    verifier.visit_module( irm );
    if ( !verifier.ok( ) )
    {
        for ( const auto& [ msg ] : verifier.errs ) std::println( "[error] {}", msg );
        return 1;
    }

    codegen::Context ctx { *irm.get( main_ref ) };
    if ( !obj.define_function( main_id, ctx ) ) return 1;

    auto product = std::move( obj ).finish( );
    if ( !product ) return 1;

    const auto dir      = std::filesystem::current_path( ) / "xiao_hello";
    const auto obj_path = dir / "hello.obj";
    const auto exe_path = dir / "hello.exe";
    std::filesystem::create_directories( dir );
    if ( !product->write( obj_path ) ) return 1;

    std::println( "wrote {} ({} bytes)\n", obj_path.string( ), product->emit( ).size( ) );
    std::println( "{}", object::dump( product->object( ) ) );
#ifdef XIAO_HAS_ZYDIS
    std::println( "  disassembly of main\n{}", disasm::dump( *ctx.compiled( ) ) );
#endif

    const auto link = std::format( "clang -fuse-ld=lld \"{}\" -o \"{}\"", obj_path.string( ), exe_path.string( ) );
    std::println( "> {}", link );
    if ( std::system( link.c_str( ) ) != 0 ) return 1;

    std::println( "> {}", exe_path.string( ) );
    std::fflush( stdout );
    return std::system( std::format( "\"\"{}\"\"", exe_path.string( ) ).c_str( ) );
}
