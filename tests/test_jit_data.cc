#include "support.hh"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

using namespace xt;

namespace
{
    int64_t    g_host_value      = 1234;
    const char g_host_text[]     = "host text";
    int32_t    g_host_array[ 4 ] = { 5, 6, 7, 8 };

    int64_t h_negate( const int64_t x )
    { return -x; }

    jit::JITBuilder data_builder( )
    {
        jit::JITBuilder b { isa::lookup_host( ) };
        b.symbol( "g_host_value", &g_host_value );
        b.symbol( "g_host_text", g_host_text );
        b.symbol( "g_host_array", g_host_array );
        b.symbol( "h_negate", reinterpret_cast< const void* >( &h_negate ) );
        return b;
    }

    module::DataId define_bytes( Jit& j, const std::string_view name, const std::vector< uint8_t >& bytes, const bool writable = false,
                                 const module::Linkage linkage = module::Linkage::Local, const uint64_t align = 0 )
    {
        const auto id = j.mod.declare_data( name, linkage, writable );
        REQUIRE( id.has_value( ) );
        module::DataDescription d;
        d.define( bytes );
        if ( align ) d.set_align( align );
        REQUIRE( j.mod.define_data( *id, d ).has_value( ) );
        return *id;
    }

    module::DataId define_zeros( Jit& j, const std::string_view name, const std::size_t size, const bool writable,
                                 const uint64_t align = 0 )
    {
        const auto id = j.mod.declare_data( name, module::Linkage::Local, writable );
        REQUIRE( id.has_value( ) );
        module::DataDescription d;
        d.define_zeroinit( size );
        if ( align ) d.set_align( align );
        REQUIRE( j.mod.define_data( *id, d ).has_value( ) );
        return *id;
    }

    std::vector< uint8_t > bytes_of( const std::string_view s )
    { return { s.begin( ), s.end( ) }; }

    template < typename T >
    std::vector< uint8_t > bytes_of_values( const std::vector< T >& vs )
    {
        std::vector< uint8_t > out( vs.size( ) * sizeof( T ) );
        std::memcpy( out.data( ), vs.data( ), out.size( ) );
        return out;
    }

    ir::GlobalValueRef gv_at( ir::IRBuilder& b, const module::DataId id, const int64_t offset, const bool colocated )
    {
        return b.create_global_value( ir::GlobalValueData {
            .name      = ir::ExternalName { ir::extns::data, id.id },
            .offset    = offset,
            .colocated = colocated
        } );
    }
}

TEST_CASE( "read only string is readable byte by byte from code", "[jit][data]" )
{
    Jit                        j;
    constexpr std::string_view text = "hello, world";
    const auto                 msg  = define_bytes( j, "msg", bytes_of( text ) );
    const auto                 id   = j.define( "f", Type::i64, { Type::i64 },
                                                [ & ]( ir::IRBuilder& b, const Args& a )
                                                {
                                  const auto p = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( msg, b ) );
                                  b.ret( b.uextend( Type::i64, b.load( Type::i8, b.iadd( p, a[ 0 ] ) ) ) );
                                                } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( id );
    for ( std::size_t k = 0; k < text.size( ); ++k ) CHECK( f( static_cast< int64_t >( k ) ) == static_cast< unsigned char >( text[ k ] ) );

    const auto blob = j.mod.get_finalised_data( msg );
    REQUIRE( blob.size( ) == text.size( ) );
    CHECK( std::memcmp( blob.data( ), text.data( ), text.size( ) ) == 0 );
}

