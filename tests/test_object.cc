#include "support.hh"

#include <xiao-object/module.hh>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace xt;
using module::Linkage;
using module::ModuleError;
using object::RelocationKind;
using object::SectionKind;
using object::SymbolScope;

namespace
{
    struct Obj
    {
        ir::IRModule         irm;
        object::ObjectModule mod {
            object::ObjectBuilder { isa::lookup_host( ), "unit" }
        };

        module::FuncId declare( const std::string_view name, const Type ret, const std::vector< Type >& params, const Linkage linkage )
        {
            const auto id = mod.declare_function( name, linkage, module::Signature { .params = params, .ret = ret } );
            REQUIRE( id.has_value( ) );
            return *id;
        }

        module::DataId data( const std::string_view name, const Linkage linkage, const bool writable )
        {
            const auto id = mod.declare_data( name, linkage, writable );
            REQUIRE( id.has_value( ) );
            return *id;
        }

        template < typename Body >
        codegen::CompiledCode build( const module::FuncId id, Body&& body )
        {
            const auto& decl = mod.declarations( ).get( id );
            const auto  ref  = irm.declare( decl.name, decl.sig.ret, std::span< const Type > { decl.sig.params }, decl.sig.cc );
            auto&       fn   = *irm.get( ref );
            {
                ir::IRBuilder b { fn };
                const auto    entry = b.add_block( std::span< const Type > { decl.sig.params } );
                b.switch_to_block( entry );
                Args args;
                for ( uint16_t i { 0 }; i < decl.sig.params.size( ); ++i ) args.push_back( b.param( i ) );
                body( b, args );
                b.finalise( );
            }
            REQUIRE( verify( fn ).empty( ) );
            codegen::Context ctx { fn };
            REQUIRE( mod.define_function( id, ctx ).has_value( ) );
            return *ctx.compiled( );
        }

        module::FuncId constant( const std::string_view name, const int64_t v, const Linkage linkage = Linkage::Export )
        {
            const auto id = declare( name, Type::i64, { }, linkage );
            build( id,
                   [ & ]( ir::IRBuilder& b, const Args& )
                   {
                       b.ret( b.iconst( Type::i64, static_cast< uint64_t >( v ) ) );
                   } );
            return id;
        }

        void bytes( const module::DataId id, const std::vector< uint8_t >& raw, const uint64_t align = 0 )
        {
            module::DataDescription d;
            d.define( raw );
            if ( align ) d.set_align( align );
            REQUIRE( mod.define_data( id, d ).has_value( ) );
        }

        void zeros( const module::DataId id, const std::size_t n, const uint64_t align = 0 )
        {
            module::DataDescription d;
            d.define_zeroinit( n );
            if ( align ) d.set_align( align );
            REQUIRE( mod.define_data( id, d ).has_value( ) );
        }

        object::ObjectProduct finish( )
        {
            auto p = std::move( mod ).finish( );
            REQUIRE( p.has_value( ) );
            return std::move( *p );
        }
    };

    const object::Section* find_section( const object::Object& o, const std::string_view name )
    {
        for ( const auto& s : o.sections( ) )
            if ( s.name == name ) return &s;
        return nullptr;
    }

    std::optional< uint32_t > find_symbol( const object::Object& o, const std::string_view name )
    {
        for ( uint32_t i { 0 }; i < o.symbols( ).size( ); ++i )
            if ( o.symbols( )[ i ].name == name ) return i;
        return std::nullopt;
    }
}

TEST_CASE( "object sections are numbered in creation order", "[object]" )
{
    object::Object o;
    CHECK( o.add_section( ".text", SectionKind::Text, 16 ) == 0 );
    CHECK( o.add_section( ".rdata", SectionKind::ReadOnlyData, 1 ) == 1 );
    CHECK( o.add_section( ".bss", SectionKind::UninitialisedData, 8 ) == 2 );
    REQUIRE( o.sections( ).size( ) == 3 );
    CHECK( o.sections( )[ 0 ].name == ".text" );
    CHECK( o.sections( )[ 0 ].kind == SectionKind::Text );
    CHECK( o.sections( )[ 0 ].align == 16 );
    CHECK( o.sections( )[ 1 ].kind == SectionKind::ReadOnlyData );
    CHECK( o.sections( )[ 2 ].kind == SectionKind::UninitialisedData );
    for ( const auto& s : o.sections( ) )
    {
        CHECK( s.size( ) == 0 );
        CHECK( s.bytes.empty( ) );
        CHECK( s.relocs.empty( ) );
    }
    CHECK( o.symbols( ).empty( ) );
}

