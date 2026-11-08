#include <xiao-jit/module.hh>
#include <xiao/isa/host.hh>
#include <xiao/ir/builder.hh>
#include <xiao/ir/module.hh>
#include <xiao/ir/verifier.hh>

#ifdef XIAO_HAS_ZYDIS
#include <xiao-disasm/disasm.hh>
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace xiao;
using ir::Type;
using ir::ValueRef;

using u64 = uint64_t;
using i64 = int64_t;

struct Item
{
    int64_t value;
    int32_t weight;
    int16_t delta;
    uint8_t flag;
    uint8_t tag;
};
static_assert( sizeof( Item ) == 16 );

struct Wide
{
    int64_t  q;
    int32_t  d;
    int16_t  w;
    int8_t   b;
    uint8_t  u;
};
static_assert( sizeof( Wide ) == 16 );

static int g_sqr_calls = 0;

extern "C" int64_t sqr_func( const int64_t v )
{
    ++g_sqr_calls;
    return static_cast< i64 >( static_cast< u64 >( v ) * static_cast< u64 >( v ) );
}

extern "C" int64_t poly4( const int64_t a, const int64_t b, const int64_t c, const int64_t d )
{ return static_cast< i64 >( static_cast< u64 >( a ) + 2 * static_cast< u64 >( b ) + 3 * static_cast< u64 >( c ) + 4 * static_cast< u64 >( d ) ); }

static std::string g_printed;

extern "C" void host_print( const char* const text, const int64_t len )
{
    g_printed.append( text, static_cast< std::size_t >( len ) );
    std::print( "  [jit] {}", std::string_view { text, static_cast< std::size_t >( len ) } );
}

constexpr std::string_view k_greeting = "hello from xiao\n";
constexpr std::string_view k_vowels   = "aeiouAEIOU";
constexpr std::string_view k_texts    = "strings live in data objects\nand jit code reads them through symbol_value\n";
constexpr std::size_t      k_split    = 29;

static i64 wrap( const u64 v ) { return static_cast< i64 >( v ); }
static u64 bits( const i64 v ) { return static_cast< u64 >( v ); }

i64 mix_ref( const i64 x, const i64 y ) { return wrap( bits( x ) * 31u ) ^ y; }

i64 crunch_ref( Item* items, const i64 n )
{
    i64 acc = 0;
    for ( i64 i = 0; i < n; ++i )
    {
        auto&     it = items[ i ];
        const i64 v  = it.value;
        const i64 w  = it.weight;
        const i64 d  = it.delta;
        const i64 f  = it.flag;

        const u64 sq  = bits( v ) * bits( v );
        const u64 a   = sq / 3 + bits( v % 5 ) - bits( w ) % 7;
        const u64 bb  = ( ( bits( v ) << 2 ) ^ bits( w >> 1 ) ) | ( bits( d ) >> 3 );
        const i64 c   = wrap( ( a & ~bb ) ^ bb );
        const i64 neg = wrap( 0 - bits( d ) );
        const i64 m   = f ? c : neg;
        const i64 r   = m > 0 ? m / 2 : wrap( bits( m ) * bits( -3 ) );

        it.value = r;
        it.delta = static_cast< int16_t >( r );
        it.tag   = static_cast< uint8_t >( r );
        acc      = wrap( bits( mix_ref( acc, r ) ) + bits( i ) );
    }
    return acc;
}

constexpr std::array< ir::ICmpCC, 10 > k_conds { ir::ICmpCC::eq,  ir::ICmpCC::ne,  ir::ICmpCC::slt, ir::ICmpCC::sle, ir::ICmpCC::sgt,
                                                 ir::ICmpCC::sge, ir::ICmpCC::ult, ir::ICmpCC::ule, ir::ICmpCC::ugt, ir::ICmpCC::uge };

i64 compare_ref( const i64 a, const i64 b )
{
    const bool r[] = { a == b, a != b, a < b, a <= b, a > b, a >= b, bits( a ) < bits( b ), bits( a ) <= bits( b ), bits( a ) > bits( b ), bits( a ) >= bits( b ) };
    i64        m   = 0;
    for ( int k = 0; k < 10; ++k ) m |= static_cast< i64 >( r[ k ] ) << k;
    return m;
}

i64 shifts_ref( const i64 x, const i64 n )
{
    const auto s = static_cast< int >( n & 63 );
    return wrap( ( ( bits( x ) << s ) ^ ( bits( x ) >> s ) ) + bits( x >> s ) );
}

i64 divmod_ref( const i64 a, const i64 b )
{ return wrap( bits( ( wrap( bits( a / b ) * 7 + bits( a % b ) * 3 ) ) ^ wrap( bits( a ) / bits( b ) ) ) + bits( a ) % bits( b ) ); }

i64 narrow_ref( const Wide& s, Wide& out )
{
    const int32_t d2  = static_cast< int32_t >( static_cast< uint32_t >( s.d ) + 0x7fff0000u );
    const int16_t w2  = static_cast< int16_t >( static_cast< uint16_t >( s.w ) * 3u );
    const int8_t  b2  = static_cast< int8_t >( static_cast< uint8_t >( s.b ) ^ 0x5au );
    const u64     sum = bits( s.q ) + bits( s.d ) + static_cast< uint32_t >( s.d ) + bits( s.w ) + static_cast< uint16_t >( s.w ) + bits( s.b ) + s.u
                  + bits( d2 );
    out = { wrap( sum ), d2, w2, b2, static_cast< uint8_t >( sum ) };
    return wrap( sum );
}

