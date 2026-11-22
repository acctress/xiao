#include "support.hh"
#include "catch2/generators/catch_generators.hpp"

#include <xiao-object/coff.hh>
#include <xiao-object/module.hh>

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#if !defined( _WIN32 )
#include <sys/wait.h>
#endif

using namespace xt;
using object::RelocationKind;
using object::SectionKind;
using object::SymbolScope;

namespace
{
    uint16_t rd16( const std::vector< uint8_t >& b, const std::size_t o )
    {
        REQUIRE( o + 2 <= b.size( ) );
        uint16_t v;
        std::memcpy( &v, b.data( ) + o, 2 );
        return v;
    }

    uint32_t rd32( const std::vector< uint8_t >& b, const std::size_t o )
    {
        REQUIRE( o + 4 <= b.size( ) );
        uint32_t v;
        std::memcpy( &v, b.data( ) + o, 4 );
        return v;
    }

    struct CSection
    {
        std::string raw_name;
        uint32_t    vsize { }, vaddr { }, size { }, data { }, relocs { }, lines { };
        uint16_t    nrelocs { }, nlines { };
        uint32_t    flags { };
    };

    struct CReloc
    {
        uint32_t offset { }, symbol { };
        uint16_t type { };
    };

    struct CSymbol
    {
        std::array< uint8_t, 8 >                 raw_name { };
        uint32_t                                 value { };
        int16_t                                  section { };
        uint16_t                                 type { };
        uint8_t                                  storage { }, naux { };
        std::vector< std::array< uint8_t, 18 > > aux { };
    };

    struct Coff
    {
        std::vector< uint8_t >  bytes;
        uint16_t                machine { }, nsections { }, opt_size { }, characteristics { };
        uint32_t                timestamp { }, symtab { }, nsyms { };
        std::vector< CSection > sections;
        std::vector< CSymbol >  symbols;
        uint32_t                strtab { };

        [[nodiscard]] uint32_t strtab_size( ) const
        { return rd32( bytes, strtab ); }

        [[nodiscard]] std::string str_at( const uint32_t off ) const
        {
            REQUIRE( off >= 4 );
            REQUIRE( off < strtab_size( ) );
            return std::string { reinterpret_cast< const char* >( bytes.data( ) + strtab + off ) };
        }

        [[nodiscard]] std::string section_name( const std::size_t i ) const
        {
            const auto& raw = sections[ i ].raw_name;
            if ( !raw.empty( ) && raw[ 0 ] == '/' ) return str_at( static_cast< uint32_t >( std::stoul( raw.substr( 1 ) ) ) );
            return raw;
        }

        [[nodiscard]] std::string symbol_name( const CSymbol& s ) const
        {
            uint32_t zero, off;
            std::memcpy( &zero, s.raw_name.data( ), 4 );
            std::memcpy( &off, s.raw_name.data( ) + 4, 4 );
            if ( zero == 0 ) return str_at( off );
            std::string n { reinterpret_cast< const char* >( s.raw_name.data( ) ), 8 };
            return n.substr( 0, n.find( '\0' ) );
        }

        [[nodiscard]] std::vector< CReloc > relocs( const std::size_t i ) const
        {
            std::vector< CReloc > out;
            const auto&           s = sections[ i ];
            for ( uint16_t k { 0 }; k < s.nrelocs; ++k )
            {
                const auto o = s.relocs + 10u * k;
                out.push_back( { rd32( bytes, o ), rd32( bytes, o + 4 ), rd16( bytes, o + 8 ) } );
            }
            return out;
        }

        [[nodiscard]] std::vector< uint8_t > raw( const std::size_t i ) const
        {
            const auto& s = sections[ i ];
            if ( s.data == 0 ) return { };
            REQUIRE( s.data + s.size <= bytes.size( ) );
            return { bytes.begin( ) + s.data, bytes.begin( ) + s.data + s.size };
        }
    };