TEST_CASE( "object text padding uses int3", "[object]" )
{
    object::Object o;
    const auto     t   = o.add_section( ".text", SectionKind::Text, 1 );
    const uint8_t  a[] = { 1, 2, 3 };
    const uint8_t  b[] = { 4 };
    CHECK( o.append( t, a, 1 ) == 0 );
    CHECK( o.append( t, b, 16 ) == 16 );
    const auto& s = o.sections( )[ t ];
    REQUIRE( s.bytes.size( ) == 17 );
    CHECK( s.bytes[ 0 ] == 1 );
    CHECK( s.bytes[ 2 ] == 3 );
    for ( std::size_t k = 3; k < 16; ++k ) CHECK( s.bytes[ k ] == 0xCC );
    CHECK( s.bytes[ 16 ] == 4 );
    CHECK( s.align == 16 );
    CHECK( s.size( ) == 17 );
}

TEST_CASE( "object data padding uses zero", "[object]" )
{
    for ( const auto kind : { SectionKind::ReadOnlyData, SectionKind::Data } )
    {
        object::Object o;
        const auto     d   = o.add_section( ".d", kind, 1 );
        const uint8_t  a[] = { 0xFF };
        CHECK( o.append( d, a, 1 ) == 0 );
        CHECK( o.append( d, a, 8 ) == 8 );
        CHECK( o.append( d, a, 2 ) == 10 );
        CHECK( o.append( d, a, 4 ) == 12 );
        const auto& s = o.sections( )[ d ];
        CHECK( s.bytes == std::vector< uint8_t > { 0xFF, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0, 0xFF, 0, 0xFF } );
        CHECK( s.align == 8 );
    }
}

TEST_CASE( "object append never lowers section alignment", "[object]" )
{
    object::Object o;
    const auto     d   = o.add_section( ".d", SectionKind::Data, 32 );
    const uint8_t  a[] = { 1, 2 };
    CHECK( o.append( d, a, 1 ) == 0 );
    CHECK( o.append( d, a, 2 ) == 2 );
    CHECK( o.sections( )[ d ].align == 32 );
    CHECK( o.append( d, a, 64 ) == 64 );
    CHECK( o.sections( )[ d ].align == 64 );
}

TEST_CASE( "object append of empty bytes still aligns", "[object]" )
{
    object::Object o;
    const auto     d   = o.add_section( ".d", SectionKind::Data, 1 );
    const uint8_t  a[] = { 1 };
    ( void ) o.append( d, a, 1 );
    CHECK( o.append( d, { }, 8 ) == 8 );
    CHECK( o.sections( )[ d ].bytes.size( ) == 8 );
}

TEST_CASE( "object bss reserves space without bytes", "[object]" )
{
    object::Object o;
    const auto     b = o.add_section( ".bss", SectionKind::UninitialisedData, 1 );
    CHECK( o.reserve( b, 10, 8 ) == 0 );
    CHECK( o.reserve( b, 4, 16 ) == 16 );
    CHECK( o.reserve( b, 1, 1 ) == 20 );
    CHECK( o.reserve( b, 0, 4 ) == 24 );
    const auto& s = o.sections( )[ b ];
    CHECK( s.size( ) == 24 );
    CHECK( s.bss_size == 24 );
    CHECK( s.bytes.empty( ) );
    CHECK( s.align == 16 );
}

