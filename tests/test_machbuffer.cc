#include "support.hh"

#include <xiao/codegen/machbuffer.hh>
#include <xiao/isa/x86/inst.hh>

#include <vector>

using namespace xt;
using x86_64::LabelUse;

namespace
{
    using Buffer = codegen::MachBuffer< LabelUse >;

    constexpr LabelUse rel32 { LabelUse::Kind::JmpRel32 };

    [[nodiscard]] std::vector< uint8_t > bytes_of( const Buffer& buf )
    { return { buf.data( ).begin( ), buf.data( ).end( ) }; }

    [[nodiscard]] int32_t read_i32( const std::span< const uint8_t > b, const std::size_t at )
    {
        int32_t v { };
        std::memcpy( &v, b.data( ) + at, 4 );
        return v;
    }
}

TEST_CASE( "machbuffer starts empty", "[machbuffer]" )
{
    const Buffer buf;
    CHECK( buf.size( ) == 0 );
    CHECK( buf.current_offset( ) == 0 );
    CHECK( buf.data( ).empty( ) );
}

TEST_CASE( "machbuffer emits single bytes", "[machbuffer]" )
{
    Buffer buf;
    buf.emit_u8( 0x00 );
    buf.emit_u8( 0x7f );
    buf.emit_u8( 0x80 );
    buf.emit_u8( 0xff );
    CHECK( bytes_of( buf ) == std::vector< uint8_t > { 0x00, 0x7f, 0x80, 0xff } );
    CHECK( buf.size( ) == 4 );
    CHECK( buf.current_offset( ) == 4 );
}

TEST_CASE( "machbuffer emits u16 little endian", "[machbuffer]" )
{
    Buffer buf;
    buf.emit_u16( 0x1234 );
    buf.emit_u16( 0xffff );
    buf.emit_u16( 0x0000 );
    buf.emit_u16( 0x00ff );
    CHECK( bytes_of( buf ) == std::vector< uint8_t > { 0x34, 0x12, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00 } );
}

TEST_CASE( "machbuffer emits u32 little endian", "[machbuffer]" )
{
    Buffer buf;
    buf.emit_u32( 0x12345678 );
    buf.emit_u32( 0xdeadbeef );
    CHECK( bytes_of( buf ) == std::vector< uint8_t > { 0x78, 0x56, 0x34, 0x12, 0xef, 0xbe, 0xad, 0xde } );
}

TEST_CASE( "machbuffer emits i32 two's complement", "[machbuffer]" )
{
    Buffer buf;
    buf.emit_i32( -1 );
    buf.emit_i32( -2 );
    buf.emit_i32( std::numeric_limits< int32_t >::min( ) );
    buf.emit_i32( std::numeric_limits< int32_t >::max( ) );
    CHECK( bytes_of( buf )
           == std::vector< uint8_t > { 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f } );
}

TEST_CASE( "machbuffer emits u64 little endian", "[machbuffer]" )
{
    Buffer buf;
    buf.emit_u64( 0x0123456789abcdefULL );
    buf.emit_u64( 0 );
    buf.emit_u64( ~0ULL );
    CHECK( bytes_of( buf ) == std::vector< uint8_t > { 0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01, 0,    0,    0,    0,
                                                       0,    0,    0,    0,    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } );
}

TEST_CASE( "machbuffer emit_bytes appends spans", "[machbuffer]" )
{
    Buffer                       buf;
    const std::vector< uint8_t > a { 1, 2, 3 };
    const std::vector< uint8_t > empty { };
    buf.emit_u8( 0xaa );
    buf.emit_bytes( a );
    buf.emit_bytes( empty );
    buf.emit_bytes( a );
    CHECK( bytes_of( buf ) == std::vector< uint8_t > { 0xaa, 1, 2, 3, 1, 2, 3 } );
}

TEST_CASE( "machbuffer clear empties bytes", "[machbuffer]" )
{
    Buffer buf;
    buf.emit_u64( 42 );
    buf.clear( );
    CHECK( buf.size( ) == 0 );
    buf.emit_u8( 9 );
    CHECK( bytes_of( buf ) == std::vector< uint8_t > { 9 } );
}

