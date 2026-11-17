#include "support.hh"

#include <xiao/isa/host.hh>
#include <xiao/isa/x86/abi.hh>
#include <xiao/isa/x86/isa.hh>
#include <xiao/isa/x86/machenv.hh>
#include <xiao/regalloc/regalloc.hh>

#include <algorithm>
#include <vector>

using namespace xt;
using namespace xiao::x86_64;
using codegen::ABISig;
using codegen::CallConv;
using codegen::PReg;

namespace
{
    [[nodiscard]] ABISig sig( const CallConv cc, const std::vector< Type >& params, const std::vector< Type >& rets = { } )
    { return host_abi( ).compute( cc, params, rets ); }

    [[nodiscard]] std::vector< PReg > regs_of( const std::vector< codegen::ArgLoc >& locs )
    {
        std::vector< PReg > out;
        for ( const auto& l : locs )
            if ( l.is_reg( ) ) out.push_back( l.reg( ) );
        return out;
    }

    [[nodiscard]] std::vector< PReg > span_vec( const std::span< const PReg > s )
    { return { s.begin( ), s.end( ) }; }
} // namespace

TEST_CASE( "abi argloc variant accessors", "[abi]" )
{
    const codegen::ArgLoc reg { RCX, Type::i64 };
    CHECK( reg.is_reg( ) );
    CHECK_FALSE( reg.is_stack( ) );
    CHECK( reg.reg( ) == RCX );
    CHECK( reg.ty == Type::i64 );

    const codegen::ArgLoc stack { int32_t { 40 }, Type::i32 };
    CHECK( stack.is_stack( ) );
    CHECK_FALSE( stack.is_reg( ) );
    CHECK( stack.stack_offset( ) == 40 );
    CHECK( stack.ty == Type::i32 );
}

TEST_CASE( "abi sysv with no params and no return", "[abi][sysv]" )
{
    const auto s = sig( CallConv::SystemV, { } );
    CHECK( s.params.empty( ) );
    CHECK( s.rets.empty( ) );
    CHECK( s.stack_arg_bytes == 0 );
    CHECK( s.stack_align == 16 );
}

TEST_CASE( "abi sysv integer arguments fill six registers", "[abi][sysv]" )
{
    const std::vector order = { RDI, RSI, RDX, RCX, R8, R9 };
    for ( std::size_t n { 1 }; n <= 6; ++n )
    {
        const auto s = sig( CallConv::SystemV, std::vector( n, Type::i64 ) );
        REQUIRE( s.params.size( ) == n );
        CHECK( regs_of( s.params ) == std::vector< PReg >( order.begin( ), order.begin( ) + static_cast< std::ptrdiff_t >( n ) ) );
        CHECK( s.stack_arg_bytes == 0 );
    }
}

TEST_CASE( "abi sysv extra integer arguments go to the stack", "[abi][sysv]" )
{
    const auto s = sig( CallConv::SystemV, std::vector< Type >( 9, Type::i32 ) );
    REQUIRE( s.params.size( ) == 9 );
    for ( std::size_t i { 0 }; i < 6; ++i ) CHECK( s.params[ i ].is_reg( ) );
    CHECK( s.params[ 6 ].stack_offset( ) == 0 );
    CHECK( s.params[ 7 ].stack_offset( ) == 8 );
    CHECK( s.params[ 8 ].stack_offset( ) == 16 );
    CHECK( s.stack_arg_bytes == 24 );
    for ( const auto& p : s.params ) CHECK( p.ty == Type::i32 );
}

TEST_CASE( "abi sysv float arguments use their own counter", "[abi][sysv]" )
{
    const auto s = sig( CallConv::SystemV, { Type::f64, Type::i64, Type::f64, Type::ptr, Type::f64 } );
    REQUIRE( s.params.size( ) == 5 );
    CHECK( s.params[ 0 ].reg( ) == XMM0 );
    CHECK( s.params[ 1 ].reg( ) == RDI );
    CHECK( s.params[ 2 ].reg( ) == XMM1 );
    CHECK( s.params[ 3 ].reg( ) == RSI );
    CHECK( s.params[ 4 ].reg( ) == XMM2 );
    CHECK( s.stack_arg_bytes == 0 );
}

TEST_CASE( "abi sysv nine floats spill the ninth", "[abi][sysv]" )
{
    const auto s = sig( CallConv::SystemV, std::vector< Type >( 9, Type::f64 ) );
    for ( uint8_t i { 0 }; i < 8; ++i ) CHECK( s.params[ i ].reg( ) == PReg::make( i, codegen::RegClass::Float ) );
    CHECK( s.params[ 8 ].stack_offset( ) == 0 );
    CHECK( s.stack_arg_bytes == 8 );
}