TEST_CASE( "object symbols and relocations are recorded verbatim", "[object]" )
{
    object::Object o;
    const auto     t  = o.add_section( ".text", SectionKind::Text, 16 );
    const auto     s0 = o.add_symbol( { .name = "a", .scope = SymbolScope::Global, .section = t, .value = 4, .is_function = true } );
    const auto     s1 = o.add_symbol( { .name = "b", .scope = SymbolScope::Undefined } );
    const auto     s2 = o.add_symbol( { .name = "c", .scope = SymbolScope::Local, .section = t, .value = 0 } );
    CHECK( s0 == 0 );
    CHECK( s1 == 1 );
    CHECK( s2 == 2 );
    o.add_reloc( t, { .offset = 3, .symbol = s1, .kind = RelocationKind::Rel32, .addend = -4 } );
    o.add_reloc( t, { .offset = 9, .symbol = s2, .kind = RelocationKind::Abs64, .addend = 100 } );
    const auto& rs = o.sections( )[ t ].relocs;
    REQUIRE( rs.size( ) == 2 );
    CHECK( rs[ 0 ].offset == 3 );
    CHECK( rs[ 0 ].symbol == 1 );
    CHECK( rs[ 0 ].kind == RelocationKind::Rel32 );
    CHECK( rs[ 0 ].addend == -4 );
    CHECK( rs[ 1 ].kind == RelocationKind::Abs64 );
    CHECK( rs[ 1 ].addend == 100 );
    CHECK( o.symbols( )[ 0 ].is_function );
    CHECK( o.symbols( )[ 0 ].value == 4 );
    CHECK( o.symbols( )[ 1 ].scope == SymbolScope::Undefined );
    CHECK( o.symbols( )[ 2 ].scope == SymbolScope::Local );
}

TEST_CASE( "object module with nothing declared finishes empty", "[object]" )
{
    Obj        o;
    const auto p = o.finish( );
    CHECK( p.object( ).sections( ).empty( ) );
    CHECK( p.object( ).symbols( ).empty( ) );
    CHECK( p.name( ) == "unit" );
}

TEST_CASE( "object module creates sections lazily with fixed names and alignment", "[object]" )
{
    Obj o;
    o.zeros( o.data( "w", Linkage::Local, true ), 8 );
    o.bytes( o.data( "r", Linkage::Local, false ), { 1 } );
    ( void ) o.constant( "f", 1 );
    o.bytes( o.data( "d", Linkage::Local, true ), { 2 } );
    o.bytes( o.data( "r2", Linkage::Local, false ), { 3 } );
    const auto  p  = o.finish( );
    const auto& ss = p.object( ).sections( );
    REQUIRE( ss.size( ) == 4 );
    CHECK( ss[ 0 ].name == ".bss" );
    CHECK( ss[ 0 ].kind == SectionKind::UninitialisedData );
    CHECK( ss[ 0 ].align == 8 );
    CHECK( ss[ 1 ].name == ".rdata" );
    CHECK( ss[ 1 ].kind == SectionKind::ReadOnlyData );
    CHECK( ss[ 1 ].align == 1 );
    CHECK( ss[ 2 ].name == ".text" );
    CHECK( ss[ 2 ].kind == SectionKind::Text );
    CHECK( ss[ 2 ].align == 16 );
    CHECK( ss[ 3 ].name == ".data" );
    CHECK( ss[ 3 ].kind == SectionKind::Data );
    CHECK( ss[ 3 ].align == 8 );
    CHECK( ss[ 1 ].bytes == std::vector< uint8_t > { 1, 3 } );
}

TEST_CASE( "object module only creates sections that are used", "[object]" )
{
    Obj o;
    o.bytes( o.data( "r", Linkage::Export, false ), { 'h', 'i' } );
    const auto p = o.finish( );
    REQUIRE( p.object( ).sections( ).size( ) == 1 );
    CHECK( p.object( ).sections( )[ 0 ].name == ".rdata" );
}