TEST_CASE( "machbuffer initial capacity does not change size", "[machbuffer]" )
{
    const Buffer small { 0 };
    const Buffer big { 1 << 16 };
    CHECK( small.size( ) == 0 );
    CHECK( big.size( ) == 0 );
}

TEST_CASE( "machbuffer patch_i32 overwrites in place", "[machbuffer]" )
{
    Buffer buf;
    buf.emit_u8( 0x90 );
    buf.emit_u32( 0 );
    buf.emit_u8( 0xc3 );
    buf.patch_i32( 1, -5 );
    CHECK( bytes_of( buf ) == std::vector< uint8_t > { 0x90, 0xfb, 0xff, 0xff, 0xff, 0xc3 } );
    buf.patch_i32( 1, 0x11223344 );
    CHECK( bytes_of( buf ) == std::vector< uint8_t > { 0x90, 0x44, 0x33, 0x22, 0x11, 0xc3 } );
    CHECK( buf.size( ) == 6 );
}

TEST_CASE( "machbuffer new_label hands out sequential ids", "[machbuffer][labels]" )
{
    Buffer buf;
    CHECK( buf.new_label( ) == 0 );
    CHECK( buf.new_label( ) == 1 );
    CHECK( buf.new_label( ) == 2 );
}

TEST_CASE( "machbuffer forward label reference", "[machbuffer][labels]" )
{
    Buffer     buf;
    const auto l = buf.new_label( );
    buf.emit_u8( 0xe9 );
    buf.emit_label_ref( l, rel32 );
    CHECK( buf.size( ) == 5 );
    CHECK( read_i32( buf.data( ), 1 ) == 0 );
    buf.emit_u8( 0x90 );
    buf.emit_u8( 0x90 );
    buf.bind_label( l );
    buf.emit_u8( 0xc3 );
    buf.resolve_labels( );
    CHECK( read_i32( buf.data( ), 1 ) == 2 );
}

TEST_CASE( "machbuffer backward label reference", "[machbuffer][labels]" )
{
    Buffer     buf;
    const auto l = buf.new_label( );
    buf.bind_label( l );
    buf.emit_u8( 0x90 );
    buf.emit_u8( 0xe9 );
    buf.emit_label_ref( l, rel32 );
    buf.resolve_labels( );
    CHECK( read_i32( buf.data( ), 2 ) == -6 );
}

TEST_CASE( "machbuffer self loop label", "[machbuffer][labels]" )
{
    Buffer     buf;
    const auto l = buf.new_label( );
    buf.bind_label( l );
    buf.emit_u8( 0xe9 );
    buf.emit_label_ref( l, rel32 );
    buf.resolve_labels( );
    CHECK( bytes_of( buf ) == std::vector< uint8_t > { 0xe9, 0xfb, 0xff, 0xff, 0xff } );
}

TEST_CASE( "machbuffer several fixups to one label", "[machbuffer][labels]" )
{
    Buffer     buf;
    const auto l = buf.new_label( );
    buf.emit_u8( 0xe9 );
    buf.emit_label_ref( l, rel32 );
    buf.emit_u8( 0x0f );
    buf.emit_u8( 0x84 );
    buf.emit_label_ref( l, rel32 );
    buf.bind_label( l );
    buf.emit_u8( 0xe9 );
    buf.emit_label_ref( l, rel32 );
    buf.resolve_labels( );
    CHECK( read_i32( buf.data( ), 1 ) == 6 );
    CHECK( read_i32( buf.data( ), 7 ) == 0 );
    CHECK( read_i32( buf.data( ), 12 ) == -5 );
}

TEST_CASE( "machbuffer label rebinding uses the last binding", "[machbuffer][labels]" )
{
    Buffer     buf;
    const auto l = buf.new_label( );
    buf.bind_label( l );
    buf.emit_u8( 0xe9 );
    buf.emit_label_ref( l, rel32 );
    buf.emit_u8( 0x90 );
    buf.bind_label( l );
    buf.resolve_labels( );
    CHECK( read_i32( buf.data( ), 1 ) == 1 );
}