TEST_CASE( "read only data supports every load width and offset", "[jit][data]" )
{
    Jit                    j;
    std::vector< uint8_t > raw;
    for ( int k = 0; k < 32; ++k ) raw.push_back( static_cast< uint8_t >( 0x81 + k * 7 ) );
    const auto d  = define_bytes( j, "raw", raw );
    const auto mk = [ & ]( const std::string_view name, const Type ty, const int32_t off )
    {
        return j.define( name, Type::i64, { },
                         [ & ]( ir::IRBuilder& b, const Args& )
                         {
                             const auto p = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( d, b ) );
                             const auto v = b.load( ty, p, off );
                             b.ret( ty == Type::i64 ? v : b.uextend( Type::i64, v ) );
                         } );
    };
    const auto l8  = mk( "l8", Type::i8, 3 );
    const auto l16 = mk( "l16", Type::i16, 6 );
    const auto l32 = mk( "l32", Type::i32, 12 );
    const auto l64 = mk( "l64", Type::i64, 24 );
    const auto l0  = mk( "l0", Type::i64, 0 );
    j.finalise( );

    const auto read = [ & ]( const std::size_t off, const std::size_t n )
    {
        uint64_t v = 0;
        std::memcpy( &v, raw.data( ) + off, n );
        return static_cast< int64_t >( v );
    };
    CHECK( j.get< int64_t( ) >( l8 )( ) == read( 3, 1 ) );
    CHECK( j.get< int64_t( ) >( l16 )( ) == read( 6, 2 ) );
    CHECK( j.get< int64_t( ) >( l32 )( ) == read( 12, 4 ) );
    CHECK( j.get< int64_t( ) >( l64 )( ) == read( 24, 8 ) );
    CHECK( j.get< int64_t( ) >( l0 )( ) == read( 0, 8 ) );
}

TEST_CASE( "writable counter is shared between code and host", "[jit][data]" )
{
    Jit        j;
    const auto counter = define_bytes( j, "counter", bytes_of_values< int64_t >( { 5 } ), true );
    const auto inc     = j.define( "inc", Type::i64, { },
                                   [ & ]( ir::IRBuilder& b, const Args& )
                                   {
                                   const auto p = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( counter, b ) );
                                   const auto v = b.iadd( b.load( Type::i64, p ), b.iconst( Type::i64, 1 ) );
                                   b.store( v, p );
                                   b.ret( v );
                                   } );
    j.finalise( );
    const auto f    = j.get< int64_t( ) >( inc );
    const auto blob = j.mod.get_finalised_data( counter );
    REQUIRE( blob.size( ) == 8 );

    CHECK( f( ) == 6 );
    CHECK( f( ) == 7 );
    CHECK( f( ) == 8 );
    int64_t seen = 0;
    std::memcpy( &seen, blob.data( ), 8 );
    CHECK( seen == 8 );

    const int64_t host = 100;
    std::memcpy( blob.data( ), &host, 8 );
    CHECK( f( ) == 101 );
    std::memcpy( &seen, blob.data( ), 8 );
    CHECK( seen == 101 );
}

TEST_CASE( "writable zero initialised data starts zero and accepts stores", "[jit][data]" )
{
    Jit        j;
    const auto buf = define_zeros( j, "buf", 64, true );
    const auto put = j.define( "put", Type::unit, { Type::i64, Type::i64 },
                               [ & ]( ir::IRBuilder& b, const Args& a )
                               {
                                   const auto p = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( buf, b ) );
                                   b.store( a[ 1 ], b.iadd( p, b.ishl( a[ 0 ], b.iconst( Type::i64, 3 ) ) ) );
                                   b.ret( );
                               } );
    j.finalise( );
    const auto blob = j.mod.get_finalised_data( buf );
    REQUIRE( blob.size( ) == 64 );
    for ( const auto byte : blob ) CHECK( byte == std::byte { 0 } );

    const auto f = j.get< void( int64_t, int64_t ) >( put );
    for ( int64_t k = 0; k < 8; ++k ) f( k, k * k - 3 );
    for ( int64_t k = 0; k < 8; ++k )
    {
        int64_t v = 0;
        std::memcpy( &v, blob.data( ) + k * 8, 8 );
        CHECK( v == k * k - 3 );
    }
}