TEST_CASE( "object module places functions at aligned text offsets", "[object]" )
{
    Obj                           o;
    std::vector< module::FuncId > ids;
    std::vector< std::size_t >    sizes;
    for ( int k = 0; k < 5; ++k )
    {
        const auto id   = o.declare( std::format( "f{}", k ), Type::i64, { Type::i64 }, Linkage::Export );
        const auto code = o.build( id,
                                   [ & ]( ir::IRBuilder& b, const Args& a )
                                   {
                                       auto v = a[ 0 ];
                                       for ( int n = 0; n < k; ++n ) v = b.imul( v, b.iconst( Type::i64, n + 2 ) );
                                       b.ret( v );
                                   } );
        ids.push_back( id );
        sizes.push_back( code.code.size( ) );
    }
    const auto  p    = o.finish( );
    const auto* text = find_section( p.object( ), ".text" );
    REQUIRE( text != nullptr );
    uint32_t expect = 0;
    for ( int k = 0; k < 5; ++k )
    {
        const auto sym = find_symbol( p.object( ), std::format( "f{}", k ) );
        REQUIRE( sym.has_value( ) );
        const auto& s = p.object( ).symbols( )[ *sym ];
        CHECK( s.value == expect );
        CHECK( s.value % 16 == 0 );
        CHECK( s.is_function );
        CHECK( s.scope == SymbolScope::Global );
        for ( auto pad = static_cast< uint32_t >( expect + sizes[ k ] );
              pad < ( ( expect + sizes[ k ] + 15 ) & ~15u ) && pad < text->bytes.size( ); ++pad )
            CHECK( text->bytes[ pad ] == 0xCC );
        expect = static_cast< uint32_t >( ( expect + sizes[ k ] + 15 ) & ~std::size_t { 15 } );
    }
}

TEST_CASE( "object module text contains the compiled bytes", "[object]" )
{
    Obj         o;
    const auto  id   = o.declare( "f", Type::i64, { Type::i64, Type::i64 }, Linkage::Export );
    const auto  code = o.build( id,
                                []( ir::IRBuilder& b, const Args& a )
                                {
                                   b.ret( b.iadd( a[ 0 ], a[ 1 ] ) );
                                } );
    const auto  p    = o.finish( );
    const auto* text = find_section( p.object( ), ".text" );
    REQUIRE( text != nullptr );
    CHECK( text->bytes == code.code );
    CHECK( text->relocs.empty( ) );
}

TEST_CASE( "object module chooses a section per data kind", "[object]" )
{
    Obj        o;
    const auto rb = o.data( "rb", Linkage::Local, false );
    const auto rz = o.data( "rz", Linkage::Local, false );
    const auto wb = o.data( "wb", Linkage::Local, true );
    const auto wz = o.data( "wz", Linkage::Local, true );
    o.bytes( rb, { 1, 2, 3 } );
    o.zeros( rz, 5 );
    o.bytes( wb, { 9, 8 } );
    o.zeros( wz, 40 );
    const auto  p   = o.finish( );
    const auto& obj = p.object( );

    const auto* rdata = find_section( obj, ".rdata" );
    const auto* data  = find_section( obj, ".data" );
    const auto* bss   = find_section( obj, ".bss" );
    REQUIRE( rdata );
    REQUIRE( data );
    REQUIRE( bss );
    CHECK( rdata->bytes == std::vector< uint8_t > { 1, 2, 3, 0, 0, 0, 0, 0 } );
    CHECK( data->bytes == std::vector< uint8_t > { 9, 8 } );
    CHECK( bss->bytes.empty( ) );
    CHECK( bss->bss_size == 40 );

    const auto sym_section = [ & ]( const std::string_view n )
    {
        return obj.sections( )[ obj.symbols( )[ *find_symbol( obj, n ) ].section ].name;
    };
    CHECK( sym_section( "rb" ) == ".rdata" );
    CHECK( sym_section( "rz" ) == ".rdata" );
    CHECK( sym_section( "wb" ) == ".data" );
    CHECK( sym_section( "wz" ) == ".bss" );
    CHECK( obj.symbols( )[ *find_symbol( obj, "rz" ) ].value == 3 );
    for ( const auto& s : obj.symbols( ) ) CHECK_FALSE( s.is_function );
}

TEST_CASE( "object module data alignment pads within the section", "[object]" )
{
    Obj        o;
    const auto a = o.data( "a", Linkage::Local, true );
    const auto b = o.data( "b", Linkage::Local, true );
    const auto c = o.data( "c", Linkage::Local, true );
    o.bytes( a, { 1 } );
    o.bytes( b, { 2 }, 32 );
    o.bytes( c, { 3 } );
    const auto  p   = o.finish( );
    const auto& obj = p.object( );
    CHECK( obj.symbols( )[ *find_symbol( obj, "a" ) ].value == 0 );
    CHECK( obj.symbols( )[ *find_symbol( obj, "b" ) ].value == 32 );
    CHECK( obj.symbols( )[ *find_symbol( obj, "c" ) ].value == 40 );
    CHECK( find_section( obj, ".data" )->align == 32 );
    CHECK( find_section( obj, ".data" )->bytes.size( ) == 41 );
}

