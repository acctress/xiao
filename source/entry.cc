#include "xiao/isa/x86/machenv.hh"
#include "xiao/isa/x86/isa.hh"
#include "xiao/regalloc/regalloc.hh"

#include <xiao/ir/builder.hh>
#include <xiao/ir/verifier.hh>
#include <xiao/ir/module.hh>
#include <xiao/isa/x86/emit.hh>
#include <print>
#include <iostream>

#include <xiao/isa/x86/abi.hh>
#include <xiao/isa/x86/lower.hh>

using namespace xiao::ir;

int main( )
{
    IRModule module;

    const auto ref = module.declare( "max", Type::i64, { Type::i64, Type::i64 } );
    IRBuilder  b { *module.get( ref ) };

    const auto entry   = b.add_block( { Type::i64, Type::i64 } );
    const auto true_b  = b.add_block( { } );
    const auto false_b = b.add_block( { } );

    b.switch_to_block( entry );

    auto x   = b.param( 0 );
    auto y   = b.param( 1 );
    auto cmp = b.icmp( ICmpCC::sgt, x, y );

    b.brif( cmp, true_b, { }, false_b, { } );

    b.switch_to_block( true_b );
    b.ret( x );

    b.switch_to_block( false_b );
    b.ret( y );

    b.finalise( );

    IRVerifier verifier;
    verifier.visit_module( module );
    if ( !verifier.ok( ) )
        for ( const auto& [ msg ] : verifier.errs ) std::println( "\x1b[38;5;198m[error]\x1b[0m {}", msg );

    std::println( "ok" );



    auto target = xiao::x86_64::make_x86_target();
    target->compile(*module.get(ref), xiao::codegen::CallConv::WindowsFastcall);

    // dump bytes
    for (const auto b : target->bytes())
        std::print("{:02x} ", b);
    std::println("");
}