TEST_CASE( "read only zero initialised data reads as zero", "[jit][data]" )
{
    Jit        j;
    const auto z  = define_zeros( j, "z", 24, false );
    const auto id = j.define( "f", Type::i64, { },
                              [ & ]( ir::IRBuilder& b, const Args& )
                              {
                                  const auto p = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( z, b ) );
                                  b.ret( b.bor( b.bor( b.load( Type::i64, p ), b.load( Type::i64, p, 8 ) ), b.load( Type::i64, p, 16 ) ) );
                              } );
    j.finalise( );
    CHECK( j.get< int64_t( ) >( id )( ) == 0 );
    const auto blob = j.mod.get_finalised_data( z );
    CHECK( blob.size( ) == 24 );
    for ( const auto byte : blob ) CHECK( byte == std::byte { 0 } );
}

TEST_CASE( "zero sized data objects can be defined", "[jit][data]" )
{
    Jit        j;
    const auto e1 = define_zeros( j, "e1", 0, false );
    const auto e2 = define_bytes( j, "e2", { }, true );
    j.finalise( );
    CHECK( j.mod.get_finalised_data( e1 ).size( ) == 0 );
    CHECK( j.mod.get_finalised_data( e2 ).size( ) == 0 );
}

TEST_CASE( "custom data alignment is honoured", "[jit][data]" )
{
    for ( const uint64_t align : { 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 1024u, 4096u } )
        for ( const bool writable : { false, true } )
        {
            DYNAMIC_SECTION( "align " << align << ( writable ? " writable" : " read only" ) )
            {
                Jit        j;
                const auto pad = define_bytes( j, "pad", { 1, 2, 3 }, writable, module::Linkage::Local, 1 );
                const auto x   = define_bytes( j, "x", { 9, 9, 9, 9 }, writable, module::Linkage::Local, align );
                const auto y   = define_zeros( j, "y", 3, writable, align );
                j.finalise( );
                CHECK( j.mod.get_finalised_data( pad ).size( ) == 3 );
                CHECK( reinterpret_cast< std::uintptr_t >( j.mod.get_finalised_data( x ).data( ) ) % align == 0 );
                CHECK( reinterpret_cast< std::uintptr_t >( j.mod.get_finalised_data( y ).data( ) ) % align == 0 );
            }
        }
}

TEST_CASE( "default writable data alignment is eight bytes", "[jit][data]" )
{
    Jit        j;
    const auto a = define_bytes( j, "a", { 1 }, true );
    const auto b = define_bytes( j, "b", { 2, 3 }, true );
    const auto c = define_zeros( j, "c", 5, true );
    j.finalise( );
    CHECK( reinterpret_cast< std::uintptr_t >( j.mod.get_finalised_data( a ).data( ) ) % jit::JITModule::writable_data_alignment == 0 );
    CHECK( reinterpret_cast< std::uintptr_t >( j.mod.get_finalised_data( b ).data( ) ) % 8 == 0 );
    CHECK( reinterpret_cast< std::uintptr_t >( j.mod.get_finalised_data( c ).data( ) ) % 8 == 0 );
}

TEST_CASE( "default read only data alignment packs bytes tightly", "[jit][data]" )
{
    Jit        j;
    const auto a = define_bytes( j, "a", { 1 } );
    const auto b = define_bytes( j, "b", { 2, 3 } );
    j.finalise( );
    CHECK( jit::JITModule::readonly_data_alignment == 1 );
    CHECK( j.mod.get_finalised_data( b ).data( ) == j.mod.get_finalised_data( a ).data( ) + 1 );
}