    Coff parse( std::vector< uint8_t > b )
    {
        Coff c;
        c.bytes           = std::move( b );
        const auto& bytes = c.bytes;
        REQUIRE( bytes.size( ) >= 20 );
        c.machine         = rd16( bytes, 0 );
        c.nsections       = rd16( bytes, 2 );
        c.timestamp       = rd32( bytes, 4 );
        c.symtab          = rd32( bytes, 8 );
        c.nsyms           = rd32( bytes, 12 );
        c.opt_size        = rd16( bytes, 16 );
        c.characteristics = rd16( bytes, 18 );

        for ( uint16_t i { 0 }; i < c.nsections; ++i )
        {
            const std::size_t o = 20 + 40u * i;
            CSection          s;
            std::string       n { reinterpret_cast< const char* >( bytes.data( ) + o ), 8 };
            s.raw_name = n.substr( 0, n.find( '\0' ) );
            s.vsize    = rd32( bytes, o + 8 );
            s.vaddr    = rd32( bytes, o + 12 );
            s.size     = rd32( bytes, o + 16 );
            s.data     = rd32( bytes, o + 20 );
            s.relocs   = rd32( bytes, o + 24 );
            s.lines    = rd32( bytes, o + 28 );
            s.nrelocs  = rd16( bytes, o + 32 );
            s.nlines   = rd16( bytes, o + 34 );
            s.flags    = rd32( bytes, o + 36 );
            c.sections.push_back( s );
        }

        uint32_t i { 0 };
        while ( i < c.nsyms )
        {
            const std::size_t o = c.symtab + 18u * i;
            REQUIRE( o + 18 <= bytes.size( ) );
            CSymbol s;
            std::memcpy( s.raw_name.data( ), bytes.data( ) + o, 8 );
            s.value   = rd32( bytes, o + 8 );
            s.section = static_cast< int16_t >( rd16( bytes, o + 12 ) );
            s.type    = rd16( bytes, o + 14 );
            s.storage = bytes[ o + 16 ];
            s.naux    = bytes[ o + 17 ];
            for ( uint8_t a { 0 }; a < s.naux; ++a )
            {
                std::array< uint8_t, 18 > rec { };
                std::memcpy( rec.data( ), bytes.data( ) + o + 18 * ( a + 1 ), 18 );
                s.aux.push_back( rec );
            }
            i += 1 + s.naux;
            c.symbols.push_back( s );
        }
        REQUIRE( i == c.nsyms );
        c.strtab = c.symtab + 18 * c.nsyms;
        REQUIRE( c.strtab + 4 <= bytes.size( ) );
        REQUIRE( c.strtab + c.strtab_size( ) == bytes.size( ) );
        return c;
    }

    uint32_t aux_u32( const std::array< uint8_t, 18 >& a, const std::size_t o )
    {
        uint32_t v;
        std::memcpy( &v, a.data( ) + o, 4 );
        return v;
    }

    uint16_t aux_u16( const std::array< uint8_t, 18 >& a, const std::size_t o )
    {
        uint16_t v;
        std::memcpy( &v, a.data( ) + o, 2 );
        return v;
    }

    Coff write_parse( const object::Object& o )
    { return parse( object::coff::write( o ) ); }

    uint32_t align_bits( const uint32_t align )
    { return static_cast< uint32_t >( std::countr_zero( align ) + 1 ) << 20; }

    uint32_t append_bytes( object::Object& o, const uint32_t s, const std::vector< uint8_t >& b, const uint32_t align = 1 )
    { return o.append( s, b, align ); }
} // namespace

TEST_CASE( "coff empty object has a header and an empty string table", "[coff]" )
{
    const auto c = write_parse( object::Object { } );
    CHECK( c.bytes.size( ) == 24 );
    CHECK( c.machine == 0x8664 );
    CHECK( c.machine == object::coff::machine_amd64 );
    CHECK( c.nsections == 0 );
    CHECK( c.timestamp == 0 );
    CHECK( c.symtab == 20 );
    CHECK( c.nsyms == 0 );
    CHECK( c.opt_size == 0 );
    CHECK( c.characteristics == 0 );
    CHECK( c.strtab_size( ) == 4 );
}

TEST_CASE( "coff record size constants", "[coff]" )
{
    CHECK( object::coff::file_header_size == 20 );
    CHECK( object::coff::section_header_size == 40 );
    CHECK( object::coff::relocation_size == 10 );
    CHECK( object::coff::symbol_size == 18 );
}

TEST_CASE( "coff single text section with one function symbol", "[coff]" )
{
    object::Object o;
    const auto     t = o.add_section( ".text", SectionKind::Text, 16 );
    append_bytes( o, t, { 0x90, 0xC3 } );
    o.add_symbol( { .name = "f", .scope = SymbolScope::Global, .section = t, .value = 0, .is_function = true } );
    const auto c = write_parse( o );

    CHECK( c.nsections == 1 );
    REQUIRE( c.sections.size( ) == 1 );
    const auto& s = c.sections[ 0 ];
    CHECK( s.raw_name == ".text" );
    CHECK( s.vsize == 0 );
    CHECK( s.vaddr == 0 );
    CHECK( s.size == 2 );
    CHECK( s.data == 60 );
    CHECK( s.relocs == 0 );
    CHECK( s.lines == 0 );
    CHECK( s.nrelocs == 0 );
    CHECK( s.nlines == 0 );
    CHECK( s.flags == 0x60500020 );
    CHECK( c.raw( 0 ) == std::vector< uint8_t > { 0x90, 0xC3 } );

    CHECK( c.symtab == 64 );
    CHECK( c.nsyms == 3 );
    REQUIRE( c.symbols.size( ) == 2 );
    const auto& sec = c.symbols[ 0 ];
    CHECK( c.symbol_name( sec ) == ".text" );
    CHECK( sec.value == 0 );
    CHECK( sec.section == 1 );
    CHECK( sec.type == 0 );
    CHECK( sec.storage == object::coff::sym_class_static );
    CHECK( sec.naux == 1 );
    CHECK( aux_u32( sec.aux[ 0 ], 0 ) == 2 );
    CHECK( aux_u16( sec.aux[ 0 ], 4 ) == 0 );
    CHECK( aux_u16( sec.aux[ 0 ], 6 ) == 0 );
    CHECK( aux_u32( sec.aux[ 0 ], 8 ) == 0 );
    CHECK( aux_u16( sec.aux[ 0 ], 12 ) == 0 );
    CHECK( sec.aux[ 0 ][ 14 ] == 0 );

    const auto& f = c.symbols[ 1 ];
    CHECK( c.symbol_name( f ) == "f" );
    CHECK( f.value == 0 );
    CHECK( f.section == 1 );
    CHECK( f.type == 0x20 );
    CHECK( f.storage == object::coff::sym_class_external );
    CHECK( f.naux == 0 );
    CHECK( c.strtab_size( ) == 4 );
    CHECK( c.bytes.size( ) == 64 + 3 * 18 + 4 );
}