TEST_CASE( "object module bss alignment", "[object]" )
{
    Obj        o;
    const auto a = o.data( "a", Linkage::Local, true );
    const auto b = o.data( "b", Linkage::Local, true );
    o.zeros( a, 3 );
    o.zeros( b, 5, 64 );
    const auto  p   = o.finish( );
    const auto& obj = p.object( );
    CHECK( obj.symbols( )[ *find_symbol( obj, "b" ) ].value == 64 );
    CHECK( find_section( obj, ".bss" )->bss_size == 69 );
    CHECK( find_section( obj, ".bss" )->align == 64 );
}

TEST_CASE( "object module symbol scopes follow linkage", "[object]" )
{
    Obj o;
    ( void ) o.constant( "exported", 1, Linkage::Export );
    ( void ) o.constant( "local", 2, Linkage::Local );
    o.bytes( o.data( "edata", Linkage::Export, false ), { 1 } );
    o.bytes( o.data( "ldata", Linkage::Local, true ), { 1 } );
    ( void ) o.declare( "unused_import", Type::i64, { }, Linkage::Import );
    ( void ) o.data( "unused_data_import", Linkage::Import, false );
    const auto  p   = o.finish( );
    const auto& obj = p.object( );
    REQUIRE( obj.symbols( ).size( ) == 4 );
    CHECK( obj.symbols( )[ 0 ].name == "exported" );
    CHECK( obj.symbols( )[ 0 ].scope == SymbolScope::Global );
    CHECK( obj.symbols( )[ 1 ].name == "local" );
    CHECK( obj.symbols( )[ 1 ].scope == SymbolScope::Local );
    CHECK( obj.symbols( )[ 2 ].name == "edata" );
    CHECK( obj.symbols( )[ 2 ].scope == SymbolScope::Global );
    CHECK( obj.symbols( )[ 3 ].name == "ldata" );
    CHECK( obj.symbols( )[ 3 ].scope == SymbolScope::Local );
    CHECK_FALSE( find_symbol( obj, "unused_import" ).has_value( ) );
    CHECK_FALSE( find_symbol( obj, "unused_data_import" ).has_value( ) );
}

TEST_CASE( "object module call to a local function is a rel32 relocation", "[object]" )
{
    Obj        o;
    const auto g = o.constant( "g", 5, Linkage::Local );
    const auto f = o.declare( "f", Type::i64, { }, Linkage::Export );
    o.build( f,
             [ & ]( ir::IRBuilder& b, const Args& )
             {
                 b.ret( b.call( o.mod.declare_func_in_func( g, b ), { } ) );
             } );
    const auto  p    = o.finish( );
    const auto& obj  = p.object( );
    const auto* text = find_section( obj, ".text" );
    REQUIRE( text->relocs.size( ) == 1 );
    const auto& r = text->relocs[ 0 ];
    CHECK( r.kind == RelocationKind::Rel32 );
    CHECK( r.addend == -4 );
    CHECK( obj.symbols( )[ r.symbol ].name == "g" );
    CHECK( r.offset > obj.symbols( )[ *find_symbol( obj, "f" ) ].value );
    CHECK( text->bytes[ r.offset - 1 ] == 0xE8 );
}

TEST_CASE( "object module call to an import is an abs64 relocation against an undefined symbol", "[object]" )
{
    Obj        o;
    const auto ext = o.declare( "ext", Type::i64, { Type::i64 }, Linkage::Import );
    const auto f   = o.declare( "f", Type::i64, { Type::i64 }, Linkage::Export );
    const auto h   = o.declare( "h", Type::i64, { Type::i64 }, Linkage::Export );
    for ( const auto id : { f, h } )
        o.build( id,
                 [ & ]( ir::IRBuilder& b, const Args& a )
                 {
                     b.ret( b.call( o.mod.declare_func_in_func( ext, b ), a ) );
                 } );
    const auto  p    = o.finish( );
    const auto& obj  = p.object( );
    const auto* text = find_section( obj, ".text" );
    REQUIRE( text->relocs.size( ) == 2 );
    const auto sym = find_symbol( obj, "ext" );
    REQUIRE( sym.has_value( ) );
    CHECK( obj.symbols( )[ *sym ].scope == SymbolScope::Undefined );
    CHECK( obj.symbols( )[ *sym ].is_function );
    CHECK( *sym == 2 );
    CHECK( obj.symbols( ).size( ) == 3 );
    for ( const auto& r : text->relocs )
    {
        CHECK( r.kind == RelocationKind::Abs64 );
        CHECK( r.addend == 0 );
        CHECK( r.symbol == *sym );
        CHECK( ( text->bytes[ r.offset - 1 ] & 0xF8 ) == 0xB8 );
    }
    CHECK( text->relocs[ 0 ].offset < 16 );
    CHECK( text->relocs[ 1 ].offset >= obj.symbols( )[ *find_symbol( obj, "h" ) ].value );
}