TEST_CASE( "global value offsets apply for colocated and absolute references", "[jit][data]" )
{
    const std::vector< int32_t > values { 10, 20, 30, 40, 50, 60, 70, 80 };
    for ( const bool colocated : { true, false } )
    {
        DYNAMIC_SECTION( ( colocated ? "colocated" : "absolute" ) )
        {
            Jit        j;
            const auto arr = define_bytes( j, "arr", bytes_of_values( values ) );
            const auto mk  = [ & ]( const std::string_view name, const int64_t gv_off, const int32_t load_off )
            {
                return j.define( name, Type::i64, { },
                                 [ & ]( ir::IRBuilder& b, const Args& )
                                 {
                                     const auto p = b.symbol_value( Type::ptr, gv_at( b, arr, gv_off, colocated ) );
                                     b.ret( b.uextend( Type::i64, b.load( Type::i32, p, load_off ) ) );
                                 } );
            };
            const auto plus12  = mk( "plus12", 12, 0 );
            const auto plus28  = mk( "plus28", 28, 0 );
            const auto minus4  = mk( "minus4", -4, 8 );
            const auto minus64 = mk( "minus64", -64, 80 );
            const auto addr    = j.define( "addr", Type::ptr, { },
                                           [ & ]( ir::IRBuilder& b, const Args& )
                                           {
                                            b.ret( b.symbol_value( Type::ptr, gv_at( b, arr, 20, colocated ) ) );
                                           } );
            j.finalise( );
            CHECK( j.get< int64_t( ) >( plus12 )( ) == 40 );
            CHECK( j.get< int64_t( ) >( plus28 )( ) == 80 );
            CHECK( j.get< int64_t( ) >( minus4 )( ) == 20 );
            CHECK( j.get< int64_t( ) >( minus64 )( ) == 50 );
            CHECK( j.get< std::byte*( ) >( addr )( ) == j.mod.get_finalised_data( arr ).data( ) + 20 );
        }
    }
}

TEST_CASE( "local data is referenced pc relative and imports absolutely", "[jit][data]" )
{
    Jit        j { data_builder( ) };
    const auto local = define_bytes( j, "local", { 1, 2, 3, 4 } );
    const auto host  = *j.mod.declare_data( "g_host_value", module::Linkage::Import, true );
    j.define( "f", Type::i64, { },
              [ & ]( ir::IRBuilder& b, const Args& )
              {
                  const auto a = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( local, b ) );
                  const auto c = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( host, b ) );
                  b.ret( b.iadd( b.load( Type::i64, c ), b.uextend( Type::i64, b.load( Type::i8, a ) ) ) );
              } );
    const auto& relocs = j.last_code.relocs;
    REQUIRE( relocs.size( ) == 2 );
    bool saw_pcrel = false, saw_abs = false;
    for ( const auto& r : relocs )
    {
        CHECK( r.target.ns == ir::extns::data );
        if ( r.kind == codegen::RelocKind::X86PCRel4 )
        {
            saw_pcrel = true;
            CHECK( r.target.index == local.id );
            CHECK( r.addend == -4 );
        }
        if ( r.kind == codegen::RelocKind::Abs8 )
        {
            saw_abs = true;
            CHECK( r.target.index == host.id );
            CHECK( r.addend == 0 );
        }
    }
    CHECK( saw_pcrel );
    CHECK( saw_abs );
}

TEST_CASE( "imported host data is read and written through an absolute address", "[jit][data]" )
{
    Jit        j { data_builder( ) };
    const auto host = *j.mod.declare_data( "g_host_value", module::Linkage::Import, true );
    const auto id   = j.define( "bump", Type::i64, { Type::i64 },
                                [ & ]( ir::IRBuilder& b, const Args& a )
                                {
                                  const auto p = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( host, b ) );
                                  const auto v = b.iadd( b.load( Type::i64, p ), a[ 0 ] );
                                  b.store( v, p );
                                  b.ret( v );
                                } );
    j.finalise( );
    g_host_value = 1234;
    const auto f = j.get< int64_t( int64_t ) >( id );
    CHECK( f( 1 ) == 1235 );
    CHECK( g_host_value == 1235 );
    g_host_value = -10;
    CHECK( f( 10 ) == 0 );
    CHECK( g_host_value == 0 );
}

