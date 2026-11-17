#include "support.hh"

#include <xiao-object/dump.hh>
#include <xiao-object/module.hh>

#if defined( XIAO_HAS_ZYDIS )
#include <xiao-disasm/disasm.hh>
#endif

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace xt;
using object::RelocationKind;
using object::SectionKind;
using object::SymbolScope;

namespace
{
    bool contains( const std::string_view hay, const std::string_view needle )
    { return hay.find( needle ) != std::string_view::npos; }

    std::vector< std::string > lines_of( const std::string& s )
    {
        std::vector< std::string > out;
        std::istringstream         in { s };
        for ( std::string l; std::getline( in, l ); ) out.push_back( l );
        return out;
    }
} // namespace

TEST_CASE( "dump to_string covers every section kind", "[dump]" )
{
    CHECK( object::to_string( SectionKind::Text ) == "code" );
    CHECK( object::to_string( SectionKind::ReadOnlyData ) == "read-only data" );
    CHECK( object::to_string( SectionKind::Data ) == "writable data" );
    CHECK( object::to_string( SectionKind::UninitialisedData ) == "zero fill" );
    CHECK( object::to_string( static_cast< SectionKind >( 200 ) ) == "?" );
}

TEST_CASE( "dump to_string covers every symbol scope", "[dump]" )
{
    CHECK( object::to_string( SymbolScope::Local ) == "local" );
    CHECK( object::to_string( SymbolScope::Global ) == "global" );
    CHECK( object::to_string( SymbolScope::Undefined ) == "undefined" );
    CHECK( object::to_string( static_cast< SymbolScope >( 99 ) ) == "?" );
}

TEST_CASE( "dump to_string covers every relocation kind", "[dump]" )
{
    CHECK( object::to_string( RelocationKind::Rel32 ) == "rel32" );
    CHECK( object::to_string( RelocationKind::Abs64 ) == "abs64" );
    CHECK( object::to_string( static_cast< RelocationKind >( 7 ) ) == "?" );
}

TEST_CASE( "dump of an empty object", "[dump]" )
{
    const auto out = object::dump( object::Object { } );
    CHECK( out == "  sections\n\n  symbols\n\n  relocations\n    (none)\n" );
}

TEST_CASE( "dump of a single function object is exact", "[dump]" )
{
    object::Object o;
    const auto     t     = o.add_section( ".text", SectionKind::Text, 16 );
    const uint8_t  raw[] = { 0x90, 0xC3 };
    ( void ) o.append( t, raw, 1 );
    o.add_symbol( { .name = "main", .scope = SymbolScope::Global, .section = t, .value = 0, .is_function = true } );

    const std::string want = "  sections\n"
                             "    [0] .text  code            align 16    size 2       relocs 0\n"
                             "\n"
                             "  symbols\n"
                             "    [0] main  global     .text+0x0      function\n"
                             "\n"
                             "  relocations\n"
                             "    (none)\n"
                             "\n"
                             "  contents of .text\n"
                             "    0000  90 c3"
                           + std::string( 45, ' ' ) + "|..|\n";
    CHECK( object::dump( o ) == want );
}

TEST_CASE( "dump lists relocations with addends", "[dump]" )
{
    object::Object o;
    const auto     t = o.add_section( ".text", SectionKind::Text, 16 );
    ( void ) o.append( t, std::vector< uint8_t >( 24, 0 ), 1 );
    const auto puts_sym = o.add_symbol( { .name = "puts", .scope = SymbolScope::Undefined, .is_function = true } );
    const auto data_sym = o.add_symbol( { .name = "msg", .scope = SymbolScope::Local, .section = t, .value = 16 } );
    o.add_reloc( t, { .offset = 1, .symbol = puts_sym, .kind = RelocationKind::Rel32, .addend = -4 } );
    o.add_reloc( t, { .offset = 0x10, .symbol = data_sym, .kind = RelocationKind::Abs64, .addend = 0 } );
    o.add_reloc( t, { .offset = 0x123, .symbol = data_sym, .kind = RelocationKind::Abs64, .addend = 77 } );
    const auto out = object::dump( o );
    CHECK( contains( out, "    .text+0x0001  rel32  puts  addend -4\n" ) );
    CHECK( contains( out, "    .text+0x0010  abs64  msg\n" ) );
    CHECK( contains( out, "    .text+0x0123  abs64  msg  addend 77\n" ) );
    CHECK_FALSE( contains( out, "(none)" ) );
    CHECK( contains( out, "relocs 3" ) );
    CHECK( contains( out, "    [0] puts  undefined  -" ) );
    CHECK( contains( out, "    [1] msg   local      .text+0x10" ) );
    CHECK( contains( out, "data" ) );
}