TEST_CASE( "abi sysv interleaved overflow keeps stack order", "[abi][sysv]" )
{
    std::vector< Type > params( 7, Type::i64 );
    params.insert( params.end( ), 9, Type::f64 );
    params.push_back( Type::i64 );
    const auto s = sig( CallConv::SystemV, params );
    CHECK( s.params[ 6 ].stack_offset( ) == 0 );
    CHECK( s.params[ 15 ].stack_offset( ) == 8 );
    CHECK( s.params[ 16 ].stack_offset( ) == 16 );
    CHECK( s.stack_arg_bytes == 24 );
}

TEST_CASE( "abi sysv f32 is treated as an integer class argument", "[abi][sysv]" )
{
    const auto s = sig( CallConv::SystemV, { Type::f32 } );
    CHECK( s.params[ 0 ].reg( ) == RDI );
}

TEST_CASE( "abi sysv return registers", "[abi][sysv]" )
{
    CHECK( sig( CallConv::SystemV, { }, { Type::i64 } ).rets[ 0 ].reg( ) == RAX );
    CHECK( sig( CallConv::SystemV, { }, { Type::i8 } ).rets[ 0 ].reg( ) == RAX );
    CHECK( sig( CallConv::SystemV, { }, { Type::ptr } ).rets[ 0 ].reg( ) == RAX );
    CHECK( sig( CallConv::SystemV, { }, { Type::f64 } ).rets[ 0 ].reg( ) == XMM0 );
    CHECK( sig( CallConv::SystemV, { }, { Type::i32 } ).rets[ 0 ].ty == Type::i32 );
}

TEST_CASE( "abi fastcall with no params reserves shadow space", "[abi][win64]" )
{
    const auto s = sig( CallConv::WindowsFastcall, { } );
    CHECK( s.params.empty( ) );
    CHECK( s.stack_arg_bytes == 32 );
    CHECK( s.stack_align == 16 );
}

TEST_CASE( "abi fastcall integer arguments fill four registers", "[abi][win64]" )
{
    const std::vector< PReg > order = { RCX, RDX, R8, R9 };
    for ( std::size_t n { 1 }; n <= 4; ++n )
    {
        const auto s = sig( CallConv::WindowsFastcall, std::vector< Type >( n, Type::i64 ) );
        CHECK( regs_of( s.params ) == std::vector< PReg >( order.begin( ), order.begin( ) + static_cast< std::ptrdiff_t >( n ) ) );
        CHECK( s.stack_arg_bytes == 32 );
    }
}

TEST_CASE( "abi fastcall extra arguments follow the shadow space", "[abi][win64]" )
{
    const auto s = sig( CallConv::WindowsFastcall, std::vector< Type >( 8, Type::i64 ) );
    REQUIRE( s.params.size( ) == 8 );
    CHECK( s.params[ 4 ].stack_offset( ) == 32 );
    CHECK( s.params[ 5 ].stack_offset( ) == 40 );
    CHECK( s.params[ 6 ].stack_offset( ) == 48 );
    CHECK( s.params[ 7 ].stack_offset( ) == 56 );
    CHECK( s.stack_arg_bytes == 64 );
}

TEST_CASE( "abi fastcall floats are positional", "[abi][win64]" )
{
    const auto s = sig( CallConv::WindowsFastcall, { Type::i64, Type::f64, Type::i32, Type::f64, Type::f64 } );
    CHECK( s.params[ 0 ].reg( ) == RCX );
    CHECK( s.params[ 1 ].reg( ) == XMM1 );
    CHECK( s.params[ 2 ].reg( ) == R8 );
    CHECK( s.params[ 3 ].reg( ) == XMM3 );
    CHECK( s.params[ 4 ].stack_offset( ) == 32 );
    CHECK( s.stack_arg_bytes == 40 );
}

TEST_CASE( "abi fastcall all float registers", "[abi][win64]" )
{
    const auto s = sig( CallConv::WindowsFastcall, std::vector< Type >( 4, Type::f64 ) );
    CHECK( regs_of( s.params ) == std::vector< PReg > { XMM0, XMM1, XMM2, XMM3 } );
}