TEST_CASE( "coff section flags per kind", "[coff]" )
{
    object::Object o;
    const auto     t = o.add_section( ".text", SectionKind::Text, 16 );
    const auto     r = o.add_section( ".rdata", SectionKind::ReadOnlyData, 1 );
    const auto     d = o.add_section( ".data", SectionKind::Data, 8 );
    const auto     b = o.add_section( ".bss", SectionKind::UninitialisedData, 4 );
    append_bytes( o, t, { 0xC3 } );
    append_bytes( o, r, { 1 } );
    append_bytes( o, d, { 2 } );
    ( void ) o.reserve( b, 12, 4 );
    const auto c = write_parse( o );
    REQUIRE( c.sections.size( ) == 4 );
    CHECK( c.sections[ 0 ].flags == ( 0x00000020u | 0x20000000u | 0x40000000u | 0x00500000u ) );
    CHECK( c.sections[ 1 ].flags == ( 0x00000040u | 0x40000000u | 0x00100000u ) );
    CHECK( c.sections[ 2 ].flags == ( 0x00000040u | 0x40000000u | 0x80000000u | 0x00400000u ) );
    CHECK( c.sections[ 3 ].flags == ( 0x00000080u | 0x40000000u | 0x80000000u | 0x00300000u ) );
}

TEST_CASE( "coff section alignment bits cover every power of two up to 8192", "[coff]" )
{
    for ( uint32_t align = 1; align <= 8192; align <<= 1 )
        for ( const auto kind : { SectionKind::Text, SectionKind::ReadOnlyData, SectionKind::Data, SectionKind::UninitialisedData } )
        {
            const object::Section s { .name = "x", .kind = kind, .align = align };
            const auto            flags = object::coff::section_flags( s );
            CHECK( ( flags & 0x00F00000u ) == align_bits( align ) );
            CHECK( ( flags & 0x00F00000u ) >> 20 == static_cast< uint32_t >( std::countr_zero( align ) + 1 ) );
        }
    CHECK( align_bits( 1 ) == 0x00100000u );
    CHECK( align_bits( 2 ) == 0x00200000u );
    CHECK( align_bits( 4096 ) == 0x00D00000u );
    CHECK( align_bits( 8192 ) == 0x00E00000u );
}

TEST_CASE( "coff section flags constants", "[coff]" )
{
    using namespace object::coff;
    CHECK( scn_cnt_code == 0x20 );
    CHECK( scn_cnt_initialized_data == 0x40 );
    CHECK( scn_cnt_uninitialized_data == 0x80 );
    CHECK( scn_mem_execute == 0x20000000 );
    CHECK( scn_mem_read == 0x40000000 );
    CHECK( scn_mem_write == 0x80000000 );
    CHECK( rel_amd64_addr64 == 1 );
    CHECK( rel_amd64_rel32 == 4 );
    CHECK( sym_class_external == 2 );
    CHECK( sym_class_static == 3 );
    CHECK( sym_type_function == 0x20 );
}

TEST_CASE( "coff long section names go through the string table", "[coff]" )
{
    object::Object o;
    const auto     a = o.add_section( ".text$mn_long", SectionKind::Text, 1 );
    const auto     b = o.add_section( ".rdata$z", SectionKind::ReadOnlyData, 1 );
    const auto     d = o.add_section( ".data$another_long", SectionKind::Data, 1 );
    append_bytes( o, a, { 0xC3 } );
    append_bytes( o, b, { 1 } );
    append_bytes( o, d, { 2 } );
    const auto c = write_parse( o );
    CHECK( c.sections[ 0 ].raw_name == "/4" );
    CHECK( c.section_name( 0 ) == ".text$mn_long" );
    CHECK( c.sections[ 1 ].raw_name == ".rdata$z" );
    CHECK( c.section_name( 1 ) == ".rdata$z" );
    CHECK( c.sections[ 2 ].raw_name == std::format( "/{}", 4 + std::string_view { ".text$mn_long" }.size( ) + 1 ) );
    CHECK( c.section_name( 2 ) == ".data$another_long" );
    CHECK( c.symbol_name( c.symbols[ 0 ] ) == ".text$mn_long" );
    CHECK( c.symbol_name( c.symbols[ 1 ] ) == ".rdata$z" );
    CHECK( c.symbol_name( c.symbols[ 2 ] ) == ".data$another_long" );
    CHECK( c.strtab_size( ) == 4 + 14 + 19 );
}