TEST_CASE( "imported host arrays and strings are readable", "[jit][data]" )
{
    Jit        j { data_builder( ) };
    const auto text = *j.mod.declare_data( "g_host_text", module::Linkage::Import, false );
    const auto arr  = *j.mod.declare_data( "g_host_array", module::Linkage::Import, true );
    const auto ch   = j.define( "ch", Type::i64, { Type::i64 },
                                [ & ]( ir::IRBuilder& b, const Args& a )
                                {
                                  const auto p = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( text, b ) );
                                  b.ret( b.uextend( Type::i64, b.load( Type::i8, b.iadd( p, a[ 0 ] ) ) ) );
                                } );
    const auto el
        = j.define( "el", Type::i64, { Type::i64 },
                    [ & ]( ir::IRBuilder& b, const Args& a )
                    {
                        const auto p = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( arr, b ) );
                        b.ret( b.sextend( Type::i64, b.load( Type::i32, b.iadd( p, b.ishl( a[ 0 ], b.iconst( Type::i64, 2 ) ) ) ) ) );
                    } );
    j.finalise( );
    for ( std::size_t k = 0; k < sizeof g_host_text; ++k )
        CHECK( j.get< int64_t( int64_t ) >( ch )( static_cast< int64_t >( k ) ) == static_cast< unsigned char >( g_host_text[ k ] ) );
    g_host_array[ 2 ] = -77;
    CHECK( j.get< int64_t( int64_t ) >( el )( 0 ) == 5 );
    CHECK( j.get< int64_t( int64_t ) >( el )( 2 ) == -77 );
    CHECK( j.get< int64_t( int64_t ) >( el )( 3 ) == 8 );
}

TEST_CASE( "data table of function pointers is filled at finalisation", "[jit][data]" )
{
    Jit        j { data_builder( ) };
    const auto sq = j.define(
        "sq", Type::i64, { Type::i64 },
        [ & ]( ir::IRBuilder& b, const Args& a )
        {
            b.ret( b.imul( a[ 0 ], a[ 0 ] ) );
        },
        module::Linkage::Local );
    const auto neg = j.import( "h_negate", Type::i64, { Type::i64 } );
    const auto tbl = *j.mod.declare_data( "tbl", module::Linkage::Local, false );
    {
        module::DataDescription d;
        d.define_zeroinit( 24 );
        const auto r0 = j.mod.declare_func_in_data( sq, d );
        const auto r1 = j.mod.declare_func_in_data( neg, d );
        d.write_function_addr( 0, r0 );
        d.write_function_addr( 8, r1 );
        d.write_function_addr( 16, r0 );
        REQUIRE( j.mod.define_data( tbl, d ).has_value( ) );
    }
    const auto call = j.define( "call", Type::i64, { Type::i64, Type::i64 },
                                [ & ]( ir::IRBuilder& b, const Args& a )
                                {
                                    const auto sig  = b.import_signature( { Type::i64 }, Type::i64, codegen::host_call_conv( ) );
                                    const auto base = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( tbl, b ) );
                                    const auto fp   = b.load( Type::ptr, b.iadd( base, b.imul( a[ 0 ], b.iconst( Type::i64, 8 ) ) ) );
                                    b.ret( b.call_indirect( sig, fp, Args { a[ 1 ] } ) );
                                } );
    j.finalise( );

    const auto f = j.get< int64_t( int64_t, int64_t ) >( call );
    for ( const int64_t x : { 0, 3, -9, 1000 } )
    {
        CHECK( f( 0, x ) == x * x );
        CHECK( f( 1, x ) == -x );
        CHECK( f( 2, x ) == x * x );
    }

    const auto blob = j.mod.get_finalised_data( tbl );
    void*      slots[ 3 ] { };
    std::memcpy( slots, blob.data( ), sizeof slots );
    CHECK( slots[ 0 ] == reinterpret_cast< void* >( j.get< int64_t( int64_t ) >( sq ) ) );
    CHECK( slots[ 1 ] == reinterpret_cast< void* >( &h_negate ) );
    CHECK( slots[ 2 ] == slots[ 0 ] );
    using Fn = int64_t ( * )( int64_t );
    CHECK( reinterpret_cast< Fn >( slots[ 1 ] )( 5 ) == -5 );
}