TEST_CASE( "abi fastcall return registers", "[abi][win64]" )
{
    CHECK( sig( CallConv::WindowsFastcall, { }, { Type::i64 } ).rets[ 0 ].reg( ) == RAX );
    CHECK( sig( CallConv::WindowsFastcall, { }, { Type::i16 } ).rets[ 0 ].reg( ) == RAX );
    CHECK( sig( CallConv::WindowsFastcall, { }, { Type::f64 } ).rets[ 0 ].reg( ) == XMM0 );
}

TEST_CASE( "abi param types are preserved", "[abi]" )
{
    const std::vector< Type > params = { Type::i8, Type::i16, Type::i32, Type::i64, Type::ptr, Type::f64 };
    for ( const auto cc : { CallConv::SystemV, CallConv::WindowsFastcall } )
    {
        const auto s = sig( cc, params );
        REQUIRE( s.params.size( ) == params.size( ) );
        for ( std::size_t i { 0 }; i < params.size( ); ++i ) CHECK( s.params[ i ].ty == params[ i ] );
    }
}

TEST_CASE( "abi host spec is a singleton", "[abi]" )
{
    CHECK( &host_abi( ) == &host_abi( ) );
    CHECK( &host_abi_spec( ) == static_cast< const codegen::ABISpec* >( &host_abi( ) ) );
}

TEST_CASE( "machenv sysv contents", "[abi][machenv]" )
{
    const auto env = sysv_env( );
    CHECK( span_vec( env.preferred[ 0 ] ) == std::vector< PReg > { RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11 } );
    CHECK( span_vec( env.non_preferred[ 0 ] ) == std::vector< PReg > { RBX, R12, R13, R14, R15 } );
    CHECK( env.preferred[ 1 ].size( ) == 16 );
    CHECK( env.non_preferred[ 1 ].empty( ) );
    CHECK( span_vec( env.scratch[ 0 ] ) == std::vector< PReg > { R10, R11 } );
    CHECK( span_vec( env.scratch[ 1 ] ) == std::vector< PReg > { XMM14 } );
}

TEST_CASE( "machenv win64 contents", "[abi][machenv]" )
{
    const auto env = win64_env( );
    CHECK( span_vec( env.preferred[ 0 ] ) == std::vector< PReg > { RAX, RCX, RDX, R8, R9, R10, R11 } );
    CHECK( span_vec( env.non_preferred[ 0 ] ) == std::vector< PReg > { RBX, RSI, RDI, R12, R13, R14, R15 } );
    CHECK( span_vec( env.preferred[ 1 ] ) == std::vector< PReg > { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5 } );
    CHECK( env.non_preferred[ 1 ].empty( ) );
    CHECK( span_vec( env.scratch[ 0 ] ) == std::vector< PReg > { R10, R11 } );
    CHECK( span_vec( env.scratch[ 1 ] ) == std::vector< PReg > { XMM4 } );
}

TEST_CASE( "machenv never hands out rsp or rbp", "[abi][machenv]" )
{
    for ( const auto& env : { sysv_env( ), win64_env( ) } )
        for ( const auto& list : { env.preferred[ 0 ], env.non_preferred[ 0 ], env.scratch[ 0 ] } )
        {
            CHECK( std::ranges::find( list, RSP ) == list.end( ) );
            CHECK( std::ranges::find( list, RBP ) == list.end( ) );
        }
}

TEST_CASE( "machenv register lists have the right classes", "[abi][machenv]" )
{
    for ( const auto& env : { sysv_env( ), win64_env( ) } )
    {
        for ( const auto pr : env.preferred[ 0 ] ) CHECK( pr.rc( ) == codegen::RegClass::Int );
        for ( const auto pr : env.non_preferred[ 0 ] ) CHECK( pr.rc( ) == codegen::RegClass::Int );
        for ( const auto pr : env.preferred[ 1 ] ) CHECK( pr.rc( ) == codegen::RegClass::Float );
        for ( const auto pr : env.scratch[ 1 ] ) CHECK( pr.rc( ) == codegen::RegClass::Float );
    }
}

TEST_CASE( "machenv env_for selects by calling convention", "[abi][machenv]" )
{
    CHECK( span_vec( env_for( CallConv::SystemV ).non_preferred[ 0 ] ) == span_vec( sysv_env( ).non_preferred[ 0 ] ) );
    CHECK( span_vec( env_for( CallConv::WindowsFastcall ).non_preferred[ 0 ] ) == span_vec( win64_env( ).non_preferred[ 0 ] ) );
    CHECK( span_vec( host_machine_env( ).preferred[ 0 ] ) == span_vec( env_for( codegen::host_call_conv( ) ).preferred[ 0 ] ) );
}