TEST_CASE( "coff long symbol names are deduplicated", "[coff]" )
{
    object::Object o;
    const auto     t = o.add_section( ".text", SectionKind::Text, 1 );
    append_bytes( o, t, { 0xC3, 0xC3 } );
    o.add_symbol( { .name = "a_very_long_symbol_name", .scope = SymbolScope::Global, .section = t, .value = 0, .is_function = true } );
    o.add_symbol( { .name = "a_very_long_symbol_name", .scope = SymbolScope::Undefined } );
    o.add_symbol( { .name = "exactly8", .scope = SymbolScope::Local, .section = t, .value = 1 } );
    o.add_symbol( { .name = "another_long_name", .scope = SymbolScope::Undefined, .is_function = true } );
    const auto c = write_parse( o );
    REQUIRE( c.symbols.size( ) == 5 );
    uint32_t z1, o1, z2, o2;
    std::memcpy( &z1, c.symbols[ 1 ].raw_name.data( ), 4 );
    std::memcpy( &o1, c.symbols[ 1 ].raw_name.data( ) + 4, 4 );
    std::memcpy( &z2, c.symbols[ 2 ].raw_name.data( ), 4 );
    std::memcpy( &o2, c.symbols[ 2 ].raw_name.data( ) + 4, 4 );
    CHECK( z1 == 0 );
    CHECK( z2 == 0 );
    CHECK( o1 == 4 );
    CHECK( o2 == o1 );
    CHECK( c.symbol_name( c.symbols[ 1 ] ) == "a_very_long_symbol_name" );
    CHECK( c.symbol_name( c.symbols[ 3 ] ) == "exactly8" );
    CHECK( std::string_view { reinterpret_cast< const char* >( c.symbols[ 3 ].raw_name.data( ) ), 8 } == "exactly8" );
    CHECK( c.symbol_name( c.symbols[ 4 ] ) == "another_long_name" );
    CHECK( c.strtab_size( ) == 4 + 24 + 18 );
}

TEST_CASE( "coff symbol records encode scope section and type", "[coff]" )
{
    object::Object o;
    const auto     t = o.add_section( ".text", SectionKind::Text, 16 );
    const auto     d = o.add_section( ".data", SectionKind::Data, 8 );
    append_bytes( o, t, std::vector< uint8_t >( 40, 0xC3 ) );
    append_bytes( o, d, std::vector< uint8_t >( 16, 0 ) );
    o.add_symbol( { .name = "gfun", .scope = SymbolScope::Global, .section = t, .value = 32, .is_function = true } );
    o.add_symbol( { .name = "lfun", .scope = SymbolScope::Local, .section = t, .value = 16, .is_function = true } );
    o.add_symbol( { .name = "gdat", .scope = SymbolScope::Global, .section = d, .value = 8 } );
    o.add_symbol( { .name = "ldat", .scope = SymbolScope::Local, .section = d, .value = 4 } );
    o.add_symbol( { .name = "ufun", .scope = SymbolScope::Undefined, .section = d, .value = 99, .is_function = true } );
    o.add_symbol( { .name = "udat", .scope = SymbolScope::Undefined, .section = t, .value = 7 } );
    const auto c = write_parse( o );
    REQUIRE( c.symbols.size( ) == 8 );
    CHECK( c.nsyms == 2 * 2 + 6 );

    struct Want
    {
        std::string_view name;
        uint32_t         value;
        int16_t          section;
        uint16_t         type;
        uint8_t          storage;
    };

    const Want wants[] = {
        {"gfun", 32, 1, 0x20, 2},
        {"lfun", 16, 1, 0x20, 3},
        {"gdat",  8, 2,    0, 2},
        {"ldat",  4, 2,    0, 3},
        {"ufun",  0, 0, 0x20, 2},
        {"udat",  0, 0,    0, 2},
    };
    for ( std::size_t k = 0; k < 6; ++k )
    {
        const auto& s = c.symbols[ k + 2 ];
        const auto& w = wants[ k ];
        CHECK( c.symbol_name( s ) == w.name );
        CHECK( s.value == w.value );
        CHECK( s.section == w.section );
        CHECK( s.type == w.type );
        CHECK( s.storage == w.storage );
        CHECK( s.naux == 0 );
    }
    CHECK( c.symbol_name( c.symbols[ 0 ] ) == ".text" );
    CHECK( c.symbols[ 0 ].section == 1 );
    CHECK( c.symbol_name( c.symbols[ 1 ] ) == ".data" );
    CHECK( c.symbols[ 1 ].section == 2 );
    CHECK( aux_u32( c.symbols[ 1 ].aux[ 0 ], 0 ) == 16 );
}