i64 fact_ref( const i64 n ) { return n <= 1 ? 1 : wrap( bits( n ) * bits( fact_ref( n - 1 ) ) ); }

i64 gcd_ref( u64 a, u64 b )
{
    while ( b ) a = std::exchange( b, a % b );
    return wrap( a );
}

i64 classify_ref( const i64 x, i64* p )
{
    if ( x != 0 ) *p = wrap( bits( x ) * bits( x ) );
    return wrap( bits( *p ) + bits( x ) * 3 );
}

i64 hostcalls_ref( const i64 a, const i64 b, const i64 c, const i64 d )
{ return wrap( bits( poly4( a, b, c, d ) ) + bits( std::llabs( a ) ) * 1000 + bits( std::llabs( d ) ) ); }

i64 fill_sum_ref( const i64 n )
{
    u64 acc = 0;
    for ( i64 i = 0; i < n; ++i ) acc += 5 * bits( i ) + bits( i );
    return wrap( acc );
}

constexpr int k_pressure_values = 16;

i64 pressure_ref( const i64 x )
{
    std::array< u64, k_pressure_values > a { };
    for ( int k = 0; k < k_pressure_values; ++k ) a[ k ] = bits( x ) * static_cast< u64 >( k + 3 ) + static_cast< u64 >( k * k + 1 );

    u64 acc = 0;
    for ( i64 i = 0; i < 8; ++i )
    {
        u64 t = acc + bits( i ^ x ) * bits( i ^ x );
        for ( int k = 0; k < k_pressure_values; ++k ) t = k % 2 == 0 ? t + a[ k ] : t ^ a[ k ];
        acc = t;
    }
    for ( int k = 0; k < k_pressure_values; ++k ) acc -= a[ k ];
    return wrap( acc );
}

i64 consts_ref( const i64 x )
{
    const u64 r  = ( ( bits( x ) + 0x123456789ABCDEF0ull ) & 0xFFFF0000FFFF0000ull ) | 0x10000000000ull;
    const u64 r2 = bits( x ) * bits( -2147483648ll );
    const u64 r3 = bits( x ) - 2147483647ull;
    return x > -5 ? wrap( r ^ r2 ) : wrap( r3 );
}

struct Fn
{
    ir::FunctionRef ref;
    module::FuncId  id;
    std::string     name;
};

static int g_checks = 0;
static int g_fails  = 0;

void check( const std::string_view what, const i64 got, const i64 want )
{
    ++g_checks;
    const bool ok = got == want;
    if ( !ok ) ++g_fails;
    std::println( "  {:<4} {:<44} = {}{}", ok ? "ok" : "FAIL", what, got, ok ? "" : std::format( "  (want {})", want ) );
}