TEST_CASE( "machenv win64 preferred matches the volatile registers", "[abi][machenv]" )
{
    const auto env = win64_env( );
    for ( const auto pr : { RBX, RSI, RDI, R12, R13, R14, R15 } )
        CHECK( std::ranges::find( env.preferred[ 0 ], pr ) == env.preferred[ 0 ].end( ) );
}

TEST_CASE( "allocation order for win64 excludes scratch registers", "[abi][machenv][regalloc]" )
{
    const auto order = regalloc::allocation_order( win64_env( ) );
    CHECK( order[ 0 ] == std::vector< PReg > { RAX, RCX, RDX, R8, R9, RBX, RSI, RDI, R12, R13, R14, R15 } );
    CHECK( order[ 1 ] == std::vector< PReg > { XMM0, XMM1, XMM2, XMM3, XMM5 } );
}

TEST_CASE( "allocation order for sysv excludes scratch registers", "[abi][machenv][regalloc]" )
{
    const auto order = regalloc::allocation_order( sysv_env( ) );
    CHECK( order[ 0 ] == std::vector< PReg > { RAX, RCX, RDX, RSI, RDI, R8, R9, RBX, R12, R13, R14, R15 } );
    REQUIRE( order[ 1 ].size( ) == 15 );
    CHECK( std::ranges::find( order[ 1 ], XMM14 ) == order[ 1 ].end( ) );
    CHECK( order[ 1 ].back( ) == XMM15 );
}

TEST_CASE( "allocation order deduplicates and keeps first occurrence", "[abi][machenv][regalloc]" )
{
    static constexpr PReg     pref[]    = { RAX, RCX, RAX, RDX };
    static constexpr PReg     nonpref[] = { RCX, RBX, RDX, R12, RBX };
    static constexpr PReg     scratch[] = { RDX };
    const codegen::MachineEnv env {
        .preferred     = {   pref, std::span< const PReg > { }},
        .non_preferred = {nonpref, std::span< const PReg > { }},
        .scratch       = {scratch, std::span< const PReg > { }},
    };
    const auto order = regalloc::allocation_order( env );
    CHECK( order[ 0 ] == std::vector< PReg > { RAX, RCX, RBX, R12 } );
    CHECK( order[ 1 ].empty( ) );
}

TEST_CASE( "allocation order of an empty env is empty", "[abi][machenv][regalloc]" )
{
    const codegen::MachineEnv env { };
    const auto                order = regalloc::allocation_order( env );
    CHECK( order[ 0 ].empty( ) );
    CHECK( order[ 1 ].empty( ) );
}

TEST_CASE( "x86 isa identifies itself", "[abi][isa]" )
{
    const X86Isa isa;
    CHECK( isa.name( ) == "x86_64" );
    CHECK( isa.default_cc( ) == codegen::host_call_conv( ) );
}

TEST_CASE( "lookup_host returns the x86 isa", "[abi][isa]" )
{
    const auto isa = isa::lookup_host( );
    REQUIRE( isa != nullptr );
    CHECK( isa->name( ) == "x86_64" );
    CHECK( isa->default_cc( ) == codegen::host_call_conv( ) );
    CHECK( dynamic_cast< const X86Isa* >( isa.get( ) ) != nullptr );
}

TEST_CASE( "host call conv matches the platform", "[abi][isa]" )
{
#if defined( _WIN64 )
    CHECK( codegen::host_call_conv( ) == CallConv::WindowsFastcall );
#else
    CHECK( codegen::host_call_conv( ) == CallConv::SystemV );
#endif
    STATIC_CHECK( codegen::host_call_conv( ) == codegen::host_call_conv( ) );
}

TEST_CASE( "x86 isa compiles a trivial function", "[abi][isa]" )
{
    ir::IRModule irm;
    const auto   ref = irm.declare( "f", Type::i64, { Type::i64 } );
    {
        ir::IRBuilder b { *irm.get( ref ) };
        b.switch_to_block( b.add_block( { Type::i64 } ) );
        b.ret( b.param( 0 ) );
        b.finalise( );
    }
    const X86Isa isa;
    const auto   code = isa.compile_function( *irm.get( ref ) );
    REQUIRE( code.has_value( ) );
    CHECK_FALSE( code->code.empty( ) );
    CHECK( code->code.back( ) == 0xC3 );
    CHECK( code->relocs.empty( ) );
    CHECK( code->alignment == 16 );
}