TEST_CASE( "data pointers with addends are filled at finalisation", "[jit][data]" )
{
    Jit                        j;
    constexpr std::string_view text = "abcdefgh";
    const auto                 msg  = define_bytes( j, "msg", bytes_of( text ) );
    const auto                 ptrs = *j.mod.declare_data( "ptrs", module::Linkage::Local, true );
    {
        module::DataDescription d;
        d.define_zeroinit( 32 );
        const auto m    = j.mod.declare_data_in_data( msg, d );
        const auto self = j.mod.declare_data_in_data( ptrs, d );
        d.write_data_addr( 0, m, 0 );
        d.write_data_addr( 8, m, 3 );
        d.write_data_addr( 16, m, 7 );
        d.write_data_addr( 24, self, 8 );
        REQUIRE( j.mod.define_data( ptrs, d ).has_value( ) );
    }
    const auto deref = j.define( "deref", Type::i64, { Type::i64 },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto base = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( ptrs, b ) );
                                     const auto p    = b.load( Type::ptr, b.iadd( base, b.ishl( a[ 0 ], b.iconst( Type::i64, 3 ) ) ) );
                                     b.ret( b.uextend( Type::i64, b.load( Type::i8, p ) ) );
                                 } );
    j.finalise( );

    const auto       blob  = j.mod.get_finalised_data( ptrs );
    const auto*      msg_p = j.mod.get_finalised_data( msg ).data( );
    const std::byte* slots[ 4 ] { };
    std::memcpy( slots, blob.data( ), sizeof slots );
    CHECK( slots[ 0 ] == msg_p );
    CHECK( slots[ 1 ] == msg_p + 3 );
    CHECK( slots[ 2 ] == msg_p + 7 );
    CHECK( slots[ 3 ] == blob.data( ) + 8 );

    const auto f = j.get< int64_t( int64_t ) >( deref );
    CHECK( f( 0 ) == 'a' );
    CHECK( f( 1 ) == 'd' );
    CHECK( f( 2 ) == 'h' );
}

TEST_CASE( "data may point at data defined after it", "[jit][data]" )
{
    Jit        j;
    const auto first  = *j.mod.declare_data( "first", module::Linkage::Local, false );
    const auto second = *j.mod.declare_data( "second", module::Linkage::Local, false );
    {
        module::DataDescription d;
        d.define( std::vector< uint8_t >( 8, 0xAA ) );
        d.write_data_addr( 0, j.mod.declare_data_in_data( second, d ), -2 );
        REQUIRE( j.mod.define_data( first, d ).has_value( ) );
    }
    {
        module::DataDescription d;
        d.define( bytes_of( "xyz" ) );
        REQUIRE( j.mod.define_data( second, d ).has_value( ) );
    }
    j.finalise( );
    const std::byte* p { };
    std::memcpy( &p, j.mod.get_finalised_data( first ).data( ), 8 );
    CHECK( p == j.mod.get_finalised_data( second ).data( ) - 2 );
}

TEST_CASE( "data pointing at imported host data and functions", "[jit][data]" )
{
    Jit        j { data_builder( ) };
    const auto neg  = j.import( "h_negate", Type::i64, { Type::i64 } );
    const auto host = *j.mod.declare_data( "g_host_array", module::Linkage::Import, true );
    const auto tbl  = *j.mod.declare_data( "tbl", module::Linkage::Local, true );
    {
        module::DataDescription d;
        d.define_zeroinit( 16 );
        d.write_function_addr( 0, j.mod.declare_func_in_data( neg, d ) );
        d.write_data_addr( 8, j.mod.declare_data_in_data( host, d ), 4 );
        REQUIRE( j.mod.define_data( tbl, d ).has_value( ) );
    }
    j.finalise( );
    const void* slots[ 2 ] { };
    std::memcpy( slots, j.mod.get_finalised_data( tbl ).data( ), sizeof slots );
    CHECK( slots[ 0 ] == reinterpret_cast< const void* >( &h_negate ) );
    CHECK( slots[ 1 ] == reinterpret_cast< const void* >( &g_host_array[ 1 ] ) );
}