TEST_CASE( "object module symbol_value of local data is a rel32 relocation", "[object]" )
{
    Obj        o;
    const auto msg = o.data( "msg", Linkage::Local, false );
    o.bytes( msg, { 'x', 'y' } );
    const auto f = o.declare( "f", Type::ptr, { }, Linkage::Export );
    o.build( f,
             [ & ]( ir::IRBuilder& b, const Args& )
             {
                 b.ret( b.symbol_value( Type::ptr, o.mod.declare_data_in_func( msg, b ) ) );
             } );
    const auto  p    = o.finish( );
    const auto& obj  = p.object( );
    const auto* text = find_section( obj, ".text" );
    REQUIRE( text->relocs.size( ) == 1 );
    const auto& r = text->relocs[ 0 ];
    CHECK( r.kind == RelocationKind::Rel32 );
    CHECK( r.addend == -4 );
    CHECK( obj.symbols( )[ r.symbol ].name == "msg" );
    CHECK( ( text->bytes[ r.offset - 1 ] & 0xC7 ) == 0x05 );
    CHECK( text->bytes[ r.offset - 2 ] == 0x8D );
}

TEST_CASE( "object module symbol_value of imported data is an abs64 relocation", "[object]" )
{
    Obj        o;
    const auto ext = o.data( "ext_data", Linkage::Import, true );
    const auto f   = o.declare( "f", Type::i64, { }, Linkage::Export );
    o.build( f,
             [ & ]( ir::IRBuilder& b, const Args& )
             {
                 b.ret( b.load( Type::i64, b.symbol_value( Type::ptr, o.mod.declare_data_in_func( ext, b ) ) ) );
             } );
    const auto  p    = o.finish( );
    const auto& obj  = p.object( );
    const auto* text = find_section( obj, ".text" );
    REQUIRE( text->relocs.size( ) == 1 );
    CHECK( text->relocs[ 0 ].kind == RelocationKind::Abs64 );
    const auto& s = obj.symbols( )[ text->relocs[ 0 ].symbol ];
    CHECK( s.name == "ext_data" );
    CHECK( s.scope == SymbolScope::Undefined );
    CHECK_FALSE( s.is_function );
}

TEST_CASE( "object module global value offsets become addends", "[object]" )
{
    Obj        o;
    const auto arr = o.data( "arr", Linkage::Local, false );
    o.bytes( arr, std::vector< uint8_t >( 64, 1 ) );
    const auto f = o.declare( "f", Type::ptr, { }, Linkage::Export );
    o.build( f,
             [ & ]( ir::IRBuilder& b, const Args& )
             {
                 const auto gv_near = b.create_global_value( {
                     .name      = { ir::extns::data, arr.id },
                     .offset    = 24,
                     .colocated = true
                 } );
                 const auto gv_far  = b.create_global_value( {
                     .name      = { ir::extns::data, arr.id },
                     .offset    = -8,
                     .colocated = false
                 } );
                 b.ret( b.iadd( b.symbol_value( Type::ptr, gv_near ), b.symbol_value( Type::ptr, gv_far ) ) );
             } );
    const auto  p    = o.finish( );
    const auto& obj  = p.object( );
    const auto* text = find_section( obj, ".text" );
    REQUIRE( text->relocs.size( ) == 2 );
    bool saw_near = false, saw_far = false;
    for ( const auto& r : text->relocs )
    {
        CHECK( obj.symbols( )[ r.symbol ].name == "arr" );
        if ( r.kind == RelocationKind::Rel32 )
        {
            saw_near = true;
            CHECK( r.addend == 20 );
        }
        else
        {
            saw_far = true;
            CHECK( r.addend == -8 );
        }
    }
    CHECK( saw_near );
    CHECK( saw_far );
}