TEST_CASE( "coff relocations store implicit addends and point past the section symbols", "[coff]" )
{
    object::Object o;
    const auto     t = o.add_section( ".text", SectionKind::Text, 16 );
    append_bytes( o, t, std::vector< uint8_t >( 24, 0 ) );
    const auto callee = o.add_symbol( { .name = "callee", .scope = SymbolScope::Undefined, .is_function = true } );
    const auto data   = o.add_symbol( { .name = "data", .scope = SymbolScope::Undefined } );
    o.add_reloc( t, { .offset = 1, .symbol = callee, .kind = RelocationKind::Rel32, .addend = -4 } );
    o.add_reloc( t, { .offset = 5, .symbol = data, .kind = RelocationKind::Rel32, .addend = 10 } );
    o.add_reloc( t, { .offset = 12, .symbol = data, .kind = RelocationKind::Abs64, .addend = 0x1122334455667788LL } );
    o.add_reloc( t, { .offset = 20, .symbol = callee, .kind = RelocationKind::Rel32, .addend = -100 } );
    const auto c = write_parse( o );

    const auto& s = c.sections[ 0 ];
    CHECK( s.data == 60 );
    CHECK( s.nrelocs == 4 );
    CHECK( s.relocs == 84 );
    CHECK( c.symtab == 124 );

    const auto raw = c.raw( 0 );
    CHECK( rd32( raw, 1 ) == 0 );
    CHECK( rd32( raw, 5 ) == 14 );
    uint64_t abs { };
    std::memcpy( &abs, raw.data( ) + 12, 8 );
    CHECK( abs == 0x1122334455667788ULL );
    CHECK( static_cast< int32_t >( rd32( raw, 20 ) ) == -96 );
    CHECK( raw[ 0 ] == 0 );
    CHECK( raw[ 9 ] == 0 );

    const auto rs = c.relocs( 0 );
    REQUIRE( rs.size( ) == 4 );
    CHECK( rs[ 0 ].offset == 1 );
    CHECK( rs[ 0 ].symbol == 2 + callee );
    CHECK( rs[ 0 ].type == object::coff::rel_amd64_rel32 );
    CHECK( rs[ 1 ].offset == 5 );
    CHECK( rs[ 1 ].symbol == 2 + data );
    CHECK( rs[ 2 ].type == object::coff::rel_amd64_addr64 );
    CHECK( rs[ 3 ].symbol == 2 + callee );
    CHECK( aux_u16( c.symbols[ 0 ].aux[ 0 ], 4 ) == 4 );
    CHECK( c.symbol_name( c.symbols[ rs[ 2 ].symbol - 1 ] ) == "data" );
}

TEST_CASE( "coff relocated_bytes leaves the section untouched", "[coff]" )
{
    object::Section s { .name = ".text", .kind = SectionKind::Text, .align = 1, .bytes = std::vector< uint8_t >( 16, 0xAA ) };
    s.relocs.push_back( { .offset = 0, .symbol = 0, .kind = RelocationKind::Rel32, .addend = -4 } );
    s.relocs.push_back( { .offset = 8, .symbol = 0, .kind = RelocationKind::Abs64, .addend = -1 } );
    const auto out = object::coff::relocated_bytes( s );
    CHECK( s.bytes == std::vector< uint8_t >( 16, 0xAA ) );
    REQUIRE( out.size( ) == 16 );
    CHECK( rd32( out, 0 ) == 0 );
    CHECK( out[ 4 ] == 0xAA );
    CHECK( out[ 7 ] == 0xAA );
    for ( std::size_t k = 8; k < 16; ++k ) CHECK( out[ k ] == 0xFF );
}

TEST_CASE( "coff relocated_bytes with no relocations is a copy", "[coff]" )
{
    const object::Section s {
        .name  = ".rdata",
        .kind  = SectionKind::ReadOnlyData,
        .bytes = { 1, 2, 3 }
    };
    CHECK( object::coff::relocated_bytes( s ) == s.bytes );
}

TEST_CASE( "coff bss has a size but no raw data", "[coff]" )
{
    object::Object o;
    const auto     b = o.add_section( ".bss", SectionKind::UninitialisedData, 8 );
    ( void ) o.reserve( b, 100, 8 );
    o.add_symbol( { .name = "zeros", .scope = SymbolScope::Global, .section = b, .value = 0 } );
    const auto c = write_parse( o );
    CHECK( c.sections[ 0 ].size == 100 );
    CHECK( c.sections[ 0 ].data == 0 );
    CHECK( c.sections[ 0 ].nrelocs == 0 );
    CHECK( c.symtab == 60 );
    CHECK( aux_u32( c.symbols[ 0 ].aux[ 0 ], 0 ) == 100 );
    CHECK( c.symbols[ 1 ].section == 1 );
    CHECK( c.bytes.size( ) == 60 + 3 * 18 + 4 );
}

TEST_CASE( "coff empty sections have no raw data pointer", "[coff]" )
{
    object::Object o;
    ( void ) o.add_section( ".text", SectionKind::Text, 16 );
    ( void ) o.add_section( ".data", SectionKind::Data, 8 );
    const auto c = write_parse( o );
    for ( const auto& s : c.sections )
    {
        CHECK( s.size == 0 );
        CHECK( s.data == 0 );
        CHECK( s.relocs == 0 );
    }
    CHECK( c.symtab == 100 );
    CHECK( c.nsyms == 4 );
}