TEST_CASE( "machbuffer independent labels", "[machbuffer][labels]" )
{
    Buffer     buf;
    const auto a = buf.new_label( );
    const auto b = buf.new_label( );
    buf.emit_u8( 0xe9 );
    buf.emit_label_ref( b, rel32 );
    buf.bind_label( a );
    buf.emit_u8( 0xe9 );
    buf.emit_label_ref( a, rel32 );
    buf.bind_label( b );
    buf.resolve_labels( );
    CHECK( read_i32( buf.data( ), 1 ) == 5 );
    CHECK( read_i32( buf.data( ), 6 ) == -5 );
}

TEST_CASE( "machbuffer resolve clears fixups", "[machbuffer][labels]" )
{
    Buffer     buf;
    const auto l = buf.new_label( );
    buf.emit_u8( 0xe9 );
    buf.emit_label_ref( l, rel32 );
    buf.bind_label( l );
    buf.resolve_labels( );
    CHECK( read_i32( buf.data( ), 1 ) == 0 );
    buf.patch_i32( 1, 1234 );
    buf.resolve_labels( );
    CHECK( read_i32( buf.data( ), 1 ) == 1234 );
}

TEST_CASE( "machbuffer add_reloc records the current offset", "[machbuffer][relocs]" )
{
    Buffer buf;
    buf.emit_u8( 0xe8 );
    buf.add_reloc( codegen::RelocKind::X86CallPCRel4, ir::ExternalName { 0, 3 }, -4 );
    buf.emit_u32( 0 );
    buf.emit_u8( 0x48 );
    buf.emit_u8( 0xb8 );
    buf.add_reloc( codegen::RelocKind::Abs8, ir::ExternalName { 1, 9 }, 16 );
    buf.emit_u64( 0 );

    auto code = std::move( buf ).finish( );
    CHECK( code.code.size( ) == 15 );
    REQUIRE( code.relocs.size( ) == 2 );
    CHECK( code.relocs[ 0 ].offset == 1 );
    CHECK( code.relocs[ 0 ].kind == codegen::RelocKind::X86CallPCRel4 );
    CHECK( code.relocs[ 0 ].target == ir::ExternalName { 0, 3 } );
    CHECK( code.relocs[ 0 ].addend == -4 );
    CHECK( code.relocs[ 1 ].offset == 7 );
    CHECK( code.relocs[ 1 ].kind == codegen::RelocKind::Abs8 );
    CHECK( code.relocs[ 1 ].target == ir::ExternalName { 1, 9 } );
    CHECK( code.relocs[ 1 ].addend == 16 );
}

TEST_CASE( "machbuffer finish resolves labels and keeps default alignment", "[machbuffer]" )
{
    Buffer     buf;
    const auto l = buf.new_label( );
    buf.emit_u8( 0xe9 );
    buf.emit_label_ref( l, rel32 );
    buf.emit_u8( 0xcc );
    buf.bind_label( l );
    buf.emit_u8( 0xc3 );
    const auto code = std::move( buf ).finish( );
    CHECK( code.code == std::vector< uint8_t > { 0xe9, 0x01, 0x00, 0x00, 0x00, 0xcc, 0xc3 } );
    CHECK( code.relocs.empty( ) );
    CHECK( code.alignment == 16 );
}

TEST_CASE( "machbuffer finish on empty buffer", "[machbuffer]" )
{
    const auto code = Buffer { }.finish( );
    CHECK( code.code.empty( ) );
    CHECK( code.relocs.empty( ) );
}

TEST_CASE( "x86 label use encodes rel32 from the end of the field", "[machbuffer][labels][x86]" )
{
    CHECK( LabelUse::encode( rel32, 0, 4 ) == 0 );
    CHECK( LabelUse::encode( rel32, 0, 0 ) == -4 );
    CHECK( LabelUse::encode( rel32, 1, 5 ) == 0 );
    CHECK( LabelUse::encode( rel32, 1, 100 ) == 95 );
    CHECK( LabelUse::encode( rel32, 100, 1 ) == -103 );
    CHECK( LabelUse::encode( rel32, 1000000, 0 ) == -1000004 );
}

TEST_CASE( "x86 label use satisfies the label use concept", "[machbuffer][x86]" )
{
    STATIC_CHECK( codegen::LabelUseC< LabelUse > );
    CHECK( rel32.kind == LabelUse::Kind::JmpRel32 );
}
