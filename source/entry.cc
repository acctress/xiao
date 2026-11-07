#include <xiao/codegen/context.hh>
#include <xiao/isa/host.hh>
#include <xiao/isa/x86/machenv.hh>
#include <xiao/isa/x86/isa.hh>
#include <xiao/isa/x86/fmt.hh>
#include <xiao/regalloc/regalloc.hh>

#include <xiao/ir/builder.hh>
#include <xiao/ir/verifier.hh>
#include <xiao/ir/module.hh>
#include <xiao/isa/x86/emit.hh>
#include <xiao/isa/x86/abi.hh>
#include <xiao/isa/x86/lower.hh>
#include <xiao-jit/memory.hh>

#include <algorithm>
#include <iterator>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>

using namespace xiao;
using namespace xiao::ir;

namespace dbg
{
    constexpr std::string_view reset   = "\x1b[0m";
    constexpr std::string_view dim     = "\x1b[2m";
    constexpr std::string_view bold    = "\x1b[1m";
    constexpr std::string_view cyan    = "\x1b[36m";
    constexpr std::string_view yellow  = "\x1b[33m";
    constexpr std::string_view green   = "\x1b[32m";
    constexpr std::string_view magenta = "\x1b[35m";
    constexpr std::string_view red     = "\x1b[38;5;198m";

    void header( const std::string_view title )
    {
        const auto pad = title.size( ) < 64 ? 64 - title.size( ) : 0;
        std::println( "\n{}{}-- {} {}{}", bold, cyan, title, std::string( pad, '-' ), reset );
    }

    [[nodiscard]] std::string preg( const codegen::PReg pr )
    { return pr.valid( ) ? std::string( x86_64::preg_name( pr ) ) : "<none>"; }

    //! @brief Pinned vregs print as their physical name, real vregs as vN
    [[nodiscard]] std::string vreg( const codegen::VReg vr )
    {
        const auto r = codegen::Reg::from_vreg( vr );
        if ( r.is_physical( ) ) return preg( r.as_preg( ) );
        return std::format( "v{}", vr.index( ) );
    }

    [[nodiscard]] std::string_view kind( const codegen::OperandKind k )
    {
        switch ( k )
        {
            case codegen::OperandKind::Use: return "use";
            case codegen::OperandKind::Def: return "def";
            default: return "mod";
        }
    }