TEST_CASE( "coff multiple sections are laid out in order with four byte alignment", "[coff]" )
{
    object::Object o;
    const auto     t = o.add_section( ".text", SectionKind::Text, 16 );
    const auto     r = o.add_section( ".rdata", SectionKind::ReadOnlyData, 1 );
    const auto     d = o.add_section( ".data", SectionKind::Data, 8 );
    const auto     b = o.add_section( ".bss", SectionKind::UninitialisedData, 8 );
    append_bytes( o, t, { 0x48, 0x31, 0xC0, 0xC3, 0x90 } );
    append_bytes( o, r, { 'h', 'i', 0, 0 } );
    append_bytes( o, d, std::vector< uint8_t >( 9, 7 ) );
    ( void ) o.reserve( b, 33, 8 );
    const auto sym = o.add_symbol( { .name = "x", .scope = SymbolScope::Undefined } );
    o.add_reloc( r, { .offset = 0, .symbol = sym, .kind = RelocationKind::Rel32, .addend = 0 } );
    o.add_reloc( d, { .offset = 1, .symbol = sym, .kind = RelocationKind::Abs64, .addend = 5 } );
    const auto c = write_parse( o );

    const uint32_t headers = 20 + 4 * 40;
    CHECK( c.sections[ 0 ].data == headers );
    CHECK( c.sections[ 1 ].data == ( ( headers + 5 + 3 ) & ~3u ) );
    CHECK( c.sections[ 1 ].relocs == c.sections[ 1 ].data + 4 );
    CHECK( c.sections[ 2 ].data == ( ( c.sections[ 1 ].relocs + 10 + 3 ) & ~3u ) );
    CHECK( c.sections[ 2 ].relocs == c.sections[ 2 ].data + 9 );
    CHECK( c.sections[ 3 ].data == 0 );
    CHECK( c.sections[ 3 ].size == 33 );
    CHECK( c.symtab == ( ( c.sections[ 2 ].relocs + 10 + 3 ) & ~3u ) );
    for ( const auto& s : c.sections )
        if ( s.data ) CHECK( s.data % 4 == 0 );

    CHECK( c.raw( 0 ) == std::vector< uint8_t > { 0x48, 0x31, 0xC0, 0xC3, 0x90 } );
    CHECK( rd32( c.raw( 1 ), 0 ) == 4 );
    std::vector< uint8_t > want_data( 9, 7 );
    const int64_t          five = 5;
    std::memcpy( want_data.data( ) + 1, &five, 8 );
    CHECK( c.raw( 2 ) == want_data );
    CHECK( c.relocs( 1 )[ 0 ].symbol == 8 );
    CHECK( c.relocs( 2 )[ 0 ].symbol == 8 );
    CHECK( c.relocs( 2 )[ 0 ].type == 1 );
    CHECK( c.nsyms == 9 );
    for ( std::size_t k = 0; k < 4; ++k )
    {
        CHECK( c.symbols[ k ].section == static_cast< int16_t >( k + 1 ) );
        CHECK( c.symbols[ k ].naux == 1 );
    }
}

TEST_CASE( "coff byte writer encodes little endian and pads", "[coff]" )
{
    object::coff::ByteWriter w;
    w.u8( 0xAB );
    w.u16( 0x1234 );
    w.u32( 0xDEADBEEF );
    CHECK( w.size( ) == 7 );
    w.zeros( 2 );
    w.pad_to( 12 );
    w.pad_to( 12 );
    w.name8( "abc" );
    w.name8( "12345678" );
    const uint8_t raw[] = { 9, 8 };
    w.bytes( raw );
    CHECK( w.size( ) == 30 );
    const auto                   out = std::move( w ).take( );
    const std::vector< uint8_t > want { 0xAB, 0x34, 0x12, 0xEF, 0xBE, 0xAD, 0xDE, 0,   0,   0,   0,   0,   'a', 'b', 'c',
                                        0,    0,    0,    0,    0,    '1',  '2',  '3', '4', '5', '6', '7', '8', 9,   8 };
    CHECK( out == want );
}

TEST_CASE( "coff string table offsets include the size field and deduplicate", "[coff]" )
{
    object::coff::StringTable t;
    CHECK( t.add( "first_long_name" ) == 4 );
    CHECK( t.add( "second" ) == 4 + 16 );
    CHECK( t.add( "first_long_name" ) == 4 );
    CHECK( t.add( "" ) == 4 + 16 + 7 );
    CHECK( t.add( "" ) == 4 + 16 + 7 );
    object::coff::ByteWriter w;
    t.write( w );
    const auto out = std::move( w ).take( );
    REQUIRE( out.size( ) == 4 + 16 + 7 + 1 );
    CHECK( rd32( out, 0 ) == out.size( ) );
    CHECK( std::string { reinterpret_cast< const char* >( out.data( ) + 4 ) } == "first_long_name" );
    CHECK( std::string { reinterpret_cast< const char* >( out.data( ) + 20 ) } == "second" );
    CHECK( out.back( ) == 0 );
}

TEST_CASE( "coff empty string table is just its size", "[coff]" )
{
    const object::coff::StringTable t;
    object::coff::ByteWriter        w;
    t.write( w );
    CHECK( std::move( w ).take( ) == std::vector< uint8_t > { 4, 0, 0, 0 } );
}