TEST_CASE( "object module data relocations are abs64 at the data offset", "[object]" )
{
    Obj        o;
    const auto g   = o.constant( "g", 1, Linkage::Local );
    const auto ext = o.declare( "ext", Type::i64, { }, Linkage::Import );
    const auto msg = o.data( "msg", Linkage::Local, false );
    const auto pad = o.data( "pad", Linkage::Local, true );
    const auto tbl = o.data( "tbl", Linkage::Export, true );
    o.bytes( msg, { 'a', 'b', 'c' } );
    o.bytes( pad, { 1, 2, 3 } );
    {
        module::DataDescription d;
        d.define( std::vector< uint8_t >( 32, 0 ) );
        d.write_function_addr( 0, o.mod.declare_func_in_data( g, d ) );
        d.write_function_addr( 8, o.mod.declare_func_in_data( ext, d ) );
        d.write_data_addr( 16, o.mod.declare_data_in_data( msg, d ), 2 );
        d.write_data_addr( 24, o.mod.declare_data_in_data( tbl, d ), -16 );
        REQUIRE( o.mod.define_data( tbl, d ).has_value( ) );
    }
    const auto  p    = o.finish( );
    const auto& obj  = p.object( );
    const auto* data = find_section( obj, ".data" );
    REQUIRE( data != nullptr );
    CHECK( data->bytes.size( ) == 40 );
    CHECK( obj.symbols( )[ *find_symbol( obj, "tbl" ) ].value == 8 );
    CHECK( obj.symbols( )[ *find_symbol( obj, "tbl" ) ].scope == SymbolScope::Global );
    REQUIRE( data->relocs.size( ) == 4 );
    const auto& rs = data->relocs;
    for ( const auto& r : rs ) CHECK( r.kind == RelocationKind::Abs64 );
    CHECK( rs[ 0 ].offset == 8 );
    CHECK( obj.symbols( )[ rs[ 0 ].symbol ].name == "g" );
    CHECK( rs[ 0 ].addend == 0 );
    CHECK( rs[ 1 ].offset == 16 );
    CHECK( obj.symbols( )[ rs[ 1 ].symbol ].name == "ext" );
    CHECK( obj.symbols( )[ rs[ 1 ].symbol ].scope == SymbolScope::Undefined );
    CHECK( rs[ 2 ].offset == 24 );
    CHECK( obj.symbols( )[ rs[ 2 ].symbol ].name == "msg" );
    CHECK( rs[ 2 ].addend == 2 );
    CHECK( rs[ 3 ].offset == 32 );
    CHECK( obj.symbols( )[ rs[ 3 ].symbol ].name == "tbl" );
    CHECK( rs[ 3 ].addend == -16 );
    CHECK( find_section( obj, ".rdata" )->relocs.empty( ) );
}

TEST_CASE( "object module finish fails on an undefined local function", "[object]" )
{
    for ( const auto linkage : { Linkage::Local, Linkage::Export } )
    {
        Obj o;
        ( void ) o.constant( "ok", 1 );
        ( void ) o.declare( "missing", Type::i64, { }, linkage );
        const auto p = std::move( o.mod ).finish( );
        REQUIRE_FALSE( p.has_value( ) );
        CHECK( p.error( ) == ModuleError::UndefinedFunction );
    }
}

TEST_CASE( "object module finish fails on undefined local data", "[object]" )
{
    for ( const auto linkage : { Linkage::Local, Linkage::Export } )
    {
        Obj o;
        ( void ) o.data( "missing", linkage, true );
        const auto p = std::move( o.mod ).finish( );
        REQUIRE_FALSE( p.has_value( ) );
        CHECK( p.error( ) == ModuleError::UndefinedData );
    }
}