TEST_CASE( "dump shows bss as zero fill without contents", "[dump]" )
{
    object::Object o;
    const auto     b = o.add_section( ".bss", SectionKind::UninitialisedData, 8 );
    ( void ) o.reserve( b, 16, 8 );
    const auto out = object::dump( o );
    CHECK( contains( out, "zero fill" ) );
    CHECK( contains( out, "size 16" ) );
    CHECK( contains( out, "  contents of .bss\n    (16 zero bytes, not stored in the file)\n" ) );
}

TEST_CASE( "dump contents rows are sixteen bytes with printable text", "[dump]" )
{
    object::Object         o;
    const auto             d = o.add_section( ".rdata", SectionKind::ReadOnlyData, 1 );
    std::vector< uint8_t > raw;
    for ( const char c : std::string_view { "hello, world! 0123456789" } ) raw.push_back( static_cast< uint8_t >( c ) );
    raw.push_back( 0 );
    raw.push_back( 0x7F );
    ( void ) o.append( d, raw, 1 );
    const auto lines = lines_of( object::dump( o ) );
    REQUIRE( lines.size( ) >= 2 );
    const auto& row0 = lines[ lines.size( ) - 2 ];
    const auto& row1 = lines[ lines.size( ) - 1 ];
    CHECK( row0 == "    0000  68 65 6c 6c 6f 2c 20 77  6f 72 6c 64 21 20 30 31  |hello, world! 01|" );
    CHECK( row1.starts_with( "    0010  32 33 34 35 36 37 38 39  00 7f " ) );
    CHECK( row1.ends_with( "|23456789..|" ) );
    CHECK( row0.size( ) == row1.size( ) + 6 );
}

TEST_CASE( "dump aligns names to the widest entry", "[dump]" )
{
    object::Object o;
    const auto     t = o.add_section( ".text$very_long", SectionKind::Text, 16 );
    const auto     d = o.add_section( ".d", SectionKind::Data, 8 );
    ( void ) o.append( t, std::vector< uint8_t > { 0xC3 }, 1 );
    ( void ) o.append( d, std::vector< uint8_t > { 1 }, 1 );
    o.add_symbol( { .name = "a_rather_long_symbol", .scope = SymbolScope::Global, .section = t, .value = 0, .is_function = true } );
    o.add_symbol( { .name = "x", .scope = SymbolScope::Local, .section = d, .value = 0 } );
    const auto out = object::dump( o );
    CHECK( contains( out, "    [1] .d               writable data" ) );
    CHECK( contains( out, "    [1] x                     local      .d+0x0" ) );
    CHECK( contains( out, "    [0] .text$very_long  code" ) );
}