TEST_CASE( "coff output of an object module round trips its symbols", "[coff]" )
{
    ir::IRModule         irm;
    object::ObjectModule mod {
        object::ObjectBuilder { isa::lookup_host( ), "rt" }
    };
    const auto ext = *mod.declare_function( "external_function_with_long_name", module::Linkage::Import, { { Type::i64 }, Type::i64 } );
    const auto msg = *mod.declare_data( "msg", module::Linkage::Local, false );
    const auto fid = *mod.declare_function( "entry_point", module::Linkage::Export, { { Type::i64 }, Type::i64 } );
    {
        module::DataDescription d;
        d.define( std::string_view { "abc" } );
        REQUIRE( mod.define_data( msg, d ).has_value( ) );
    }
    const auto ref = irm.declare( "entry_point", Type::i64, { Type::i64 } );
    {
        ir::IRBuilder b { *irm.get( ref ) };
        const auto    e  = mod.declare_func_in_func( ext, b );
        const auto    gv = mod.declare_data_in_func( msg, b );
        b.switch_to_block( b.add_block( { Type::i64 } ) );
        const auto p = b.symbol_value( Type::ptr, gv );
        b.ret( b.iadd( b.call( e, Args { b.param( 0 ) } ), b.uextend( Type::i64, b.load( Type::i8, p ) ) ) );
        b.finalise( );
    }
    codegen::Context ctx { *irm.get( ref ) };
    REQUIRE( mod.define_function( fid, ctx ).has_value( ) );
    auto product = std::move( mod ).finish( );
    REQUIRE( product.has_value( ) );
    const auto c = parse( product->emit( ) );

    REQUIRE( c.nsections == 2 );
    CHECK( c.section_name( 0 ) == ".rdata" );
    CHECK( c.section_name( 1 ) == ".text" );
    CHECK( c.raw( 0 ) == std::vector< uint8_t > { 'a', 'b', 'c' } );
    CHECK( c.sections[ 1 ].nrelocs == 2 );
    REQUIRE( c.symbols.size( ) == 5 );
    CHECK( c.symbol_name( c.symbols[ 2 ] ) == "entry_point" );
    CHECK( c.symbols[ 2 ].storage == 2 );
    CHECK( c.symbols[ 2 ].type == 0x20 );
    CHECK( c.symbols[ 2 ].section == 2 );
    CHECK( c.symbol_name( c.symbols[ 3 ] ) == "msg" );
    CHECK( c.symbols[ 3 ].storage == 3 );
    CHECK( c.symbols[ 3 ].section == 1 );
    CHECK( c.symbol_name( c.symbols[ 4 ] ) == "external_function_with_long_name" );
    CHECK( c.symbols[ 4 ].section == 0 );
    for ( const auto& r : c.relocs( 1 ) )
    {
        CHECK( r.symbol >= 4 );
        CHECK( r.symbol < c.nsyms );
        if ( r.type == object::coff::rel_amd64_rel32 )
        {
            CHECK( r.symbol == 5 );
            CHECK( rd32( c.raw( 1 ), r.offset ) == 0 );
        }
        else
            CHECK( r.symbol == 6 );
    }
}