int main( int argc, char** argv )
{
    bool show_ir { false }, show_asm { false };
    for ( int k = 1; k < argc; ++k )
    {
        const std::string_view a { argv[ k ] };
        show_ir |= a == "ir" || a == "all";
        show_asm |= a == "asm" || a == "all";
    }

    jit::JITBuilder builder { isa::lookup_host( ) };
    builder.symbol( "sqr_func", reinterpret_cast< const void* >( &sqr_func ) );
    builder.symbol( "poly4", reinterpret_cast< const void* >( &poly4 ) );
    builder.symbol( "host_print", reinterpret_cast< const void* >( &host_print ) );
    jit::JITModule jit { std::move( builder ) };

    const auto cc        = codegen::host_call_conv( );
    const auto square_id = *jit.declare_function( "sqr_func", module::Linkage::Import, { { Type::i64 }, Type::i64, cc } );
    const auto poly4_id  = *jit.declare_function( "poly4", module::Linkage::Import, { { Type::i64, Type::i64, Type::i64, Type::i64 }, Type::i64, cc } );
    const auto llabs_id  = *jit.declare_function( "llabs", module::Linkage::Import, { { Type::i64 }, Type::i64, cc } );

    ir::IRModule      irm;
    std::vector< Fn > fns;

    const auto declare = [ & ]( const std::string_view name, const module::Linkage linkage, const Type ret, const std::initializer_list< Type > params )
    {
        const auto ref = irm.declare( name, ret, params );
        const auto id  = *jit.declare_function( name, linkage, module::signature_of( *irm.get( ref ) ) );
        fns.push_back( { ref, id, std::string { name } } );
        return fns.back( );
    };

    const auto i64c = [ ]( ir::IRBuilder& b, const i64 v ) { return b.iconst( Type::i64, bits( v ) ); };

    const auto mix       = declare( "mix", module::Linkage::Local, Type::i64, { Type::i64, Type::i64 } );
    const auto crunch    = declare( "crunch", module::Linkage::Export, Type::i64, { Type::ptr, Type::i64 } );
    const auto compare   = declare( "compare", module::Linkage::Export, Type::i64, { Type::i64, Type::i64 } );
    const auto shifts    = declare( "shifts", module::Linkage::Export, Type::i64, { Type::i64, Type::i64 } );
    const auto divmod    = declare( "divmod", module::Linkage::Export, Type::i64, { Type::i64, Type::i64 } );
    const auto narrow    = declare( "narrow", module::Linkage::Export, Type::i64, { Type::ptr, Type::ptr } );
    const auto fact      = declare( "fact", module::Linkage::Export, Type::i64, { Type::i64 } );
    const auto gcd       = declare( "gcd", module::Linkage::Export, Type::i64, { Type::i64, Type::i64 } );
    const auto classify  = declare( "classify", module::Linkage::Export, Type::i64, { Type::i64, Type::ptr } );
    const auto hostcalls = declare( "hostcalls", module::Linkage::Export, Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 } );
    const auto fill      = declare( "fill", module::Linkage::Export, Type::unit, { Type::ptr, Type::i64, Type::i64 } );
    const auto fill_sum  = declare( "fill_sum", module::Linkage::Export, Type::i64, { Type::ptr, Type::i64 } );
    const auto pressure  = declare( "pressure", module::Linkage::Export, Type::i64, { Type::i64 } );
    const auto consts    = declare( "consts", module::Linkage::Export, Type::i64, { Type::i64 } );
    const auto greet     = declare( "greet", module::Linkage::Export, Type::i64, { } );
    const auto vowels    = declare( "count_vowels", module::Linkage::Export, Type::i64, { Type::ptr, Type::i64 } );
    const auto message   = declare( "print_message", module::Linkage::Export, Type::i64, { Type::i64 } );

    const auto print_id    = *jit.declare_function( "host_print", module::Linkage::Import, { { Type::ptr, Type::i64 }, Type::unit, cc } );
    const auto greeting_id = *jit.declare_data( "greeting", module::Linkage::Local, false );
    const auto vowels_id   = *jit.declare_data( "vowels", module::Linkage::Local, false );
    const auto texts_id    = *jit.declare_data( "texts", module::Linkage::Local, false );
    const auto messages_id = *jit.declare_data( "messages", module::Linkage::Local, false );
    const auto counter_id  = *jit.declare_data( "greet_count", module::Linkage::Export, true );

    {
        const auto define = [ & ]( const module::DataId id, const module::DataDescription& d )
        {
            if ( !jit.define_data( id, d ) ) std::println( "[error] failed to define data {}", id.id );
        };

        module::DataDescription d;
        d.define( k_greeting );
        define( greeting_id, d );

        d.clear( );
        d.define( k_vowels );
        define( vowels_id, d );

        d.clear( );
        d.define( k_texts );
        define( texts_id, d );

        d.clear( );
        std::array< int64_t, 4 > entries { 0, static_cast< int64_t >( k_split ), 0, static_cast< int64_t >( k_texts.size( ) - k_split ) };
        d.define( std::span { reinterpret_cast< const uint8_t* >( entries.data( ) ), sizeof entries } );
        d.set_align( 8 );
        const auto texts = jit.declare_data_in_data( texts_id, d );
        d.write_data_addr( 0, texts, 0 );
        d.write_data_addr( 16, texts, static_cast< int64_t >( k_split ) );
        define( messages_id, d );

        d.clear( );
        d.define_zeroinit( 8 );
        define( counter_id, d );
    }

    {
        ir::IRBuilder b { *irm.get( mix.ref ) };
        b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
        b.ret( b.bxor( b.imul( b.param( 0 ), i64c( b, 31 ) ), b.param( 1 ) ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( crunch.ref ) };
        const auto    square = jit.declare_func_in_func( square_id, b );
        const auto    mixf   = jit.declare_func_in_func( mix.id, b );

        const auto entry  = b.add_block( { Type::ptr, Type::i64 } );
        const auto header = b.add_block( { Type::i64, Type::i64, Type::ptr } );
        const auto body   = b.add_block( { } );
        const auto join   = b.add_block( { Type::i64 } );
        const auto exit   = b.add_block( { Type::i64 } );

        b.switch_to_block( entry );
        const auto     n       = b.param( 1 );
        const auto     zero    = i64c( b, 0 );
        const ValueRef start[] = { zero, zero, b.param( 0 ) };
        b.br( header, start );

        b.switch_to_block( header );
        const auto     i      = b.block_param( header, 0 );
        const auto     acc    = b.block_param( header, 1 );
        const auto     q      = b.block_param( header, 2 );
        const ValueRef done[] = { acc };
        b.brif( b.icmp( ir::ICmpCC::slt, i, n ), body, { }, exit, done );

        b.switch_to_block( body );
        const auto v = b.load( Type::i64, q );
        const auto w = b.sextend( Type::i64, b.load( Type::i32, q, 8 ) );
        const auto d = b.sextend( Type::i64, b.load( Type::i16, q, 12 ) );
        const auto f = b.uextend( Type::i64, b.load( Type::i8, q, 14 ) );

        const ValueRef sq_args[] = { v };
        const auto     sq        = b.call( square, sq_args );

        const auto a  = b.isub( b.iadd( b.udiv( sq, i64c( b, 3 ) ), b.srem( v, i64c( b, 5 ) ) ), b.urem( w, i64c( b, 7 ) ) );
        const auto bb = b.bor( b.bxor( b.ishl( v, i64c( b, 2 ) ), b.sshr( w, i64c( b, 1 ) ) ), b.ushr( d, i64c( b, 3 ) ) );
        const auto c  = b.bxor( b.band( a, b.bnot( bb ) ), bb );
        const auto m  = b.select( f, c, b.ineg( d ) );

        const ValueRef halved[]  = { b.idiv( m, i64c( b, 2 ) ) };
        const ValueRef tripled[] = { b.imul( m, i64c( b, -3 ) ) };
        b.brif( b.icmp( ir::ICmpCC::sgt, m, i64c( b, 0 ) ), join, halved, join, tripled );

        b.switch_to_block( join );
        const auto r = b.block_param( join, 0 );
        b.store( r, q );
        b.store( b.ireduce( Type::i16, r ), q, 12 );
        b.store( b.ireduce( Type::i8, r ), q, 15 );

        const ValueRef mix_args[] = { acc, r };
        const auto     acc2       = b.iadd( b.call( mixf, mix_args ), i );
        const ValueRef next[]     = { b.iadd( i, i64c( b, 1 ) ), acc2, b.iadd( q, i64c( b, sizeof( Item ) ) ) };
        b.br( header, next );

        b.switch_to_block( exit );
        b.ret( b.block_param( exit, 0 ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( compare.ref ) };
        b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
        auto mask = i64c( b, 0 );
        for ( std::size_t k = 0; k < k_conds.size( ); ++k )
        {
            const auto bit = b.uextend( Type::i64, b.icmp( k_conds[ k ], b.param( 0 ), b.param( 1 ) ) );
            mask           = b.bor( mask, b.ishl( bit, i64c( b, static_cast< i64 >( k ) ) ) );
        }
        b.ret( mask );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( shifts.ref ) };
        b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
        const auto x = b.param( 0 );
        const auto n = b.band( b.param( 1 ), i64c( b, 63 ) );
        b.ret( b.iadd( b.bxor( b.ishl( x, n ), b.ushr( x, n ) ), b.sshr( x, n ) ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( divmod.ref ) };
        b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
        const auto x  = b.param( 0 );
        const auto y  = b.param( 1 );
        const auto qr = b.iadd( b.imul( b.idiv( x, y ), i64c( b, 7 ) ), b.imul( b.srem( x, y ), i64c( b, 3 ) ) );
        b.ret( b.iadd( b.bxor( qr, b.udiv( x, y ) ), b.urem( x, y ) ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( narrow.ref ) };
        b.switch_to_block( b.add_block( { Type::ptr, Type::ptr } ) );
        const auto src = b.param( 0 );
        const auto dst = b.param( 1 );

        const auto q  = b.load( Type::i64, src );
        const auto d  = b.load( Type::i32, src, 8 );
        const auto w  = b.load( Type::i16, src, 12 );
        const auto sb = b.load( Type::i8, src, 14 );
        const auto u  = b.load( Type::i8, src, 15 );

        const auto d2 = b.iadd( d, b.iconst( Type::i32, 0x7fff0000u ) );
        const auto w2 = b.imul( w, b.iconst( Type::i16, 3 ) );
        const auto b2 = b.bxor( sb, b.iconst( Type::i8, 0x5a ) );
        b.store( d2, dst, 8 );
        b.store( w2, dst, 12 );
        b.store( b2, dst, 14 );

        auto sum = q;
        for ( const auto part : { b.sextend( Type::i64, d ), b.uextend( Type::i64, d ), b.sextend( Type::i64, w ), b.uextend( Type::i64, w ),
                                  b.sextend( Type::i64, sb ), b.uextend( Type::i64, u ), b.sextend( Type::i64, d2 ) } )
            sum = b.iadd( sum, part );

        b.store( sum, dst );
        b.store( b.ireduce( Type::i8, sum ), dst, 15 );
        b.ret( sum );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( fact.ref ) };
        const auto    self  = jit.declare_func_in_func( fact.id, b );
        const auto    entry = b.add_block( { Type::i64 } );
        const auto    base  = b.add_block( { } );
        const auto    rec   = b.add_block( { } );

        b.switch_to_block( entry );
        const auto n = b.param( 0 );
        b.brif( b.icmp( ir::ICmpCC::sle, n, i64c( b, 1 ) ), base, { }, rec, { } );

        b.switch_to_block( base );
        b.ret( i64c( b, 1 ) );

        b.switch_to_block( rec );
        const ValueRef args[] = { b.isub( n, i64c( b, 1 ) ) };
        b.ret( b.imul( n, b.call( self, args ) ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( gcd.ref ) };
        const auto    entry  = b.add_block( { Type::i64, Type::i64 } );
        const auto    header = b.add_block( { Type::i64, Type::i64 } );
        const auto    body   = b.add_block( { } );
        const auto    done   = b.add_block( { Type::i64 } );

        b.switch_to_block( entry );
        const ValueRef start[] = { b.param( 0 ), b.param( 1 ) };
        b.br( header, start );

        b.switch_to_block( header );
        const auto     x        = b.block_param( header, 0 );
        const auto     y        = b.block_param( header, 1 );
        const ValueRef result[] = { x };
        b.brif( b.icmp( ir::ICmpCC::eq, y, i64c( b, 0 ) ), done, result, body, { } );

        b.switch_to_block( body );
        const ValueRef next[] = { y, b.urem( x, y ) };
        b.br( header, next );

        b.switch_to_block( done );
        b.ret( b.block_param( done, 0 ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( classify.ref ) };
        const auto    entry = b.add_block( { Type::i64, Type::ptr } );
        const auto    mid   = b.add_block( { } );
        const auto    tail  = b.add_block( { } );

        b.switch_to_block( entry );
        const auto x = b.param( 0 );
        const auto p = b.param( 1 );
        const auto t = b.imul( x, i64c( b, 3 ) );
        b.brif( b.icmp( ir::ICmpCC::eq, x, i64c( b, 0 ) ), tail, { }, mid, { } );

        b.switch_to_block( mid );
        b.store( b.imul( x, x ), p );
        b.br( tail );

        b.switch_to_block( tail );
        b.ret( b.iadd( b.load( Type::i64, p ), t ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( hostcalls.ref ) };
        const auto    poly  = jit.declare_func_in_func( poly4_id, b );
        const auto    absf  = jit.declare_func_in_func( llabs_id, b );
        b.switch_to_block( b.add_block( { Type::i64, Type::i64, Type::i64, Type::i64 } ) );

        const ValueRef four[] = { b.param( 0 ), b.param( 1 ), b.param( 2 ), b.param( 3 ) };
        const ValueRef a[]    = { b.param( 0 ) };
        const ValueRef d[]    = { b.param( 3 ) };
        const auto     r      = b.call( poly, four );
        const auto     abs_a  = b.call( absf, a );
        const auto     abs_d  = b.call( absf, d );
        b.ret( b.iadd( b.iadd( r, b.imul( abs_a, i64c( b, 1000 ) ) ), abs_d ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( fill.ref ) };
        const auto    entry  = b.add_block( { Type::ptr, Type::i64, Type::i64 } );
        const auto    header = b.add_block( { Type::i64 } );
        const auto    body   = b.add_block( { } );
        const auto    done   = b.add_block( { } );

        b.switch_to_block( entry );
        const auto     p       = b.param( 0 );
        const auto     n       = b.param( 1 );
        const auto     v       = b.param( 2 );
        const ValueRef start[] = { i64c( b, 0 ) };
        b.br( header, start );

        b.switch_to_block( header );
        const auto i = b.block_param( header, 0 );
        b.brif( b.icmp( ir::ICmpCC::slt, i, n ), body, { }, done, { } );

        b.switch_to_block( body );
        b.store( b.iadd( b.imul( v, i ), i ), b.iadd( p, b.imul( i, i64c( b, 8 ) ) ) );
        const ValueRef next[] = { b.iadd( i, i64c( b, 1 ) ) };
        b.br( header, next );

        b.switch_to_block( done );
        b.ret( );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( fill_sum.ref ) };
        const auto    fillf  = jit.declare_func_in_func( fill.id, b );
        const auto    entry  = b.add_block( { Type::ptr, Type::i64 } );
        const auto    header = b.add_block( { Type::i64, Type::i64 } );
        const auto    body   = b.add_block( { } );
        const auto    done   = b.add_block( { Type::i64 } );

        b.switch_to_block( entry );
        const auto     p         = b.param( 0 );
        const auto     n         = b.param( 1 );
        const ValueRef fill_args[] = { p, n, i64c( b, 5 ) };
        ( void ) b.call( fillf, fill_args );
        const ValueRef start[] = { i64c( b, 0 ), i64c( b, 0 ) };
        b.br( header, start );

        b.switch_to_block( header );
        const auto     i      = b.block_param( header, 0 );
        const auto     acc    = b.block_param( header, 1 );
        const ValueRef result[] = { acc };
        b.brif( b.icmp( ir::ICmpCC::slt, i, n ), body, { }, done, result );

        b.switch_to_block( body );
        const auto     val    = b.load( Type::i64, b.iadd( p, b.imul( i, i64c( b, 8 ) ) ) );
        const ValueRef next[] = { b.iadd( i, i64c( b, 1 ) ), b.iadd( acc, val ) };
        b.br( header, next );

        b.switch_to_block( done );
        b.ret( b.block_param( done, 0 ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( pressure.ref ) };
        const auto    square = jit.declare_func_in_func( square_id, b );
        const auto    entry  = b.add_block( { Type::i64 } );
        const auto    header = b.add_block( { Type::i64, Type::i64 } );
        const auto    body   = b.add_block( { } );
        const auto    done   = b.add_block( { Type::i64 } );

        b.switch_to_block( entry );
        const auto                                x = b.param( 0 );
        std::array< ValueRef, k_pressure_values > a { };
        for ( int k = 0; k < k_pressure_values; ++k ) a[ k ] = b.iadd( b.imul( x, i64c( b, k + 3 ) ), i64c( b, k * k + 1 ) );
        const ValueRef start[] = { i64c( b, 0 ), i64c( b, 0 ) };
        b.br( header, start );

        b.switch_to_block( header );
        const auto     i      = b.block_param( header, 0 );
        const auto     acc    = b.block_param( header, 1 );
        const ValueRef result[] = { acc };
        b.brif( b.icmp( ir::ICmpCC::slt, i, i64c( b, 8 ) ), body, { }, done, result );

        b.switch_to_block( body );
        const ValueRef sq_args[] = { b.bxor( i, x ) };
        auto           t         = b.iadd( acc, b.call( square, sq_args ) );
        for ( int k = 0; k < k_pressure_values; ++k ) t = k % 2 == 0 ? b.iadd( t, a[ k ] ) : b.bxor( t, a[ k ] );
        const ValueRef next[] = { b.iadd( i, i64c( b, 1 ) ), t };
        b.br( header, next );

        b.switch_to_block( done );
        auto r = b.block_param( done, 0 );
        for ( int k = 0; k < k_pressure_values; ++k ) r = b.isub( r, a[ k ] );
        b.ret( r );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( consts.ref ) };
        b.switch_to_block( b.add_block( { Type::i64 } ) );
        const auto x  = b.param( 0 );
        const auto r  = b.bor( b.band( b.iadd( x, b.iconst( Type::i64, 0x123456789ABCDEF0ull ) ), b.iconst( Type::i64, 0xFFFF0000FFFF0000ull ) ),
                               i64c( b, 0x10000000000ll ) );
        const auto r2 = b.imul( x, i64c( b, -2147483648ll ) );
        const auto r3 = b.isub( x, i64c( b, 2147483647ll ) );
        b.ret( b.select( b.icmp( ir::ICmpCC::sgt, x, i64c( b, -5 ) ), b.bxor( r, r2 ), r3 ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( greet.ref ) };
        const auto    print    = jit.declare_func_in_func( print_id, b );
        const auto    greeting = jit.declare_data_in_func( greeting_id, b );
        const auto    counter  = jit.declare_data_in_func( counter_id, b );
        b.switch_to_block( b.add_block( { } ) );

        const auto p     = b.symbol_value( Type::ptr, counter );
        const auto count = b.iadd( b.load( Type::i64, p ), i64c( b, 1 ) );
        b.store( count, p );

        const ValueRef args[] = { b.symbol_value( Type::ptr, greeting ), i64c( b, static_cast< i64 >( k_greeting.size( ) ) ) };
        ( void ) b.call( print, args );
        b.ret( count );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( vowels.ref ) };
        const auto    table = jit.declare_data_in_func( vowels_id, b );
        const auto    entry = b.add_block( { Type::ptr, Type::i64 } );
        const auto    outer = b.add_block( { Type::i64, Type::i64 } );
        const auto    load  = b.add_block( { } );
        const auto    inner = b.add_block( { Type::i64, Type::i64 } );
        const auto    test  = b.add_block( { } );
        const auto    next  = b.add_block( { Type::i64 } );
        const auto    done  = b.add_block( { Type::i64 } );

        b.switch_to_block( entry );
        const auto     text    = b.param( 0 );
        const auto     len     = b.param( 1 );
        const auto     vtab    = b.symbol_value( Type::ptr, table );
        const ValueRef start[] = { i64c( b, 0 ), i64c( b, 0 ) };
        b.br( outer, start );

        b.switch_to_block( outer );
        const auto     i      = b.block_param( outer, 0 );
        const auto     count  = b.block_param( outer, 1 );
        const ValueRef total[] = { count };
        b.brif( b.icmp( ir::ICmpCC::slt, i, len ), load, { }, done, total );

        b.switch_to_block( load );
        const auto     c          = b.uextend( Type::i64, b.load( Type::i8, b.iadd( text, i ) ) );
        const ValueRef scan[]     = { i64c( b, 0 ), count };
        b.br( inner, scan );

        b.switch_to_block( inner );
        const auto     j       = b.block_param( inner, 0 );
        const auto     seen    = b.block_param( inner, 1 );
        const ValueRef carry[] = { seen };
        b.brif( b.icmp( ir::ICmpCC::slt, j, i64c( b, static_cast< i64 >( k_vowels.size( ) ) ) ), test, { }, next, carry );

        b.switch_to_block( test );
        const auto     v        = b.uextend( Type::i64, b.load( Type::i8, b.iadd( vtab, j ) ) );
        const auto     hit      = b.uextend( Type::i64, b.icmp( ir::ICmpCC::eq, c, v ) );
        const ValueRef again[]  = { b.iadd( j, i64c( b, 1 ) ), b.iadd( seen, hit ) };
        b.br( inner, again );

        b.switch_to_block( next );
        const ValueRef step[] = { b.iadd( i, i64c( b, 1 ) ), b.block_param( next, 0 ) };
        b.br( outer, step );

        b.switch_to_block( done );
        b.ret( b.block_param( done, 0 ) );
        b.finalise( );
    }

    {
        ir::IRBuilder b { *irm.get( message.ref ) };
        const auto    print    = jit.declare_func_in_func( print_id, b );
        const auto    messages = jit.declare_data_in_func( messages_id, b );
        b.switch_to_block( b.add_block( { Type::i64 } ) );

        const auto     slot   = b.iadd( b.symbol_value( Type::ptr, messages ), b.imul( b.param( 0 ), i64c( b, 16 ) ) );
        const auto     len    = b.load( Type::i64, slot, 8 );
        const ValueRef args[] = { b.load( Type::ptr, slot ), len };
        ( void ) b.call( print, args );
        b.ret( len );
        b.finalise( );
    }

    std::println( "~~ verifier ~~" );
    {
        ir::IRModule bad_module;
        const auto   bad = bad_module.declare( "bad", Type::i64, { Type::i64 } );
        {
            ir::IRBuilder b { *bad_module.get( bad ) };
            b.switch_to_block( b.add_block( { Type::i64 } ) );
            const auto narrow32 = b.iconst( Type::i32, 7 );
            const auto mixed    = b.iadd( b.param( 0 ), narrow32 );
            ( void ) b.udiv( narrow32, narrow32 );
            b.ret( mixed );
            b.finalise( );
        }
        ir::IRVerifier v;
        v.visit_module( bad_module );
        for ( const auto& [ msg ] : v.errs ) std::println( "  rejected: {}", msg );
        check( "verifier rejects the broken function", static_cast< i64 >( v.errs.size( ) ), 2 );
    }

    ir::IRVerifier verifier;
    verifier.visit_module( irm );
    if ( !verifier.ok( ) )
    {
        for ( const auto& [ msg ] : verifier.errs ) std::println( "[error] {}", msg );
        return 1;
    }

    if ( show_ir )
    {
        std::string text;
        irm.fmt( std::back_inserter( text ) );
        std::println( "~~ ir ~~\n{}", text );
    }

    std::vector< codegen::Context > contexts;
    contexts.reserve( fns.size( ) );
    for ( const auto& f : fns )
    {
        contexts.emplace_back( *irm.get( f.ref ) );
        if ( !jit.define_function( f.id, contexts.back( ) ) )
        {
            std::println( "[error] failed to compile {}", f.name );
            return 1;
        }
    }
    if ( !jit.finalise_definitions( ) )
    {
        std::println( "[error] failed to finalise" );
        return 1;
    }

    std::println( "~~ compiled ~~" );
    for ( std::size_t k = 0; k < fns.size( ); ++k )
    {
        const auto& code = *contexts[ k ].compiled( );
        std::println( "  {:<10} {:>4} bytes  {} relocs", fns[ k ].name, code.code.size( ), code.relocs.size( ) );
    }

#ifdef XIAO_HAS_ZYDIS
    if ( show_asm )
        for ( std::size_t k = 0; k < fns.size( ); ++k ) std::println( "\n~~ x86-64: {} ~~\n{}", fns[ k ].name, disasm::dump( *contexts[ k ].compiled( ) ) );
#endif

    const auto get = [ & ]< typename F >( const Fn& f ) { return jit.get_finalised_function< F >( f.id ); };

    std::println( "\n~~ crunch: loop, memory, calls, select, division ~~" );
    {
        std::array< Item, 6 > items { {
            {      7,        -12,   -300, 1, 0 },
            {    -15,         40,   1234, 0, 0 },
            { 123456,         -7,     -1, 1, 0 },
            {     -2,          5,  32767, 1, 0 },
            {      0,          0, -32768, 0, 0 },
            {  99999, 2147483647,      5, 1, 0 },
        } };
        auto       expected = items;
        const auto fn       = get.operator( )< int64_t( Item*, int64_t ) >( crunch );
        const int  calls    = g_sqr_calls;
        check( "crunch(items, 6)", fn( items.data( ), 6 ), crunch_ref( expected.data( ), 6 ) );
        check( "items written back like the reference", std::memcmp( items.data( ), expected.data( ), sizeof( items ) ), 0 );
        check( "sqr_func calls from crunch", g_sqr_calls - calls, 6 );
    }

    std::println( "\n~~ compare: every icmp condition ~~" );
    {
        const auto fn = get.operator( )< int64_t( int64_t, int64_t ) >( compare );
        for ( const auto [ a, b ] : std::initializer_list< std::pair< i64, i64 > > { { 1, 2 }, { 2, 1 }, { 5, 5 }, { -1, 1 }, { 1, -1 }, { INT64_MIN, INT64_MAX } } )
            check( std::format( "compare({}, {})", a, b ), fn( a, b ), compare_ref( a, b ) );
    }

    std::println( "\n~~ shifts: variable counts through cl ~~" );
    {
        const auto fn = get.operator( )< int64_t( int64_t, int64_t ) >( shifts );
        for ( const auto [ x, n ] : std::initializer_list< std::pair< i64, i64 > > { { 1, 0 }, { -1, 63 }, { 0x123456789, 12 }, { -12345, 70 }, { INT64_MIN, 1 } } )
            check( std::format( "shifts({}, {})", x, n ), fn( x, n ), shifts_ref( x, n ) );
    }

    std::println( "\n~~ divmod: signed and unsigned division by a register ~~" );
    {
        const auto fn = get.operator( )< int64_t( int64_t, int64_t ) >( divmod );
        for ( const auto [ a, b ] : std::initializer_list< std::pair< i64, i64 > > { { 100, 7 }, { -100, 7 }, { 100, -7 }, { -1, 3 }, { INT64_MAX, 2 }, { 12345, 12345 } } )
            check( std::format( "divmod({}, {})", a, b ), fn( a, b ), divmod_ref( a, b ) );
    }

    std::println( "\n~~ narrow: 8, 16, 32 and 64-bit loads, stores and extensions ~~" );
    {
        const auto fn = get.operator( )< int64_t( const Wide*, Wide* ) >( narrow );
        for ( const Wide src : { Wide { 1, 2, 3, 4, 5 }, Wide { -1, -2, -3, -4, 250 }, Wide { INT64_MAX, INT32_MIN, INT16_MAX, INT8_MIN, 128 } } )
        {
            Wide out { }, want { };
            check( std::format( "narrow({{ {}, {}, {}, {}, {} }})", src.q, src.d, src.w, src.b, src.u ), fn( &src, &out ), narrow_ref( src, want ) );
            check( "  narrow output struct", std::memcmp( &out, &want, sizeof( Wide ) ), 0 );
        }
    }

    std::println( "\n~~ fact: recursion through an exported function ~~" );
    {
        const auto fn = get.operator( )< int64_t( int64_t ) >( fact );
        for ( const i64 n : { 0, 1, 5, 10, 20, 25 } ) check( std::format( "fact({})", n ), fn( n ), fact_ref( n ) );
    }

    std::println( "\n~~ gcd: loop with block arguments on both edges ~~" );
    {
        const auto fn = get.operator( )< int64_t( int64_t, int64_t ) >( gcd );
        for ( const auto [ a, b ] : std::initializer_list< std::pair< i64, i64 > > { { 48, 18 }, { 17, 5 }, { 0, 9 }, { 9, 0 }, { 1071, 462 }, { -2, 6 } } )
            check( std::format( "gcd({}, {})", a, b ), fn( a, b ), gcd_ref( bits( a ), bits( b ) ) );
    }

    std::println( "\n~~ classify: critical edge split by lowering ~~" );
    {
        const auto fn = get.operator( )< int64_t( int64_t, int64_t* ) >( classify );
        for ( const i64 x : { 0, 4, -9, 1000000 } )
        {
            i64 slot = 100, want_slot = 100;
            check( std::format( "classify({})", x ), fn( x, &slot ), classify_ref( x, &want_slot ) );
            check( "  classify memory", slot, want_slot );
        }
    }

    std::println( "\n~~ hostcalls: four register arguments, registered and runtime symbols ~~" );
    {
        const auto fn = get.operator( )< int64_t( int64_t, int64_t, int64_t, int64_t ) >( hostcalls );
        for ( const auto& [ a, b, c, d ] : std::initializer_list< std::array< i64, 4 > > { { 1, 2, 3, 4 }, { -5, 6, -7, 8 }, { 0, 0, 0, -1 }, { 123456789, -2, 3, -987654321 } } )
            check( std::format( "hostcalls({}, {}, {}, {})", a, b, c, d ), fn( a, b, c, d ), hostcalls_ref( a, b, c, d ) );
    }

    std::println( "\n~~ fill / fill_sum: unit return, called from C++ and from jit code ~~" );
    {
        std::array< i64, 16 > buf { };
        const auto            fill_fn = get.operator( )< void( int64_t*, int64_t, int64_t ) >( fill );
        fill_fn( buf.data( ), 16, 3 );
        bool same = true;
        for ( i64 i = 0; i < 16; ++i ) same = same && buf[ i ] == 3 * i + i;
        check( "fill(buf, 16, 3) matches", same, 1 );

        const auto sum_fn = get.operator( )< int64_t( int64_t*, int64_t ) >( fill_sum );
        for ( const i64 n : { 0, 1, 16 } ) check( std::format( "fill_sum(buf, {})", n ), sum_fn( buf.data( ), n ), fill_sum_ref( n ) );
    }

    std::println( "\n~~ pressure: {} values live across calls (spilling, eviction, splitting) ~~", k_pressure_values );
    {
        const auto fn = get.operator( )< int64_t( int64_t ) >( pressure );
        for ( const i64 x : { 0, 1, -7, 123456789 } ) check( std::format( "pressure({})", x ), fn( x ), pressure_ref( x ) );
    }

    std::println( "\n~~ consts: 64-bit and 32-bit boundary immediates ~~" );
    {
        const auto fn = get.operator( )< int64_t( int64_t ) >( consts );
        for ( const i64 x : std::initializer_list< i64 > { 0, -5, -6, 42, INT64_MAX, INT64_MIN } ) check( std::format( "consts({})", x ), fn( x ), consts_ref( x ) );
    }

    std::println( "\n~~ strings: data objects, symbol_value, data relocations ~~" );
    {
        const auto greet_fn = get.operator( )< int64_t( ) >( greet );
        check( "greet() first call", greet_fn( ), 1 );
        check( "greet() second call", greet_fn( ), 2 );
        check( "host_print got the greeting twice", g_printed == std::string { k_greeting } + std::string { k_greeting }, 1 );

        int64_t stored { };
        std::memcpy( &stored, jit.get_finalised_data( counter_id ).data( ), sizeof stored );
        check( "greet_count in writable data", stored, 2 );

        const auto count_fn  = get.operator( )< int64_t( const char*, int64_t ) >( vowels );
        const auto vowel_ref = [ ]( const std::string_view s ) { return static_cast< i64 >( std::ranges::count_if( s, [ ]( const char c ) { return k_vowels.find( c ) != std::string_view::npos; } ) ); };
        for ( const std::string_view s : { std::string_view { "" }, std::string_view { "xyz" }, std::string_view { "Audio Equipment" }, k_texts } )
            check( std::format( "count_vowels(\"{}\")", s.substr( 0, 18 ) ), count_fn( s.data( ), static_cast< i64 >( s.size( ) ) ), vowel_ref( s ) );

        const auto greeting = jit.get_finalised_data( greeting_id );
        check( "count_vowels(greeting data)", count_fn( reinterpret_cast< const char* >( greeting.data( ) ), static_cast< i64 >( greeting.size( ) ) ),
               vowel_ref( k_greeting ) );

        g_printed.clear( );
        const auto message_fn = get.operator( )< int64_t( int64_t ) >( message );
        check( "print_message(0) length", message_fn( 0 ), static_cast< i64 >( k_split ) );
        check( "print_message(1) length", message_fn( 1 ), static_cast< i64 >( k_texts.size( ) - k_split ) );
        check( "messages printed through the pointer table", g_printed == k_texts, 1 );
    }

    std::println( "\n~~ result ~~\n  {} checks, {} failed", g_checks, g_fails );
    return g_fails == 0 ? 0 : 1;
}