TEST_CASE( "dump of an object module product", "[dump]" )
{
    ir::IRModule         irm;
    object::ObjectModule mod {
        object::ObjectBuilder { isa::lookup_host( ), "dumped" }
    };
    const auto puts_id = *mod.declare_function( "puts", module::Linkage::Import, { { Type::ptr }, Type::i32 } );
    const auto text_id = *mod.declare_data( "hello", module::Linkage::Local, false );
    const auto main_id = *mod.declare_function( "main", module::Linkage::Export, { { }, Type::i32 } );
    {
        module::DataDescription d;
        d.define( std::string_view { "hi\0", 3 } );
        REQUIRE( mod.define_data( text_id, d ).has_value( ) );
    }
    const auto ref = irm.declare( "main", Type::i32, { } );
    {
        ir::IRBuilder b { *irm.get( ref ) };
        const auto    p = mod.declare_func_in_func( puts_id, b );
        const auto    t = mod.declare_data_in_func( text_id, b );
        b.switch_to_block( b.add_block( { } ) );
        ( void ) b.call( p, Args { b.symbol_value( Type::ptr, t ) } );
        b.ret( b.iconst( Type::i32, 0 ) );
        b.finalise( );
    }
    codegen::Context ctx { *irm.get( ref ) };
    REQUIRE( mod.define_function( main_id, ctx ).has_value( ) );
    const auto product = std::move( mod ).finish( );
    REQUIRE( product.has_value( ) );
    const auto out = object::dump( product->object( ) );
    CHECK( contains( out, "[0] .rdata" ) );
    CHECK( contains( out, "[1] .text" ) );
    CHECK( contains( out, "read-only data" ) );
    CHECK( contains( out, "main" ) );
    CHECK( contains( out, "global" ) );
    CHECK( contains( out, "undefined" ) );
    CHECK( contains( out, "rel32  hello  addend -4" ) );
    CHECK( contains( out, "abs64  puts" ) );
    CHECK( contains( out, "  contents of .rdata\n    0000  68 69 00" ) );
    CHECK( contains( out, "|hi.|" ) );
    CHECK( contains( out, "  contents of .text\n" ) );
}

#if defined( XIAO_HAS_ZYDIS )

TEST_CASE( "disasm reloc_name covers every relocation kind", "[dump][disasm]" )
{
    CHECK( disasm::reloc_name( codegen::RelocKind::X86CallPCRel4 ) == "x86_call_pcrel4" );
    CHECK( disasm::reloc_name( codegen::RelocKind::X86PCRel4 ) == "x86_pcrel4" );
    CHECK( disasm::reloc_name( codegen::RelocKind::Abs8 ) == "abs8" );
    CHECK( disasm::reloc_name( static_cast< codegen::RelocKind >( 50 ) ) == "<?>" );
}

TEST_CASE( "disasm of empty code is empty", "[dump][disasm]" )
{ CHECK( disasm::dump( codegen::CompiledCode { } ).empty( ) ); }

TEST_CASE( "disasm shows offsets bytes and mnemonics", "[dump][disasm]" )
{
    const codegen::CompiledCode code {
        .code = { 0x48, 0x89, 0xC8, 0x48, 0x0F, 0xAF, 0xC2, 0xC3 }
    };
    const auto lines = lines_of( disasm::dump( code ) );
    REQUIRE( lines.size( ) == 3 );
    CHECK( lines[ 0 ].starts_with( "  0000  48 89 c8 " ) );
    CHECK( contains( lines[ 0 ], "mov rax, rcx" ) );
    CHECK( lines[ 1 ].starts_with( "  0003  48 0f af c2 " ) );
    CHECK( contains( lines[ 1 ], "imul rax, rdx" ) );
    CHECK( lines[ 2 ].starts_with( "  0007  c3 " ) );
    CHECK( contains( lines[ 2 ], "ret" ) );
    const auto col = lines[ 0 ].find( "mov" );
    CHECK( lines[ 1 ].find( "imul" ) == col );
    CHECK( lines[ 2 ].find( "ret" ) == col );
}

TEST_CASE( "disasm marks undecodable bytes", "[dump][disasm]" )
{
    const codegen::CompiledCode code {
        .code = { 0x06, 0xC3 }
    };
    const auto lines = lines_of( disasm::dump( code ) );
    REQUIRE( lines.size( ) == 2 );
    CHECK( lines[ 0 ].starts_with( "  0000  06 " ) );
    CHECK( contains( lines[ 0 ], "(bad)" ) );
    CHECK( lines[ 1 ].starts_with( "  0001  c3 " ) );
}