TEST_CASE( "coff object links into a runnable executable", "[coff][link]" )
{
#if !defined( _WIN32 )
    SKIP( "a coff object only links into an executable on windows" );
#endif
#if defined( _WIN32 )
    constexpr std::string_view null_device = "NUL";
#else
    constexpr std::string_view null_device = "/dev/null";
#endif
    if ( std::system( std::format( "clang --version > {} 2>&1", null_device ).c_str( ) ) != 0 ) SKIP( "clang is not available" );

    ir::IRModule         irm;
    object::ObjectModule mod {
        object::ObjectBuilder { isa::lookup_host( ), "linktest" }
    };
    constexpr auto cc = codegen::host_call_conv( );

    const auto strlen_id = *mod.declare_function( "strlen", module::Linkage::Import, { { Type::ptr }, Type::i64, cc } );
    const auto helper_id = *mod.declare_function( "xiao_link_helper", module::Linkage::Local, { { Type::i64 }, Type::i64, cc } );
    const auto main_id   = *mod.declare_function( "main", module::Linkage::Export, { { }, Type::i32, cc } );
    const auto msg_id    = *mod.declare_data( "xiao_link_message", module::Linkage::Local, false );
    const auto w_id      = *mod.declare_data( "xiao_link_counter", module::Linkage::Local, true );
    const auto z_id      = *mod.declare_data( "xiao_link_zeros", module::Linkage::Export, true );

    {
        module::DataDescription d;
        d.define( std::string_view { "xiao!!\0", 7 } );
        REQUIRE( mod.define_data( msg_id, d ).has_value( ) );
    }
    {
        module::DataDescription d;
        const int64_t           thirty = 30;
        d.define( std::span { reinterpret_cast< const uint8_t* >( &thirty ), 8 } );
        REQUIRE( mod.define_data( w_id, d ).has_value( ) );
    }
    {
        module::DataDescription d;
        d.define_zeroinit( 64 );
        REQUIRE( mod.define_data( z_id, d ).has_value( ) );
    }

    const auto helper_ref = irm.declare( "xiao_link_helper", Type::i64, { Type::i64 }, cc );
    {
        ir::IRBuilder b { *irm.get( helper_ref ) };
        b.switch_to_block( b.add_block( { Type::i64 } ) );
        b.ret( b.imul( b.param( 0 ), b.iconst( Type::i64, 2 ) ) );
        b.finalise( );
    }
    const auto main_ref = irm.declare( "main", Type::i32, { }, cc );
    {
        ir::IRBuilder b { *irm.get( main_ref ) };
        const auto    sl  = mod.declare_func_in_func( strlen_id, b );
        const auto    hp  = mod.declare_func_in_func( helper_id, b );
        const auto    msg = mod.declare_data_in_func( msg_id, b );
        const auto    w   = mod.declare_data_in_func( w_id, b );
        const auto    z   = mod.declare_data_in_func( z_id, b );
        b.switch_to_block( b.add_block( { } ) );
        const auto len = b.call( sl, Args { b.symbol_value( Type::ptr, msg ) } );
        const auto dbl = b.call( hp, Args { len } );
        const auto wv  = b.load( Type::i64, b.symbol_value( Type::ptr, w ) );
        const auto zv  = b.load( Type::i64, b.symbol_value( Type::ptr, z ), 56 );
        b.ret( b.ireduce( Type::i32, b.iadd( b.iadd( dbl, wv ), zv ) ) );
        b.finalise( );
    }
    for ( const auto ref : { helper_ref, main_ref } ) REQUIRE( verify( *irm.get( ref ) ).empty( ) );

    codegen::Context hctx { *irm.get( helper_ref ) };
    codegen::Context mctx { *irm.get( main_ref ) };
    REQUIRE( mod.define_function( helper_id, hctx ).has_value( ) );
    REQUIRE( mod.define_function( main_id, mctx ).has_value( ) );
    auto product = std::move( mod ).finish( );
    REQUIRE( product.has_value( ) );

    const auto dir = std::filesystem::temp_directory_path( ) / "xiao_link_test";
    std::filesystem::create_directories( dir );
    const auto obj = dir / "linktest.obj";
    const auto exe = dir /
#if defined( _WIN32 )
                     "linktest.exe";
#else
                     "linktest";
#endif
    std::filesystem::remove( exe );
    REQUIRE( product->write( obj ) );

    const auto link = std::format( "clang -fuse-ld=lld \"{}\" -o \"{}\" > {} 2>&1", obj.string( ), exe.string( ), null_device );
    REQUIRE( std::system( link.c_str( ) ) == 0 );
    REQUIRE( std::filesystem::exists( exe ) );

#if defined( _WIN32 )
    const auto rc = std::system( std::format( "\"\"{}\"\"", exe.string( ) ).c_str( ) );
#else
    const auto raw = std::system( std::format( "\"{}\"", exe.string( ) ).c_str( ) );
    const auto rc  = WIFEXITED( raw ) ? WEXITSTATUS( raw ) : -1;
#endif
    CHECK( rc == 42 );
    std::filesystem::remove_all( dir );
}

namespace
{
    object::Object object_with_relocs( const uint32_t n, uint32_t& callee )
    {
        object::Object o;
        const auto     t = o.add_section( ".text", SectionKind::Text, 16 );
        append_bytes( o, t, std::vector< uint8_t >( 4 * n, 0 ) );
        callee = o.add_symbol( { .name = "callee", .scope = SymbolScope::Undefined, .is_function = true } );
        for ( uint32_t i { 0 }; i < n; ++i )
            o.add_reloc( t, { .offset = 4 * i, .symbol = callee, .kind = RelocationKind::Rel32, .addend = -4 } );
        return o;
    }
} // namespace

TEST_CASE( "coff sections with 65535 or more relocations use the overflow record", "[coff]" )
{
    const uint32_t n = GENERATE( 65535u, 70000u );
    uint32_t       callee { };
    const auto     bytes = object::coff::write( object_with_relocs( n, callee ) );
    const auto     c     = parse( bytes );
    const auto&    s     = c.sections[ 0 ];

    CHECK( s.nrelocs == 0xFFFF );
    CHECK( ( s.flags & object::coff::scn_lnk_nreloc_ovfl ) != 0 );
    CHECK( rd32( bytes, s.relocs ) == n + 1 );
    CHECK( rd32( bytes, s.relocs + 4 ) == 0 );
    CHECK( rd16( bytes, s.relocs + 8 ) == 0 );
    CHECK( rd32( bytes, s.relocs + 10 ) == 0 );
    CHECK( rd32( bytes, s.relocs + 14 ) == 2 + callee );
    CHECK( rd32( bytes, s.relocs + 10 * n ) == 4 * ( n - 1 ) );
    CHECK( aux_u16( c.symbols[ 0 ].aux[ 0 ], 4 ) == 0xFFFF );
}

TEST_CASE( "coff sections with 65534 relocations do not overflow", "[coff]" )
{
    uint32_t    callee { };
    const auto  bytes = object::coff::write( object_with_relocs( 65534, callee ) );
    const auto  c     = parse( bytes );
    const auto& s     = c.sections[ 0 ];

    CHECK( s.nrelocs == 65534 );
    CHECK( ( s.flags & object::coff::scn_lnk_nreloc_ovfl ) == 0 );
    CHECK( rd32( bytes, s.relocs ) == 0 );
    CHECK( aux_u16( c.symbols[ 0 ].aux[ 0 ], 4 ) == 65534 );
}