TEST_CASE( "addresses of two data objects differ as on the host", "[jit][data]" )
{
    Jit        j;
    const auto a  = define_bytes( j, "a", { 1, 2, 3, 4, 5 } );
    const auto c  = define_zeros( j, "c", 100, true );
    const auto id = j.define( "diff", Type::i64, { },
                              [ & ]( ir::IRBuilder& b, const Args& )
                              {
                                  const auto pa = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( a, b ) );
                                  const auto pc = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( c, b ) );
                                  b.ret( b.isub( pc, pa ) );
                              } );
    j.finalise( );
    CHECK( j.get< int64_t( ) >( id )( ) == j.mod.get_finalised_data( c ).data( ) - j.mod.get_finalised_data( a ).data( ) );
}

TEST_CASE( "many data objects are all reachable from one function", "[jit][data]" )
{
    constexpr int                 count = 64;
    Jit                           j;
    std::vector< module::DataId > ids;
    for ( int k = 0; k < count; ++k )
        ids.push_back( define_bytes( j, std::format( "d{}", k ), bytes_of_values< int64_t >( { k * 3 + 1 } ), k % 2 == 0 ) );
    const auto sum
        = j.define( "sum", Type::i64, { },
                    [ & ]( ir::IRBuilder& b, const Args& )
                    {
                        auto acc = b.iconst( Type::i64, 0 );
                        for ( const auto id : ids )
                            acc = b.iadd( acc, b.load( Type::i64, b.symbol_value( Type::ptr, j.mod.declare_data_in_func( id, b ) ) ) );
                        b.ret( acc );
                    } );
    j.finalise( );
    int64_t want = 0;
    for ( int k = 0; k < count; ++k ) want += k * 3 + 1;
    CHECK( j.get< int64_t( ) >( sum )( ) == want );
}

TEST_CASE( "data description starts undefined", "[jit][data][module]" )
{
    module::DataDescription d;
    CHECK( d.size( ) == 0 );
    CHECK( std::holds_alternative< std::monostate >( d.init( ) ) );
    CHECK_FALSE( d.align( ).has_value( ) );
    CHECK( d.function_decls( ).empty( ) );
    CHECK( d.data_decls( ).empty( ) );
    CHECK( d.function_relocs( ).empty( ) );
    CHECK( d.data_relocs( ).empty( ) );
}

TEST_CASE( "data description holds bytes", "[jit][data][module]" )
{
    module::DataDescription d;
    const uint8_t           raw[] = { 1, 2, 3, 250 };
    d.define( raw );
    REQUIRE( std::holds_alternative< std::vector< uint8_t > >( d.init( ) ) );
    CHECK( std::get< std::vector< uint8_t > >( d.init( ) ) == std::vector< uint8_t > { 1, 2, 3, 250 } );
    CHECK( d.size( ) == 4 );
}

TEST_CASE( "data description holds a string without a terminator", "[jit][data][module]" )
{
    module::DataDescription d;
    d.define( std::string_view { "abc" } );
    CHECK( d.size( ) == 3 );
    CHECK( std::get< std::vector< uint8_t > >( d.init( ) ) == std::vector< uint8_t > { 'a', 'b', 'c' } );

    module::DataDescription e;
    e.define( std::string_view { "ab\0c", 4 } );
    CHECK( e.size( ) == 4 );
}

TEST_CASE( "data description holds zero fill", "[jit][data][module]" )
{
    module::DataDescription d;
    d.define_zeroinit( 4096 );
    REQUIRE( std::holds_alternative< module::DataDescription::Zeros >( d.init( ) ) );
    CHECK( std::get< module::DataDescription::Zeros >( d.init( ) ).size == 4096 );
    CHECK( d.size( ) == 4096 );
}

TEST_CASE( "data description records alignment", "[jit][data][module]" )
{
    for ( const uint64_t a : { 1u, 2u, 8u, 64u, 65536u } )
    {
        module::DataDescription d;
        d.set_align( a );
        CHECK( d.align( ) == a );
    }
}