TEST_CASE( "object module rejects defining imports", "[object]" )
{
    Obj        o;
    const auto f  = o.declare( "f", Type::i64, { }, Linkage::Import );
    const auto d  = o.data( "d", Linkage::Import, false );
    const auto rf = o.irm.declare( "f", Type::i64, { } );
    {
        ir::IRBuilder b { *o.irm.get( rf ) };
        b.switch_to_block( b.add_block( { } ) );
        b.ret( b.iconst( Type::i64, 0 ) );
        b.finalise( );
    }
    codegen::Context ctx { *o.irm.get( rf ) };
    const auto       r1 = o.mod.define_function( f, ctx );
    REQUIRE_FALSE( r1.has_value( ) );
    CHECK( r1.error( ) == ModuleError::InvalidImportDefinition );

    module::DataDescription desc;
    desc.define_zeroinit( 4 );
    const auto r2 = o.mod.define_data( d, desc );
    REQUIRE_FALSE( r2.has_value( ) );
    CHECK( r2.error( ) == ModuleError::InvalidImportDefinition );
}

TEST_CASE( "object module rejects duplicate and uninitialised definitions", "[object]" )
{
    Obj        o;
    const auto f  = o.constant( "f", 1 );
    const auto rf = o.irm.declare( "f_again", Type::i64, { } );
    {
        ir::IRBuilder b { *o.irm.get( rf ) };
        b.switch_to_block( b.add_block( { } ) );
        b.ret( b.iconst( Type::i64, 2 ) );
        b.finalise( );
    }
    codegen::Context ctx { *o.irm.get( rf ) };
    const auto       r1 = o.mod.define_function( f, ctx );
    REQUIRE_FALSE( r1.has_value( ) );
    CHECK( r1.error( ) == ModuleError::DuplicateDefinition );

    const auto d = o.data( "d", Linkage::Local, true );
    o.zeros( d, 4 );
    module::DataDescription again;
    again.define_zeroinit( 4 );
    const auto r2 = o.mod.define_data( d, again );
    REQUIRE_FALSE( r2.has_value( ) );
    CHECK( r2.error( ) == ModuleError::DuplicateDefinition );

    const auto                    e = o.data( "e", Linkage::Local, false );
    const module::DataDescription empty;
    const auto                    r3 = o.mod.define_data( e, empty );
    REQUIRE_FALSE( r3.has_value( ) );
    CHECK( r3.error( ) == ModuleError::UninitialisedData );
}

TEST_CASE( "object module exposes its isa and declarations", "[object]" )
{
    Obj o;
    CHECK( o.mod.isa( ).name( ) == "x86_64" );
    ( void ) o.declare( "f", Type::i64, { }, Linkage::Import );
    CHECK( o.mod.declarations( ).size( ) == 1 );
    const auto bad = o.mod.declare_data( "f", Linkage::Import, false );
    REQUIRE_FALSE( bad.has_value( ) );
    CHECK( bad.error( ) == ModuleError::IncompatibleDeclaration );
}

TEST_CASE( "object product emits coff and writes it to disk", "[object]" )
{
    Obj o;
    ( void ) o.constant( "main", 0 );
    o.bytes( o.data( "msg", Linkage::Local, false ), { 'o', 'k' } );
    const auto p     = o.finish( );
    const auto bytes = p.emit( );
    REQUIRE( bytes.size( ) > 20 );
    CHECK( bytes[ 0 ] == 0x64 );
    CHECK( bytes[ 1 ] == 0x86 );
    CHECK( bytes[ 2 ] == 2 );
    CHECK( bytes == object::coff::write( p.object( ) ) );
    CHECK( p.name( ) == "unit" );

    const auto path = std::filesystem::temp_directory_path( ) / "xiao_test_object_product.obj";
    REQUIRE( p.write( path ) );
    std::vector< uint8_t > back;
    {
        std::ifstream in { path, std::ios::binary };
        back.assign( std::istreambuf_iterator< char > { in }, std::istreambuf_iterator< char > { } );
    }
    CHECK( back == bytes );
    std::filesystem::remove( path );
}

TEST_CASE( "object product write fails for an unwritable path", "[object]" )
{
    Obj        o;
    const auto p    = o.finish( );
    const auto path = std::filesystem::temp_directory_path( ) / "xiao_no_such_dir_4c1e" / "nested" / "x.obj";
    CHECK_FALSE( p.write( path ) );
}