    [[nodiscard]] std::string vreg_set( const std::unordered_set< uint32_t >& set )
    {
        std::vector sorted( set.begin( ), set.end( ) );
        std::ranges::sort( sorted );

        std::string out;
        for ( const auto v : sorted ) out += std::format( "{}v{}", out.empty( ) ? "" : " ", v );
        return out.empty( ) ? "-" : out;
    }
} // namespace dbg

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
    {
        for ( const auto& [ msg ] : verifier.errs ) std::println( "{}[error]{} {}", dbg::red, dbg::reset, msg );
        return 1;
    }

    auto& fn = *module.get( ref );

    dbg::header( "ir" );
    {
        std::string text;
        module.fmt( std::back_inserter( text ) );
        std::print( "{}", text );
    }

    const auto        env = x86_64::env_for( fn.sig.cc );
    x86_64::IRLowerer lowerer { env, x86_64::host_abi( ) };
    const auto        vcode    = lowerer.lower( fn );
    const auto        liveness = regalloc::Liveness::compute( vcode );
    const auto        allocs   = regalloc::allocate( vcode, env );

    std::unordered_map< uint32_t, codegen::PReg > assigned;
    for ( std::size_t i = 0; i < vcode.operands.size( ); ++i )
        if ( !vcode.operands[ i ].is_fixed( ) ) assigned.try_emplace( vcode.operands[ i ].vreg( ).index( ), allocs.ops[ i ] );

    dbg::header( std::format( "vcode  ({} blocks, {} insts, cc {})", vcode.blocks.size( ), vcode.insts.size( ),
                              fn.sig.cc == codegen::CallConv::SystemV ? "sysv" : "win64" ) );

    for ( uint32_t bi = 0; bi < vcode.blocks.size( ); ++bi )
    {
        const auto& blk = vcode.blocks[ bi ];

        std::string params;
        for ( uint16_t p = 0; p < blk.params_count; ++p )
            params += std::format( "{}{}", p ? ", " : "", dbg::vreg( vcode.block_params[ blk.params_offset + p ] ) );

        std::string succs;
        for ( uint8_t s = 0; s < blk.succs_count; ++s )
            succs += std::format( "{}block{}", s ? ", " : "", vcode.succs[ blk.succs_offset + s ].id );

        std::println( "{}{}block{}{}({}){}{}", bi ? "\n" : "", dbg::bold, bi, dbg::reset, params, succs.empty( ) ? "" : "  → ", succs );
        std::println( "  {}live-in  {}{}", dbg::dim, dbg::vreg_set( liveness.blocks[ bi ].live_in ), dbg::reset );

        for ( uint32_t ii = 0; ii < blk.insts_count; ++ii )
        {
            const auto idx          = blk.insts_offset + ii;
            const auto [ off, cnt ] = vcode.operand_ranges[ idx ];

            std::string ops;
            for ( auto o = off; o < off + cnt; ++o )
            {
                const auto& op = vcode.operands[ o ];
                if ( op.is_fixed( ) )
                    ops += std::format( "  {}{} {}{}", dbg::magenta, dbg::kind( op.kind( ) ), dbg::preg( *op.fixed_preg( ) ), dbg::reset );
                else
                    ops += std::format( "  {}{} {}→{}{}", dbg::yellow, dbg::kind( op.kind( ) ), dbg::vreg( op.vreg( ) ),
                                        dbg::preg( allocs.ops[ o ] ), dbg::reset );
            }

            std::println( "  {}{:3}{}  {:<22}{}", dbg::dim, idx, dbg::reset, vcode.insts[ idx ].to_string( ), ops );
        }

        std::println( "  {}live-out {}{}", dbg::dim, dbg::vreg_set( liveness.blocks[ bi ].live_out ), dbg::reset );
    }

    dbg::header( "live intervals" );
    {
        auto intervals = regalloc::LiveIntervals::build( vcode, liveness ).intervals;
        std::ranges::stable_sort( intervals, { }, &regalloc::LiveInterval::start );

        for ( const auto& iv : intervals )
        {
            std::string ranges;
            for ( const auto& [ s, e ] : iv.ranges ) ranges += std::format( "[{},{}) ", s, e );

            if ( iv.fixed )
                std::println( "  {}{:<6} fixed{}  {:<24} {}{}{}", dbg::magenta, dbg::preg( *iv.fixed ), dbg::reset, ranges, dbg::magenta,
                              dbg::preg( *iv.fixed ), dbg::reset );
            else
            {
                const auto it  = assigned.find( iv.vreg );
                const auto reg = it != assigned.end( ) ? dbg::preg( it->second ) : std::string( "<spilled>" );
                std::println( "  {}v{:<5}{} {:<5}  {:<24} {}{}{}", dbg::yellow, iv.vreg, dbg::reset,
                              iv.cls == codegen::RegClass::Int ? "int" : "float", ranges, dbg::green, reg, dbg::reset );
            }
        }
    }

    auto             isa = isa::lookup_host( );
    codegen::Context ctx { fn };
    const auto       res = ctx.compile( *isa );
    if ( !res )
    {
        std::println( "{}[error]{} compile failed", dbg::red, dbg::reset );
        return 1;
    }

    const auto& code = ( *res )->code;

    dbg::header( std::format( "machine code  ({} bytes, {} relocs)", code.size( ), ( *res )->relocs.size( ) ) );
    for ( std::size_t i = 0; i < code.size( ); i += 16 )
    {
        std::print( "  {}{:04x}{}  ", dbg::dim, i, dbg::reset );
        for ( std::size_t j = i; j < std::min( i + 16, code.size( ) ); ++j ) std::print( "{:02x} ", code[ j ] );
        std::println( "" );
    }

    dbg::header( "output" );
    {
        jit::JITMemoryProvider mem { code.size( ) };
        mem.write( code );
        if ( !mem.finalise_permissions( ) )
        {
            std::println( "{}[error]{} could not make memory executable", dbg::red, dbg::reset );
            return 1;
        }

        using MaxFn = int64_t ( * )( int64_t, int64_t );
        auto* max   = reinterpret_cast< MaxFn >( mem.data( ) );

        constexpr std::pair< int64_t, int64_t > cases[] = {
            {        3,         7},
            {        9,        -2},
            {       -5,        -5},
            {     -100,       100},
            {INT64_MAX, INT64_MIN}
        };
        for ( const auto [ a, c ] : cases )
        {
            const auto got  = max( a, c );
            const auto want = std::max( a, c );
            std::println( "  max({}, {}) = {}  {}{}{}", a, c, got, got == want ? dbg::green : dbg::red, got == want ? "ok" : "WRONG",
                          dbg::reset );
        }
    }
}