TEST_CASE( "data description numbers imported names in order", "[jit][data][module]" )
{
    module::DataDescription d;
    d.define_zeroinit( 64 );
    const auto f0 = d.import_function( { ir::extns::function, 7 } );
    const auto f1 = d.import_function( { ir::extns::function, 3 } );
    const auto g0 = d.import_global_value( { ir::extns::data, 9 } );
    const auto g1 = d.import_global_value( { ir::extns::data, 1 } );
    CHECK( f0.id == 0 );
    CHECK( f1.id == 1 );
    CHECK( g0.id == 0 );
    CHECK( g1.id == 1 );
    REQUIRE( d.function_decls( ).size( ) == 2 );
    REQUIRE( d.data_decls( ).size( ) == 2 );
    CHECK( d.function_decls( )[ 0 ] == ir::ExternalName { ir::extns::function, 7 } );
    CHECK( d.function_decls( )[ 1 ] == ir::ExternalName { ir::extns::function, 3 } );
    CHECK( d.data_decls( )[ 0 ] == ir::ExternalName { ir::extns::data, 9 } );
    CHECK( d.data_decls( )[ 1 ] == ir::ExternalName { ir::extns::data, 1 } );

    d.write_function_addr( 0, f1 );
    d.write_function_addr( 56, f0 );
    d.write_data_addr( 8, g1, -5 );
    d.write_data_addr( 16, g0, 1234 );
    REQUIRE( d.function_relocs( ).size( ) == 2 );
    REQUIRE( d.data_relocs( ).size( ) == 2 );
    CHECK( d.function_relocs( )[ 0 ].offset == 0 );
    CHECK( d.function_relocs( )[ 0 ].func.id == 1 );
    CHECK( d.function_relocs( )[ 1 ].offset == 56 );
    CHECK( d.function_relocs( )[ 1 ].func.id == 0 );
    CHECK( d.data_relocs( )[ 0 ].offset == 8 );
    CHECK( d.data_relocs( )[ 0 ].data.id == 1 );
    CHECK( d.data_relocs( )[ 0 ].addend == -5 );
    CHECK( d.data_relocs( )[ 1 ].addend == 1234 );
}

TEST_CASE( "data description clear resets everything", "[jit][data][module]" )
{
    module::DataDescription d;
    d.define( std::string_view { "12345678" } );
    d.set_align( 16 );
    d.write_function_addr( 0, d.import_function( { ir::extns::function, 0 } ) );
    d.write_data_addr( 0, d.import_global_value( { ir::extns::data, 0 } ), 1 );
    d.clear( );
    CHECK( d.size( ) == 0 );
    CHECK( std::holds_alternative< std::monostate >( d.init( ) ) );
    CHECK_FALSE( d.align( ).has_value( ) );
    CHECK( d.function_decls( ).empty( ) );
    CHECK( d.data_decls( ).empty( ) );
    CHECK( d.function_relocs( ).empty( ) );
    CHECK( d.data_relocs( ).empty( ) );
    d.define_zeroinit( 3 );
    CHECK( d.size( ) == 3 );
}

TEST_CASE( "writable data defined with bytes keeps its initial contents", "[jit][data]" )
{
    Jit        j;
    const auto w = define_bytes( j, "w", bytes_of_values< int32_t >( { -1, 2, -3, 4 } ), true );
    const auto id
        = j.define( "f", Type::i64, { Type::i64 },
                    [ & ]( ir::IRBuilder& b, const Args& a )
                    {
                        const auto p = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( w, b ) );
                        b.ret( b.sextend( Type::i64, b.load( Type::i32, b.iadd( p, b.imul( a[ 0 ], b.iconst( Type::i64, 4 ) ) ) ) ) );
                    } );
    j.finalise( );
    const auto f = j.get< int64_t( int64_t ) >( id );
    CHECK( f( 0 ) == -1 );
    CHECK( f( 1 ) == 2 );
    CHECK( f( 2 ) == -3 );
    CHECK( f( 3 ) == 4 );
}