TEST_CASE( "disasm uses the base address for relative targets", "[dump][disasm]" )
{
    const codegen::CompiledCode code {
        .code = { 0xE8, 0x00, 0x00, 0x00, 0x00 }
    };
    CHECK( contains( disasm::dump( code, 0x1000 ), "1005" ) );
    CHECK( contains( disasm::dump( code, 0 ), "call" ) );
}

TEST_CASE( "disasm annotates relocations", "[dump][disasm]" )
{
    codegen::CompiledCode code {
        .code = { 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xC3 }
    };
    code.relocs.push_back( {
        .offset = 1,
        .kind   = codegen::RelocKind::X86CallPCRel4,
        .target = { 0, 3 },
        .addend = -4
    } );
    code.relocs.push_back( {
        .offset = 7,
        .kind   = codegen::RelocKind::Abs8,
        .target = { 1, 2 },
        .addend = 0
    } );
    const auto lines = lines_of( disasm::dump( code ) );
    REQUIRE( lines.size( ) == 3 );
    CHECK( lines[ 0 ].ends_with( "    ; x86_call_pcrel4 u0:3-4" ) );
    CHECK( lines[ 1 ].ends_with( "    ; abs8 u1:2" ) );
    CHECK( contains( lines[ 1 ], "mov rax" ) );
    CHECK_FALSE( contains( lines[ 2 ], ";" ) );
}

TEST_CASE( "disasm of a compiled function", "[dump][disasm]" )
{
    auto       f   = compile< int64_t( int64_t, int64_t ) >( Type::i64, { Type::i64, Type::i64 },
                                                             []( ir::IRBuilder& b, const Args& a )
                                                             {
                                                         b.ret( b.iadd( b.imul( a[ 0 ], a[ 1 ] ), b.iconst( Type::i64, 1 ) ) );
                                                     } );
    const auto out = disasm::dump( f.code );
    CHECK( contains( out, "imul" ) );
    CHECK( contains( out, "ret" ) );
    CHECK_FALSE( contains( out, "(bad)" ) );
    CHECK_FALSE( contains( out, ";" ) );
    CHECK( f( 6, 7 ) == 43 );
}

TEST_CASE( "disasm of compiled code with calls and data references", "[dump][disasm]" )
{
    Jit                     j;
    const auto              g   = j.define( "g", Type::i64, { },
                                            []( ir::IRBuilder& b, const Args& )
                                            {
                                 b.ret( b.iconst( Type::i64, 1 ) );
                             } );
    const auto              ext = j.import( "strlen", Type::i64, { Type::ptr } );
    const auto              msg = *j.mod.declare_data( "msg", module::Linkage::Local, false );
    module::DataDescription d;
    d.define( std::string_view { "abc\0", 4 } );
    REQUIRE( j.mod.define_data( msg, d ).has_value( ) );
    const auto f   = j.define( "f", Type::i64, { },
                               [ & ]( ir::IRBuilder& b, const Args& )
                               {
                                 const auto p = b.symbol_value( Type::ptr, j.mod.declare_data_in_func( msg, b ) );
                                 const auto x = b.call( j.mod.declare_func_in_func( g, b ), { } );
                                 b.ret( b.iadd( x, b.call( j.mod.declare_func_in_func( ext, b ), Args { p } ) ) );
                             } );
    const auto out = disasm::dump( j.last_code );
    CHECK( contains( out, std::format( "; x86_call_pcrel4 u0:{}-4", g.id ) ) );
    CHECK( contains( out, std::format( "; abs8 u0:{}", ext.id ) ) );
    CHECK( contains( out, std::format( "; x86_pcrel4 u1:{}-4", msg.id ) ) );
    CHECK( contains( out, "lea" ) );
    CHECK( contains( out, "call" ) );
    j.finalise( );
    CHECK( j.get< int64_t( ) >( f )( ) == 4 );
}

#endif
