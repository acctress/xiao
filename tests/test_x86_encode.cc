#include "support.hh"

#include <xiao/codegen/machbuffer.hh>
#include <xiao/isa/x86/emit.hh>
#include <xiao/isa/x86/encode.hh>
#include <xiao/isa/x86/frame.hh>
#include <xiao/isa/x86/inst.hh>
#include <xiao/isa/x86/reg.hh>

#include <Zydis.h>

#include <array>
#include <format>
#include <string>
#include <vector>

using namespace xt;
using namespace xiao::x86_64;
using codegen::PReg;
using codegen::Reg;
using codegen::RegClass;
using Bytes = std::vector< uint8_t >;

namespace
{
    using Buffer = codegen::MachBuffer< LabelUse >;

    constexpr std::array< PReg, 16 > gprs = { RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 };

    constexpr std::array< const char*, 16 > q_names
        = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
    constexpr std::array< const char*, 16 > d_names
        = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };
    constexpr std::array< const char*, 16 > w_names
        = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di", "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w" };
    constexpr std::array< const char*, 16 > b_names
        = { "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil", "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" };

    constexpr std::array< CC, 10 > all_ccs = { CC::E, CC::NE, CC::L, CC::LE, CC::G, CC::GE, CC::B, CC::BE, CC::A, CC::AE };

    constexpr std::array< int, 4 > all_sizes = { 8, 16, 32, 64 };

    [[nodiscard]] Reg r( const PReg p )
    { return Reg::from_preg( p ); }

    [[nodiscard]] GprMem rm( const PReg p )
    { return GprMem { Reg::from_preg( p ) }; }

    [[nodiscard]] GprMem mem( const AddressMode& m )
    { return GprMem { m }; }

    [[nodiscard]] std::string q( const PReg p )
    { return q_names[ p.hw( ) ]; }

    [[nodiscard]] std::string sized( const PReg p, const int bits )
    {
        switch ( bits )
        {
            case 8: return b_names[ p.hw( ) ];
            case 16: return w_names[ p.hw( ) ];
            case 32: return d_names[ p.hw( ) ];
            case 64: return q_names[ p.hw( ) ];
            default: return "?";
        }
    }

    [[nodiscard]] std::string cc_suffix( const CC cc )
    {
        switch ( cc )
        {
            case CC::E: return "z";
            case CC::NE: return "nz";
            case CC::L: return "l";
            case CC::LE: return "le";
            case CC::G: return "nle";
            case CC::GE: return "nl";
            case CC::B: return "b";
            case CC::BE: return "be";
            case CC::A: return "nbe";
            case CC::AE: return "nb";
        }
        return "?";
    }

    [[nodiscard]] std::string hex( const uint64_t v )
    { return std::format( "0x{:02X}", v ); }

    [[nodiscard]] std::string sx( const int64_t v )
    { return hex( static_cast< uint64_t >( v ) ); }

    [[nodiscard]] std::string disp_text( const int32_t d )
    {
        if ( d > 0 ) return std::format( "+0x{:02X}", d );
        if ( d < 0 ) return std::format( "-0x{:02X}", -static_cast< int64_t >( d ) );
        return "";
    }

    [[nodiscard]] std::string mem_text( const PReg base, const PReg index, const uint8_t scale, const int32_t disp )
    {
        std::string s = "[" + q( base );
        if ( index.valid( ) ) s += std::format( "+{}*{}", q( index ), 1 << scale );
        return s + disp_text( disp ) + "]";
    }

    [[nodiscard]] Bytes enc( const X86Inst& inst, const std::vector< PReg >& pregs, const std::vector< int32_t >& slot_offsets = { } )
    {
        Buffer buf;
        auto   rewritten = inst;
        rewritten.apply_alloc( pregs );
        rewritten.emit( buf, slot_offsets );
        return { buf.data( ).begin( ), buf.data( ).end( ) };
    }

    template < typename I >
    [[nodiscard]] Bytes raw( const I& inst )
    {
        Buffer buf;
        inst.emit( buf );
        return { buf.data( ).begin( ), buf.data( ).end( ) };
    }

    [[nodiscard]] Bytes take( const Buffer& buf )
    { return { buf.data( ).begin( ), buf.data( ).end( ) }; }

    struct Decoded
    {
        bool        ok { };
        std::size_t length { };
        std::string text { };
    };

    [[nodiscard]] Decoded decode( const Bytes& b )
    {
        ZydisDisassembledInstruction d { };
        if ( b.empty( ) ) return { };
        const bool ok = ZYAN_SUCCESS( ZydisDisassembleIntel( ZYDIS_MACHINE_MODE_LONG_64, 0, b.data( ), b.size( ), &d ) );
        return { ok, ok ? std::size_t { d.info.length } : std::size_t { 0 }, ok ? std::string { d.text } : std::string { } };
    }

    [[nodiscard]] std::vector< std::string > decode_all( const Bytes& b )
    {
        std::vector< std::string > out;
        std::size_t                off { 0 };
        while ( off < b.size( ) )
        {
            ZydisDisassembledInstruction d { };
            if ( !ZYAN_SUCCESS( ZydisDisassembleIntel( ZYDIS_MACHINE_MODE_LONG_64, off, b.data( ) + off, b.size( ) - off, &d ) ) )
            {
                out.push_back( "(bad)" );
                break;
            }
            out.emplace_back( d.text );
            off += d.info.length;
        }
        return out;
    }

    struct Sweep
    {
        std::size_t checked { };
        std::size_t failures { };

        void check( const X86Inst& inst, const std::vector< PReg >& pregs, const std::string& expected,
                    const std::vector< int32_t >& slot_offsets = { } )
        { check_bytes( enc( inst, pregs, slot_offsets ), expected ); }

        template < typename I >
        void check_raw( const I& inst, const std::string& expected )
        { check_bytes( raw( inst ), expected ); }

        void check_bytes( const Bytes& b, const std::string& expected )
        {
            ++checked;
            const auto d = decode( b );
            if ( d.ok && d.length == b.size( ) && d.text == expected ) return;
            ++failures;
            if ( failures > 20 ) return;
            std::string hexed;
            for ( const auto x : b ) hexed += std::format( "{:02x} ", x );
            UNSCOPED_INFO(
                std::format( "expected '{}' got '{}' ok={} len={}/{} bytes={}", expected, d.text, d.ok, d.length, b.size( ), hexed ) );
            CHECK( false );
        }
    };

    const std::vector< int32_t > displacements
        = { 0, 1, -1, 8, -8, 127, -128, 128, -129, 0x100, -0x100, 0x7fffffff, std::numeric_limits< int32_t >::min( ) };

    [[nodiscard]] X86Inst load_of( const PReg d, const AddressMode& m, const int bits )
    {
        switch ( bits )
        {
            case 8: return inst::movzxq_rm8 { r( d ), mem( m ) };
            case 16: return inst::movzxq_rm16 { r( d ), mem( m ) };
            case 32: return inst::movl_rm { r( d ), mem( m ) };
            default: return inst::movq_rm { r( d ), mem( m ) };
        }
    }

    [[nodiscard]] X86Inst store_of( const AddressMode& m, const PReg src, const int bits )
    {
        switch ( bits )
        {
            case 8: return inst::movb_mr { mem( m ), r( src ) };
            case 16: return inst::movw_mr { mem( m ), r( src ) };
            case 32: return inst::movl_mr { mem( m ), r( src ) };
            default: return inst::movq_mr { mem( m ), r( src ) };
        }
    }

    [[nodiscard]] X86Inst test_of( const PReg d, const int bits )
    {
        switch ( bits )
        {
            case 8: return inst::testb_mr { rm( d ), r( d ) };
            case 16: return inst::testw_mr { rm( d ), r( d ) };
            case 32: return inst::testl_mr { rm( d ), r( d ) };
            default: return inst::testq_mr { rm( d ), r( d ) };
        }
    }

    template < typename Imm8, typename Imm32 >
    [[nodiscard]] X86Inst alu_imm( const PReg d, const int32_t imm )
    {
        if ( fits_i8( imm ) ) return Imm8 { rm( d ), static_cast< int8_t >( imm ) };
        return Imm32 { rm( d ), imm };
    }

    [[nodiscard]] X86Inst imul_imm( const PReg d, const int32_t imm )
    {
        if ( fits_i8( imm ) ) return inst::imulq_rmi8 { r( d ), rm( d ), static_cast< int8_t >( imm ) };
        return inst::imulq_rmi32 { r( d ), rm( d ), imm };
    }
} // namespace

TEST_CASE( "encode modrm packs fields", "[x86][encode][helpers]" )
{
    CHECK( modrm( 0b11, 0, 0 ) == 0xC0 );
    CHECK( modrm( 0b11, 7, 7 ) == 0xFF );
    CHECK( modrm( 0b00, 0, 5 ) == 0x05 );
    CHECK( modrm( 0b01, 7, 4 ) == 0x7C );
    CHECK( modrm( 0b10, 2, 1 ) == 0x91 );
    CHECK( modrm( 0b11, 15, 9 ) == 0xF9 );
    CHECK( modrm( 0b11, 8, 8 ) == 0xC0 );
    STATIC_CHECK( modrm( 0b01, 0, 5 ) == 0x45 );
}

TEST_CASE( "encode needs_rex_for_byte only for spl bpl sil dil", "[x86][encode][helpers]" )
{
    for ( std::size_t i { 0 }; i < gprs.size( ); ++i ) CHECK( needs_rex_for_byte( gprs[ i ] ) == ( i >= 4 && i <= 7 ) );
    CHECK_FALSE( needs_rex_for_byte( PReg::invalid( ) ) );
}

TEST_CASE( "encode fits_i8 boundaries", "[x86][encode][helpers]" )
{
    CHECK( fits_i8( 0 ) );
    CHECK( fits_i8( 127 ) );
    CHECK( fits_i8( -128 ) );
    CHECK_FALSE( fits_i8( 128 ) );
    CHECK_FALSE( fits_i8( -129 ) );
    CHECK_FALSE( fits_i8( std::numeric_limits< int64_t >::max( ) ) );
    CHECK_FALSE( fits_i8( std::numeric_limits< int64_t >::min( ) ) );
}

TEST_CASE( "encode cc_nibble matches the x86 condition table", "[x86][encode][helpers]" )
{
    CHECK( cc_nibble( CC::B ) == 0x2 );
    CHECK( cc_nibble( CC::AE ) == 0x3 );
    CHECK( cc_nibble( CC::E ) == 0x4 );
    CHECK( cc_nibble( CC::NE ) == 0x5 );
    CHECK( cc_nibble( CC::BE ) == 0x6 );
    CHECK( cc_nibble( CC::A ) == 0x7 );
    CHECK( cc_nibble( CC::L ) == 0xC );
    CHECK( cc_nibble( CC::GE ) == 0xD );
    CHECK( cc_nibble( CC::LE ) == 0xE );
    CHECK( cc_nibble( CC::G ) == 0xF );
}

TEST_CASE( "encode emit_rex combinations", "[x86][encode][helpers]" )
{
    const auto rex = []( const bool w, const PReg reg, const PReg rm, const bool byte_op = false )
    {
        Buffer buf;
        emit_rex( buf, w, reg, rm, byte_op );
        return take( buf );
    };

    CHECK( rex( false, RAX, RCX ) == Bytes { } );
    CHECK( rex( false, PReg::invalid( ), PReg::invalid( ) ) == Bytes { } );
    CHECK( rex( true, RAX, RCX ) == Bytes { 0x48 } );
    CHECK( rex( false, R8, RAX ) == Bytes { 0x44 } );
    CHECK( rex( false, RAX, R8 ) == Bytes { 0x41 } );
    CHECK( rex( true, R15, R15 ) == Bytes { 0x4D } );
    CHECK( rex( true, PReg::invalid( ), R9 ) == Bytes { 0x49 } );
    CHECK( rex( false, RSI, RAX, true ) == Bytes { 0x40 } );
    CHECK( rex( false, RAX, RDI, true ) == Bytes { 0x40 } );
    CHECK( rex( false, RAX, RBX, true ) == Bytes { } );
    CHECK( rex( false, RSP, RSP, false ) == Bytes { } );
    CHECK( rex( false, R8, RSI, true ) == Bytes { 0x44 } );
    CHECK( rex( true, RSI, RSI, true ) == Bytes { 0x48 } );
}

TEST_CASE( "encode emit_rex_mem combinations", "[x86][encode][helpers]" )
{
    const auto rex = []( const bool w, const PReg reg, const PReg base, const PReg index, const bool byte_op = false )
    {
        Buffer buf;
        emit_rex_mem( buf, w, reg, base, index, byte_op );
        return take( buf );
    };

    CHECK( rex( false, RAX, RCX, PReg::invalid( ) ) == Bytes { } );
    CHECK( rex( true, RAX, RCX, PReg::invalid( ) ) == Bytes { 0x48 } );
    CHECK( rex( false, R8, RAX, PReg::invalid( ) ) == Bytes { 0x44 } );
    CHECK( rex( false, RAX, R8, PReg::invalid( ) ) == Bytes { 0x41 } );
    CHECK( rex( false, RAX, RAX, R8 ) == Bytes { 0x42 } );
    CHECK( rex( true, R9, R10, R11 ) == Bytes { 0x4F } );
    CHECK( rex( false, RSI, RAX, PReg::invalid( ), true ) == Bytes { 0x40 } );
    CHECK( rex( false, RAX, RSI, PReg::invalid( ), true ) == Bytes { } );
    CHECK( rex( false, RAX, RAX, RSI, true ) == Bytes { } );
}

TEST_CASE( "encode emit_mem_operand forms", "[x86][encode][helpers]" )
{
    const auto mem = []( const uint8_t reg, const PReg base, const PReg index, const uint8_t scale, const int32_t disp )
    {
        Buffer buf;
        emit_mem_operand( buf, reg, base, index, scale, disp );
        return take( buf );
    };
    const auto none = PReg::invalid( );

    CHECK( mem( 0, RCX, none, 0, 0 ) == Bytes { 0x01 } );
    CHECK( mem( 0, RCX, none, 0, 8 ) == Bytes { 0x41, 0x08 } );
    CHECK( mem( 0, RCX, none, 0, -8 ) == Bytes { 0x41, 0xF8 } );
    CHECK( mem( 0, RCX, none, 0, 127 ) == Bytes { 0x41, 0x7F } );
    CHECK( mem( 0, RCX, none, 0, -128 ) == Bytes { 0x41, 0x80 } );
    CHECK( mem( 0, RCX, none, 0, 128 ) == Bytes { 0x81, 0x80, 0x00, 0x00, 0x00 } );
    CHECK( mem( 0, RCX, none, 0, -129 ) == Bytes { 0x81, 0x7F, 0xFF, 0xFF, 0xFF } );
    CHECK( mem( 0, RSP, none, 0, 0 ) == Bytes { 0x04, 0x24 } );
    CHECK( mem( 0, RSP, none, 0, 8 ) == Bytes { 0x44, 0x24, 0x08 } );
    CHECK( mem( 0, RSP, none, 0, 0x100 ) == Bytes { 0x84, 0x24, 0x00, 0x01, 0x00, 0x00 } );
    CHECK( mem( 0, R12, none, 0, 0 ) == Bytes { 0x04, 0x24 } );
    CHECK( mem( 0, RBP, none, 0, 0 ) == Bytes { 0x45, 0x00 } );
    CHECK( mem( 0, R13, none, 0, 0 ) == Bytes { 0x45, 0x00 } );
    CHECK( mem( 0, RBP, none, 0, -8 ) == Bytes { 0x45, 0xF8 } );
    CHECK( mem( 7, RAX, none, 0, 0 ) == Bytes { 0x38 } );
    CHECK( mem( 9, RAX, none, 0, 0 ) == Bytes { 0x08 } );
    CHECK( mem( 0, RCX, RDX, 3, 0 ) == Bytes { 0x04, 0xD1 } );
    CHECK( mem( 0, R8, R9, 0, 4 ) == Bytes { 0x44, 0x08, 0x04 } );
    CHECK( mem( 0, RBP, RAX, 2, 0 ) == Bytes { 0x44, 0x85, 0x00 } );
    CHECK( mem( 0, RAX, R12, 0, 0 ) == Bytes { 0x04, 0x20 } );
    CHECK( mem( 0, RSP, RCX, 1, 0x1000 ) == Bytes { 0x84, 0x4C, 0x00, 0x10, 0x00, 0x00 } );
}

TEST_CASE( "encode mov register to register", "[x86][encode]" )
{
    CHECK( enc( inst::movq_mr { rm( RAX ), r( RCX ) }, { RAX, RCX } ) == Bytes { 0x48, 0x89, 0xC8 } );
    CHECK( enc( inst::movq_mr { rm( R8 ), r( RAX ) }, { R8, RAX } ) == Bytes { 0x49, 0x89, 0xC0 } );
    CHECK( enc( inst::movq_mr { rm( RAX ), r( R15 ) }, { RAX, R15 } ) == Bytes { 0x4C, 0x89, 0xF8 } );
    CHECK( enc( inst::movq_mr { rm( R15 ), r( R15 ) }, { R15, R15 } ) == Bytes { 0x4D, 0x89, 0xFF } );
    CHECK( enc( inst::movq_mr { rm( RSP ), r( RBP ) }, { RSP, RBP } ) == Bytes { 0x48, 0x89, 0xEC } );
}

TEST_CASE( "encode mov uses the pregs not the inst registers", "[x86][encode]" )
{
    const auto v = Reg::from_vreg( codegen::VReg::make( 300, RegClass::Int ) );
    CHECK( enc( inst::movq_mr { GprMem { v }, v }, { RDX, RBX } ) == Bytes { 0x48, 0x89, 0xDA } );
}

TEST_CASE( "encode mov immediate zero is a 32-bit xor", "[x86][encode]" )
{
    CHECK( enc( inst::MovImm { r( RAX ), 0 }, { RAX } ) == Bytes { 0x31, 0xC0 } );
    CHECK( enc( inst::MovImm { r( RDI ), 0 }, { RDI } ) == Bytes { 0x31, 0xFF } );
    CHECK( enc( inst::MovImm { r( R8 ), 0 }, { R8 } ) == Bytes { 0x45, 0x31, 0xC0 } );
    CHECK( enc( inst::MovImm { r( R15 ), 0 }, { R15 } ) == Bytes { 0x45, 0x31, 0xFF } );
}

TEST_CASE( "encode mov immediate positive 32-bit", "[x86][encode]" )
{
    CHECK( enc( inst::MovImm { r( RCX ), 1 }, { RCX } ) == Bytes { 0xB9, 0x01, 0x00, 0x00, 0x00 } );
    CHECK( enc( inst::MovImm { r( R9 ), 1 }, { R9 } ) == Bytes { 0x41, 0xB9, 0x01, 0x00, 0x00, 0x00 } );
    CHECK( enc( inst::MovImm { r( RAX ), 0x7fffffff }, { RAX } ) == Bytes { 0xB8, 0xFF, 0xFF, 0xFF, 0x7F } );
    CHECK( enc( inst::MovImm { r( RAX ), 0xffffffff }, { RAX } ) == Bytes { 0xB8, 0xFF, 0xFF, 0xFF, 0xFF } );
}

TEST_CASE( "encode mov immediate negative 32-bit sign extended", "[x86][encode]" )
{
    CHECK( enc( inst::MovImm { r( RAX ), -1 }, { RAX } ) == Bytes { 0x48, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF } );
    CHECK( enc( inst::MovImm { r( R11 ), -1 }, { R11 } ) == Bytes { 0x49, 0xC7, 0xC3, 0xFF, 0xFF, 0xFF, 0xFF } );
    CHECK( enc( inst::MovImm { r( RAX ), std::numeric_limits< int32_t >::min( ) }, { RAX } )
           == Bytes { 0x48, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x80 } );
}

TEST_CASE( "encode mov immediate 64-bit", "[x86][encode]" )
{
    CHECK( enc( inst::MovImm { r( RAX ), -0x80000001LL }, { RAX } )
           == Bytes { 0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF } );
    CHECK( enc( inst::MovImm { r( RDX ), 0x100000000LL }, { RDX } )
           == Bytes { 0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 } );
    CHECK( enc( inst::MovImm { r( R12 ), std::numeric_limits< int64_t >::min( ) }, { R12 } )
           == Bytes { 0x49, 0xBC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80 } );
    CHECK( enc( inst::MovImm { r( RAX ), std::numeric_limits< int64_t >::max( ) }, { RAX } )
           == Bytes { 0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F } );
}

TEST_CASE( "encode alu register forms", "[x86][encode]" )
{
    CHECK( enc( inst::addq_mr { rm( RAX ), r( RCX ) }, { RAX, RCX } ) == Bytes { 0x48, 0x01, 0xC8 } );
    CHECK( enc( inst::subq_mr { rm( RDX ), r( R9 ) }, { RDX, R9 } ) == Bytes { 0x4C, 0x29, 0xCA } );
    CHECK( enc( inst::andq_mr { rm( R10 ), r( RBX ) }, { R10, RBX } ) == Bytes { 0x49, 0x21, 0xDA } );
    CHECK( enc( inst::orq_mr { rm( RAX ), r( RAX ) }, { RAX, RAX } ) == Bytes { 0x48, 0x09, 0xC0 } );
    CHECK( enc( inst::xorq_mr { rm( R15 ), r( R14 ) }, { R15, R14 } ) == Bytes { 0x4D, 0x31, 0xF7 } );
    CHECK( enc( inst::cmpq_mr { rm( RSI ), r( RDI ) }, { RSI, RDI } ) == Bytes { 0x48, 0x39, 0xFE } );
    CHECK( enc( inst::testq_mr { rm( RAX ), r( RBX ) }, { RAX, RBX } ) == Bytes { 0x48, 0x85, 0xD8 } );
}

TEST_CASE( "encode imul register form puts dst in reg field", "[x86][encode]" )
{
    CHECK( enc( inst::imulq_rm { r( RAX ), rm( RCX ) }, { RAX, RCX } ) == Bytes { 0x48, 0x0F, 0xAF, 0xC1 } );
    CHECK( enc( inst::imulq_rm { r( R8 ), rm( R9 ) }, { R8, R9 } ) == Bytes { 0x4D, 0x0F, 0xAF, 0xC1 } );
    CHECK( enc( inst::imulq_rm { r( RBX ), rm( R12 ) }, { RBX, R12 } ) == Bytes { 0x49, 0x0F, 0xAF, 0xDC } );
    CHECK( enc( inst::imulq_rm { r( R12 ), rm( RBX ) }, { R12, RBX } ) == Bytes { 0x4C, 0x0F, 0xAF, 0xE3 } );
}

TEST_CASE( "encode alu immediate short and long forms", "[x86][encode]" )
{
    CHECK( enc( inst::addq_mi8 { rm( RAX ), 5 }, { RAX } ) == Bytes { 0x48, 0x83, 0xC0, 0x05 } );
    CHECK( enc( inst::addq_mi8 { rm( RAX ), 127 }, { RAX } ) == Bytes { 0x48, 0x83, 0xC0, 0x7F } );
    CHECK( enc( inst::addq_mi32 { rm( RAX ), 128 }, { RAX } ) == Bytes { 0x48, 0x81, 0xC0, 0x80, 0x00, 0x00, 0x00 } );
    CHECK( enc( inst::addq_mi8 { rm( RAX ), -128 }, { RAX } ) == Bytes { 0x48, 0x83, 0xC0, 0x80 } );
    CHECK( enc( inst::addq_mi32 { rm( RAX ), -129 }, { RAX } ) == Bytes { 0x48, 0x81, 0xC0, 0x7F, 0xFF, 0xFF, 0xFF } );
    CHECK( enc( inst::orq_mi8 { rm( RCX ), 1 }, { RCX } ) == Bytes { 0x48, 0x83, 0xC9, 0x01 } );
    CHECK( enc( inst::andq_mi32 { rm( R8 ), 0xff }, { R8 } ) == Bytes { 0x49, 0x81, 0xE0, 0xFF, 0x00, 0x00, 0x00 } );
    CHECK( enc( inst::subq_mi8 { rm( RSP ), 8 }, { RSP } ) == Bytes { 0x48, 0x83, 0xEC, 0x08 } );
    CHECK( enc( inst::xorq_mi8 { rm( RDX ), -1 }, { RDX } ) == Bytes { 0x48, 0x83, 0xF2, 0xFF } );
    CHECK( enc( inst::cmpq_mi8 { rm( R15 ), 0 }, { R15 } ) == Bytes { 0x49, 0x83, 0xFF, 0x00 } );
    CHECK( enc( inst::cmpq_mi32 { rm( RAX ), 0x7fffffff }, { RAX } ) == Bytes { 0x48, 0x81, 0xF8, 0xFF, 0xFF, 0xFF, 0x7F } );
}

TEST_CASE( "encode alu immediate imul uses the three operand form", "[x86][encode]" )
{
    CHECK( enc( inst::imulq_rmi8 { r( RAX ), rm( RAX ), 5 }, { RAX, RAX } ) == Bytes { 0x48, 0x6B, 0xC0, 0x05 } );
    CHECK( enc( inst::imulq_rmi32 { r( R9 ), rm( R9 ), 1000 }, { R9, R9 } ) == Bytes { 0x4D, 0x69, 0xC9, 0xE8, 0x03, 0x00, 0x00 } );
    CHECK( enc( inst::imulq_rmi8 { r( RBX ), rm( RBX ), -1 }, { RBX, RBX } ) == Bytes { 0x48, 0x6B, 0xDB, 0xFF } );
    CHECK( enc( inst::imulq_rmi8 { r( RAX ), rm( R12 ), 3 }, { RAX, R12 } ) == Bytes { 0x49, 0x6B, 0xC4, 0x03 } );
}

TEST_CASE( "encode alu immediate test takes imm32", "[x86][encode]" )
{
    CHECK( raw( inst::testq_mi32 { rm( RAX ), 1 } ) == Bytes { 0x48, 0xF7, 0xC0, 0x01, 0x00, 0x00, 0x00 } );
    CHECK( raw( inst::testq_mi32 { rm( R10 ), 5 } ) == Bytes { 0x49, 0xF7, 0xC2, 0x05, 0x00, 0x00, 0x00 } );
    CHECK( raw( inst::testq_mi32 { rm( RAX ), -1 } ) == Bytes { 0x48, 0xF7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF } );
}

TEST_CASE( "encode unary not and neg", "[x86][encode]" )
{
    CHECK( enc( inst::notq_m { rm( RAX ) }, { RAX } ) == Bytes { 0x48, 0xF7, 0xD0 } );
    CHECK( enc( inst::negq_m { rm( RAX ) }, { RAX } ) == Bytes { 0x48, 0xF7, 0xD8 } );
    CHECK( enc( inst::notq_m { rm( R13 ) }, { R13 } ) == Bytes { 0x49, 0xF7, 0xD5 } );
    CHECK( enc( inst::negq_m { rm( R8 ) }, { R8 } ) == Bytes { 0x49, 0xF7, 0xD8 } );
}

TEST_CASE( "encode shifts by cl", "[x86][encode]" )
{
    CHECK( enc( inst::shlq_mc { rm( RAX ), r( RCX ) }, { RAX, RCX } ) == Bytes { 0x48, 0xD3, 0xE0 } );
    CHECK( enc( inst::shrq_mc { rm( RDX ), r( RCX ) }, { RDX, RCX } ) == Bytes { 0x48, 0xD3, 0xEA } );
    CHECK( enc( inst::sarq_mc { rm( R9 ), r( RCX ) }, { R9, RCX } ) == Bytes { 0x49, 0xD3, 0xF9 } );
}

TEST_CASE( "encode shifts by immediate emit the count as given", "[x86][encode]" )
{
    CHECK( enc( inst::shlq_mi8 { rm( RAX ), 3 }, { RAX } ) == Bytes { 0x48, 0xC1, 0xE0, 0x03 } );
    CHECK( enc( inst::shrq_mi8 { rm( R11 ), 63 }, { R11 } ) == Bytes { 0x49, 0xC1, 0xEB, 0x3F } );
    CHECK( enc( inst::sarq_mi8 { rm( RCX ), 1 }, { RCX } ) == Bytes { 0x48, 0xC1, 0xF9, 0x01 } );
    CHECK( enc( inst::shlq_mi8 { rm( RAX ), 64 }, { RAX } ) == Bytes { 0x48, 0xC1, 0xE0, 0x40 } );
}

TEST_CASE( "encode test register every size", "[x86][encode]" )
{
    CHECK( enc( test_of( RAX, 64 ), { RAX, RAX } ) == Bytes { 0x48, 0x85, 0xC0 } );
    CHECK( enc( test_of( R9, 64 ), { R9, R9 } ) == Bytes { 0x4D, 0x85, 0xC9 } );
    CHECK( enc( test_of( RAX, 32 ), { RAX, RAX } ) == Bytes { 0x85, 0xC0 } );
    CHECK( enc( test_of( R9, 32 ), { R9, R9 } ) == Bytes { 0x45, 0x85, 0xC9 } );
    CHECK( enc( test_of( RAX, 16 ), { RAX, RAX } ) == Bytes { 0x66, 0x85, 0xC0 } );
    CHECK( enc( test_of( R9, 16 ), { R9, R9 } ) == Bytes { 0x66, 0x45, 0x85, 0xC9 } );
    CHECK( enc( test_of( RAX, 8 ), { RAX, RAX } ) == Bytes { 0x84, 0xC0 } );
    CHECK( enc( test_of( RBX, 8 ), { RBX, RBX } ) == Bytes { 0x84, 0xDB } );
    CHECK( enc( test_of( RSI, 8 ), { RSI, RSI } ) == Bytes { 0x40, 0x84, 0xF6 } );
    CHECK( enc( test_of( RSP, 8 ), { RSP, RSP } ) == Bytes { 0x40, 0x84, 0xE4 } );
    CHECK( enc( test_of( R9, 8 ), { R9, R9 } ) == Bytes { 0x45, 0x84, 0xC9 } );
}

TEST_CASE( "encode cmov every condition", "[x86][encode]" )
{
    CHECK( enc( inst::cmovq_rm { r( RAX ), rm( RCX ), CC::E }, { RAX, RCX } ) == Bytes { 0x48, 0x0F, 0x44, 0xC1 } );
    CHECK( enc( inst::cmovq_rm { r( R8 ), rm( R9 ), CC::NE }, { R8, R9 } ) == Bytes { 0x4D, 0x0F, 0x45, 0xC1 } );
    CHECK( enc( inst::cmovq_rm { r( RDX ), rm( RBX ), CC::L }, { RDX, RBX } ) == Bytes { 0x48, 0x0F, 0x4C, 0xD3 } );
    for ( const auto cc : all_ccs )
        CHECK( enc( inst::cmovq_rm { r( RAX ), rm( RAX ), cc }, { RAX, RAX } )
               == Bytes { 0x48, 0x0F, static_cast< uint8_t >( 0x40 | cc_nibble( cc ) ), 0xC0 } );
}

TEST_CASE( "encode loads of every size", "[x86][encode][mem]" )
{
    const auto load = []( const PReg dst, const PReg base, const int32_t disp, const int bits )
    {
        return enc( load_of( dst, { .base = r( base ), .disp = disp }, bits ), { dst, base } );
    };

    CHECK( load( RAX, RCX, 0, 64 ) == Bytes { 0x48, 0x8B, 0x01 } );
    CHECK( load( RAX, RCX, 8, 64 ) == Bytes { 0x48, 0x8B, 0x41, 0x08 } );
    CHECK( load( RAX, RCX, 0x100, 64 ) == Bytes { 0x48, 0x8B, 0x81, 0x00, 0x01, 0x00, 0x00 } );
    CHECK( load( RAX, RCX, -8, 64 ) == Bytes { 0x48, 0x8B, 0x41, 0xF8 } );
    CHECK( load( RAX, RCX, -129, 64 ) == Bytes { 0x48, 0x8B, 0x81, 0x7F, 0xFF, 0xFF, 0xFF } );
    CHECK( load( RAX, RSP, 0, 64 ) == Bytes { 0x48, 0x8B, 0x04, 0x24 } );
    CHECK( load( RAX, RSP, 8, 64 ) == Bytes { 0x48, 0x8B, 0x44, 0x24, 0x08 } );
    CHECK( load( RAX, RBP, 0, 64 ) == Bytes { 0x48, 0x8B, 0x45, 0x00 } );
    CHECK( load( RAX, R12, 0, 64 ) == Bytes { 0x49, 0x8B, 0x04, 0x24 } );
    CHECK( load( RAX, R13, 0, 64 ) == Bytes { 0x49, 0x8B, 0x45, 0x00 } );
    CHECK( load( R8, R9, 16, 64 ) == Bytes { 0x4D, 0x8B, 0x41, 0x10 } );
    CHECK( load( RAX, RCX, 0, 32 ) == Bytes { 0x8B, 0x01 } );
    CHECK( load( R10, RAX, 0, 32 ) == Bytes { 0x44, 0x8B, 0x10 } );
    CHECK( load( RAX, RCX, 0, 16 ) == Bytes { 0x48, 0x0F, 0xB7, 0x01 } );
    CHECK( load( RAX, RCX, 0, 8 ) == Bytes { 0x48, 0x0F, 0xB6, 0x01 } );
    CHECK( load( RDI, R12, 1, 8 ) == Bytes { 0x49, 0x0F, 0xB6, 0x7C, 0x24, 0x01 } );
}

TEST_CASE( "encode loads with an index register", "[x86][encode][mem]" )
{
    const auto load = []( const PReg dst, const PReg base, const PReg index, const uint8_t scale, const int32_t disp )
    {
        return enc( load_of( dst, { .base = r( base ), .index = r( index ), .scale = scale, .disp = disp }, 64 ), { dst, base, index } );
    };

    CHECK( load( RAX, RCX, RDX, 3, 0 ) == Bytes { 0x48, 0x8B, 0x04, 0xD1 } );
    CHECK( load( RAX, R8, R9, 0, 4 ) == Bytes { 0x4B, 0x8B, 0x44, 0x08, 0x04 } );
    CHECK( load( RAX, RBP, RAX, 2, 0 ) == Bytes { 0x48, 0x8B, 0x44, 0x85, 0x00 } );
    CHECK( load( RAX, RAX, R12, 0, 0 ) == Bytes { 0x4A, 0x8B, 0x04, 0x20 } );
}

TEST_CASE( "encode stores of every size", "[x86][encode][mem]" )
{
    const auto store = []( const PReg base, const int32_t disp, const PReg src, const int bits )
    {
        return enc( store_of( { .base = r( base ), .disp = disp }, src, bits ), { base, src } );
    };

    CHECK( store( RCX, 0, RAX, 64 ) == Bytes { 0x48, 0x89, 0x01 } );
    CHECK( store( RCX, 0, RAX, 32 ) == Bytes { 0x89, 0x01 } );
    CHECK( store( RCX, 0, RAX, 16 ) == Bytes { 0x66, 0x89, 0x01 } );
    CHECK( store( RCX, 0, RAX, 8 ) == Bytes { 0x88, 0x01 } );
    CHECK( store( RAX, 0, RSI, 8 ) == Bytes { 0x40, 0x88, 0x30 } );
    CHECK( store( RAX, 0, RSP, 8 ) == Bytes { 0x40, 0x88, 0x20 } );
    CHECK( store( RAX, 0, R8, 8 ) == Bytes { 0x44, 0x88, 0x00 } );
    CHECK( store( RSI, 0, RAX, 8 ) == Bytes { 0x88, 0x06 } );
    CHECK( store( R8, 0, R9, 16 ) == Bytes { 0x66, 0x45, 0x89, 0x08 } );
    CHECK( store( RSP, 16, RDX, 64 ) == Bytes { 0x48, 0x89, 0x54, 0x24, 0x10 } );
    CHECK( store( R13, 0, RAX, 64 ) == Bytes { 0x49, 0x89, 0x45, 0x00 } );
}

TEST_CASE( "encode stores with an index register", "[x86][encode][mem]" )
{
    const auto bytes = enc( store_of( { .base = r( RAX ), .index = r( RCX ), .scale = 2, .disp = -4 }, RDX, 32 ), { RAX, RCX, RDX } );
    CHECK( bytes == Bytes { 0x89, 0x54, 0x88, 0xFC } );
}

TEST_CASE( "encode zero extension", "[x86][encode]" )
{
    CHECK( enc( inst::movzxq_rm8 { r( RAX ), rm( RCX ) }, { RAX, RCX } ) == Bytes { 0x48, 0x0F, 0xB6, 0xC1 } );
    CHECK( enc( inst::movzxq_rm8 { r( RAX ), rm( RSI ) }, { RAX, RSI } ) == Bytes { 0x48, 0x0F, 0xB6, 0xC6 } );
    CHECK( enc( inst::movzxq_rm16 { r( RAX ), rm( RCX ) }, { RAX, RCX } ) == Bytes { 0x48, 0x0F, 0xB7, 0xC1 } );
    CHECK( enc( inst::movl_rm { r( RAX ), rm( RCX ) }, { RAX, RCX } ) == Bytes { 0x8B, 0xC1 } );
    CHECK( enc( inst::movl_rm { r( R8 ), rm( R9 ) }, { R8, R9 } ) == Bytes { 0x45, 0x8B, 0xC1 } );
    CHECK( enc( inst::movl_rm { r( RAX ), rm( RSI ) }, { RAX, RSI } ) == Bytes { 0x8B, 0xC6 } );
}

TEST_CASE( "encode sign extension", "[x86][encode]" )
{
    CHECK( enc( inst::movsxq_rm8 { r( RAX ), rm( RCX ) }, { RAX, RCX } ) == Bytes { 0x48, 0x0F, 0xBE, 0xC1 } );
    CHECK( enc( inst::movsxq_rm16 { r( RDX ), rm( R9 ) }, { RDX, R9 } ) == Bytes { 0x49, 0x0F, 0xBF, 0xD1 } );
    CHECK( enc( inst::movsxdq_rm32 { r( RAX ), rm( RCX ) }, { RAX, RCX } ) == Bytes { 0x48, 0x63, 0xC1 } );
    CHECK( enc( inst::movsxdq_rm32 { r( R10 ), rm( RAX ) }, { R10, RAX } ) == Bytes { 0x4C, 0x63, 0xD0 } );
}

TEST_CASE( "encode division family", "[x86][encode]" )
{
    CHECK( enc( inst::cqo { r( RAX ), r( RDX ) }, { RAX, RDX } ) == Bytes { 0x48, 0x99 } );
    CHECK( enc( inst::divq_m { rm( RCX ), r( RAX ), r( RDX ) }, { RCX, RAX, RDX } ) == Bytes { 0x48, 0xF7, 0xF1 } );
    CHECK( enc( inst::divq_m { rm( R8 ), r( RAX ), r( RDX ) }, { R8, RAX, RDX } ) == Bytes { 0x49, 0xF7, 0xF0 } );
    CHECK( enc( inst::idivq_m { rm( RCX ), r( RAX ), r( RDX ) }, { RCX, RAX, RDX } ) == Bytes { 0x48, 0xF7, 0xF9 } );
    CHECK( enc( inst::idivq_m { rm( R15 ), r( RAX ), r( RDX ) }, { R15, RAX, RDX } ) == Bytes { 0x49, 0xF7, 0xFF } );
}

TEST_CASE( "encode push and pop", "[x86][encode]" )
{
    CHECK( raw( inst::pushq_r { r( RAX ) } ) == Bytes { 0x50 } );
    CHECK( raw( inst::pushq_r { r( RBX ) } ) == Bytes { 0x53 } );
    CHECK( raw( inst::pushq_r { r( R8 ) } ) == Bytes { 0x41, 0x50 } );
    CHECK( raw( inst::pushq_r { r( R15 ) } ) == Bytes { 0x41, 0x57 } );
    CHECK( raw( inst::popq_r { r( RAX ) } ) == Bytes { 0x58 } );
    CHECK( raw( inst::popq_r { r( RBP ) } ) == Bytes { 0x5D } );
    CHECK( raw( inst::popq_r { r( R12 ) } ) == Bytes { 0x41, 0x5C } );
}

TEST_CASE( "encode stack addr resolves the slot offset from rsp", "[x86][encode]" )
{
    CHECK( enc( inst::StackAddr { r( RAX ), 0, 0 }, { RAX }, { 0 } ) == Bytes { 0x48, 0x8D, 0x04, 0x24 } );
    CHECK( enc( inst::StackAddr { r( RAX ), 0, 8 }, { RAX }, { 0 } ) == Bytes { 0x48, 0x8D, 0x44, 0x24, 0x08 } );
    CHECK( enc( inst::StackAddr { r( RAX ), 1, 4 }, { RAX }, { 0, 32 } ) == Bytes { 0x48, 0x8D, 0x44, 0x24, 0x24 } );
    CHECK( enc( inst::StackAddr { r( RAX ), 0, 0 }, { RAX }, { 127 } ) == Bytes { 0x48, 0x8D, 0x44, 0x24, 0x7F } );
    CHECK( enc( inst::StackAddr { r( RAX ), 0, 1 }, { RAX }, { 127 } ) == Bytes { 0x48, 0x8D, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00 } );
    CHECK( enc( inst::StackAddr { r( R9 ), 0, 8 }, { R9 }, { 0 } ) == Bytes { 0x4C, 0x8D, 0x4C, 0x24, 0x08 } );
    CHECK( enc( inst::StackAddr { r( R12 ), 0, 16 }, { R12 }, { 0 } ) == Bytes { 0x4C, 0x8D, 0x64, 0x24, 0x10 } );
}

TEST_CASE( "encode setcc every condition and byte register", "[x86][encode]" )
{
    const std::pair< CC, uint8_t > codes[] = {
        { CC::E, 0x94},
        {CC::NE, 0x95},
        { CC::B, 0x92},
        {CC::BE, 0x96},
        { CC::A, 0x97},
        {CC::AE, 0x93},
        { CC::L, 0x9C},
        {CC::LE, 0x9E},
        { CC::G, 0x9F},
        {CC::GE, 0x9D}
    };
    for ( const auto& [ cc, code ] : codes ) CHECK( enc( inst::setcc_m8 { rm( RAX ), cc }, { RAX } ) == Bytes { 0x0F, code, 0xC0 } );

    CHECK( enc( inst::setcc_m8 { rm( RCX ), CC::NE }, { RCX } ) == Bytes { 0x0F, 0x95, 0xC1 } );
    CHECK( enc( inst::setcc_m8 { rm( RBX ), CC::E }, { RBX } ) == Bytes { 0x0F, 0x94, 0xC3 } );
    CHECK( enc( inst::setcc_m8 { rm( RSP ), CC::E }, { RSP } ) == Bytes { 0x40, 0x0F, 0x94, 0xC4 } );
    CHECK( enc( inst::setcc_m8 { rm( RSI ), CC::E }, { RSI } ) == Bytes { 0x40, 0x0F, 0x94, 0xC6 } );
    CHECK( enc( inst::setcc_m8 { rm( RDI ), CC::E }, { RDI } ) == Bytes { 0x40, 0x0F, 0x94, 0xC7 } );
    CHECK( enc( inst::setcc_m8 { rm( R8 ), CC::E }, { R8 } ) == Bytes { 0x41, 0x0F, 0x94, 0xC0 } );
    CHECK( enc( inst::setcc_m8 { rm( R15 ), CC::E }, { R15 } ) == Bytes { 0x41, 0x0F, 0x94, 0xC7 } );
}

TEST_CASE( "encode jmp and jcc leave a rel32 label fixup", "[x86][encode]" )
{
    Buffer     buf;
    const auto target = buf.new_label( );
    X86Inst { inst::Jmp { codegen::BlockId { target } } }.emit( buf, { } );
    CHECK( take( buf ) == Bytes { 0xE9, 0x00, 0x00, 0x00, 0x00 } );
    buf.bind_label( target );
    buf.resolve_labels( );
    CHECK( take( buf ) == Bytes { 0xE9, 0x00, 0x00, 0x00, 0x00 } );
}

TEST_CASE( "encode jcc every condition", "[x86][encode]" )
{
    const std::pair< CC, uint8_t > codes[] = {
        { CC::E, 0x84},
        {CC::NE, 0x85},
        { CC::B, 0x82},
        {CC::BE, 0x86},
        { CC::A, 0x87},
        {CC::AE, 0x83},
        { CC::L, 0x8C},
        {CC::LE, 0x8E},
        { CC::G, 0x8F},
        {CC::GE, 0x8D}
    };
    for ( const auto& [ cc, code ] : codes )
    {
        Buffer     buf;
        const auto l = buf.new_label( );
        buf.bind_label( l );
        X86Inst {
            inst::Jcc { cc, codegen::BlockId { l }, codegen::BlockId { l } }
        }
            .emit( buf, { } );
        buf.resolve_labels( );
        CHECK( take( buf ) == Bytes { 0x0F, code, 0xFA, 0xFF, 0xFF, 0xFF, 0xE9, 0xF5, 0xFF, 0xFF, 0xFF } );
    }
}

TEST_CASE( "encode call records a pc relative reloc", "[x86][encode][relocs]" )
{
    Buffer buf;
    buf.emit_u8( 0x90 );
    X86Inst {
        inst::Call { .name = { 0, 7 }, .info = xiao::support::Box { inst::CallInfo { .args = { RCX, RDX }, .ret = RAX } } }
    }
        .emit( buf );
    const auto code = std::move( buf ).finish( );
    CHECK( code.code == Bytes { 0x90, 0xE8, 0x00, 0x00, 0x00, 0x00 } );
    REQUIRE( code.relocs.size( ) == 1 );
    CHECK( code.relocs[ 0 ].offset == 2 );
    CHECK( code.relocs[ 0 ].kind == codegen::RelocKind::X86CallPCRel4 );
    CHECK( code.relocs[ 0 ].target == ir::ExternalName { 0, 7 } );
    CHECK( code.relocs[ 0 ].addend == -4 );
}

TEST_CASE( "encode call with no args and no return", "[x86][encode][relocs]" )
{
    Buffer buf;
    X86Inst { inst::Call { .name = { 0, 1 } } }.emit( buf, { } );
    const auto code = std::move( buf ).finish( );
    CHECK( code.code == Bytes { 0xE8, 0x00, 0x00, 0x00, 0x00 } );
    REQUIRE( code.relocs.size( ) == 1 );
    CHECK( code.relocs[ 0 ].offset == 1 );
}

TEST_CASE( "encode load ext name near is rip relative lea", "[x86][encode][relocs]" )
{
    Buffer buf;
    X86Inst {
        inst::LoadExtName { .dst = r( RAX ), .name = { 1, 2 }, .offset = 0, .distance = RelocDistance::Near }
    }
        .emit( buf );
    X86Inst {
        inst::LoadExtName { .dst = r( R9 ), .name = { 1, 3 }, .offset = 16, .distance = RelocDistance::Near }
    }
        .emit( buf );
    X86Inst {
        inst::LoadExtName { .dst = r( RCX ), .name = { 0, 4 }, .offset = -8, .distance = RelocDistance::Near }
    }
        .emit( buf );
    const auto code = std::move( buf ).finish( );
    CHECK( code.code == Bytes { 0x48, 0x8D, 0x05, 0, 0, 0, 0, 0x4C, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0x8D, 0x0D, 0, 0, 0, 0 } );
    REQUIRE( code.relocs.size( ) == 3 );
    CHECK( code.relocs[ 0 ].offset == 3 );
    CHECK( code.relocs[ 0 ].kind == codegen::RelocKind::X86PCRel4 );
    CHECK( code.relocs[ 0 ].target == ir::ExternalName { 1, 2 } );
    CHECK( code.relocs[ 0 ].addend == -4 );
    CHECK( code.relocs[ 1 ].offset == 10 );
    CHECK( code.relocs[ 1 ].addend == 12 );
    CHECK( code.relocs[ 1 ].target == ir::ExternalName { 1, 3 } );
    CHECK( code.relocs[ 2 ].offset == 17 );
    CHECK( code.relocs[ 2 ].addend == -12 );
}

TEST_CASE( "encode load ext name far is movabs with abs8 reloc", "[x86][encode][relocs]" )
{
    Buffer buf;
    X86Inst {
        inst::LoadExtName { .dst = r( RAX ), .name = { 0, 5 } }
    }
        .emit( buf );
    X86Inst {
        inst::LoadExtName { .dst = r( R10 ), .name = { 1, 6 }, .offset = 24, .distance = RelocDistance::Far }
    }
        .emit( buf );
    const auto code = std::move( buf ).finish( );
    CHECK( code.code == Bytes { 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0x49, 0xBA, 0, 0, 0, 0, 0, 0, 0, 0 } );
    REQUIRE( code.relocs.size( ) == 2 );
    CHECK( code.relocs[ 0 ].offset == 2 );
    CHECK( code.relocs[ 0 ].kind == codegen::RelocKind::Abs8 );
    CHECK( code.relocs[ 0 ].addend == 0 );
    CHECK( code.relocs[ 0 ].target == ir::ExternalName { 0, 5 } );
    CHECK( code.relocs[ 1 ].offset == 12 );
    CHECK( code.relocs[ 1 ].kind == codegen::RelocKind::Abs8 );
    CHECK( code.relocs[ 1 ].addend == 24 );
}

TEST_CASE( "encode indirect call", "[x86][encode]" )
{
    CHECK( enc( inst::CallInd { .target = r( RAX ) }, { RAX } ) == Bytes { 0xFF, 0xD0 } );
    CHECK( enc( inst::CallInd { .target = r( R11 ) }, { R11 } ) == Bytes { 0x41, 0xFF, 0xD3 } );
    CHECK( enc( inst::CallInd { .target = r( RSP ) }, { RSP } ) == Bytes { 0xFF, 0xD4 } );
    CHECK( enc( inst::CallInd { .target = r( R10 ),
                                .info   = xiao::support::Box { inst::CallInfo { .args = { RCX, RDX, R8, R9 }, .ret = RAX } } },
                { R10, RCX, RDX, R8, R9, RAX } )
           == Bytes { 0x41, 0xFF, 0xD2 } );
}

TEST_CASE( "encode ret and dummy use", "[x86][encode]" )
{
    CHECK( enc( inst::ret { }, { } ) == Bytes { 0xC3 } );
    CHECK( enc( inst::DummyUse { r( RAX ) }, { RAX } ).empty( ) );
    CHECK( enc( inst::DummyUse { r( R15 ) }, { R15 } ).empty( ) );
}

TEST_CASE( "frame spill load and store address rsp", "[x86][encode][frame]" )
{
    const auto load = []( const PReg dst, const int32_t off )
    {
        Buffer buf;
        emit_spill_load( buf, dst, off );
        return take( buf );
    };
    const auto store = []( const int32_t off, const PReg src )
    {
        Buffer buf;
        emit_spill_store( buf, off, src );
        return take( buf );
    };

    CHECK( load( RAX, 8 ) == Bytes { 0x48, 0x8B, 0x44, 0x24, 0x08 } );
    CHECK( load( R10, 0 ) == Bytes { 0x4C, 0x8B, 0x14, 0x24 } );
    CHECK( load( R11, 200 ) == Bytes { 0x4C, 0x8B, 0x9C, 0x24, 0xC8, 0x00, 0x00, 0x00 } );
    CHECK( store( 16, RCX ) == Bytes { 0x48, 0x89, 0x4C, 0x24, 0x10 } );
    CHECK( store( 0, R11 ) == Bytes { 0x4C, 0x89, 0x1C, 0x24 } );
    CHECK( store( 1024, RDI ) == Bytes { 0x48, 0x89, 0xBC, 0x24, 0x00, 0x04, 0x00, 0x00 } );
}

TEST_CASE( "frame spill_offset adds the spill base", "[x86][frame]" )
{
    FrameLayout f { };
    CHECK( f.spill_offset( 0 ) == 0 );
    CHECK( f.spill_offset( 3 ) == 24 );
    f.spill_base = 32;
    CHECK( f.spill_offset( 0 ) == 32 );
    CHECK( f.spill_offset( 1 ) == 40 );
    CHECK( f.spill_offset( 10 ) == 112 );
}

TEST_CASE( "frame emit_move handles every location pair", "[x86][encode][frame]" )
{
    FrameLayout frame { };
    frame.spill_base = 32;
    frame.scratch    = { R10, R11 };

    const auto move = [ & ]( const regalloc::Allocation from, const regalloc::Allocation to )
    {
        Buffer buf;
        emit_move( buf, frame, regalloc::Move { from, to } );
        return take( buf );
    };
    const auto reg = []( const PReg p )
    {
        return regalloc::Allocation { .preg = p };
    };
    const auto slot = []( const int32_t s )
    {
        return regalloc::Allocation { .slot = s };
    };

    CHECK( move( reg( RAX ), reg( RBX ) ) == Bytes { 0x48, 0x89, 0xC3 } );
    CHECK( move( reg( R8 ), reg( RCX ) ) == Bytes { 0x4C, 0x89, 0xC1 } );
    CHECK( move( reg( RDX ), slot( 1 ) ) == Bytes { 0x48, 0x89, 0x54, 0x24, 0x28 } );
    CHECK( move( slot( 0 ), reg( RSI ) ) == Bytes { 0x48, 0x8B, 0x74, 0x24, 0x20 } );
    CHECK( move( slot( 2 ), slot( 3 ) ) == Bytes { 0x4C, 0x8B, 0x5C, 0x24, 0x30, 0x4C, 0x89, 0x5C, 0x24, 0x38 } );
}

TEST_CASE( "frame emit_edits sequences a register swap through scratch", "[x86][encode][frame]" )
{
    FrameLayout frame { };
    frame.scratch = { R10, R11 };

    const std::vector< regalloc::Edit > edits = {
        {0, regalloc::EditPrio::Split, 200, { .preg = RAX }, { .preg = RCX }},
        {0, regalloc::EditPrio::Split, 201, { .preg = RCX }, { .preg = RAX }},
    };
    Buffer buf;
    emit_edits( buf, frame, edits );
    CHECK( decode_all( take( buf ) ) == std::vector< std::string > { "mov r10, rcx", "mov rcx, rax", "mov rax, r10" } );
}

TEST_CASE( "frame emit_edits keeps priority groups separate", "[x86][encode][frame]" )
{
    FrameLayout frame { };
    frame.scratch = { R10, R11 };

    const std::vector< regalloc::Edit > edits = {
        {0, regalloc::EditPrio::EdgeIn, 200, { .preg = RAX }, { .preg = RCX }},
        {0,  regalloc::EditPrio::Split, 201, { .preg = RCX }, { .preg = RDX }},
    };
    Buffer buf;
    emit_edits( buf, frame, edits );
    CHECK( decode_all( take( buf ) ) == std::vector< std::string > { "mov rcx, rax", "mov rdx, rcx" } );
}

TEST_CASE( "frame emit_edits drops self moves", "[x86][encode][frame]" )
{
    FrameLayout frame { };
    frame.scratch = { R10, R11 };

    const std::vector< regalloc::Edit > edits = {
        {0, regalloc::EditPrio::Split, 200, { .preg = RAX }, { .preg = RAX }},
        {0, regalloc::EditPrio::Split, 201,   { .slot = 2 },   { .slot = 2 }},
    };
    Buffer buf;
    emit_edits( buf, frame, edits );
    CHECK( buf.size( ) == 0 );
}

TEST_CASE( "frame rsp adjust picks imm8 or imm32", "[x86][encode][frame]" )
{
    const auto adj = []( const uint8_t ext, const uint32_t n )
    {
        Buffer buf;
        emit_rsp_adjust( buf, ext, n );
        return take( buf );
    };
    CHECK( adj( 5, 8 ) == Bytes { 0x48, 0x83, 0xEC, 0x08 } );
    CHECK( adj( 0, 8 ) == Bytes { 0x48, 0x83, 0xC4, 0x08 } );
    CHECK( adj( 5, 127 ) == Bytes { 0x48, 0x83, 0xEC, 0x7F } );
    CHECK( adj( 5, 128 ) == Bytes { 0x48, 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00 } );
    CHECK( adj( 0, 0x1000 ) == Bytes { 0x48, 0x81, 0xC4, 0x00, 0x10, 0x00, 0x00 } );
    CHECK( decode_all( adj( 5, 40 ) ) == std::vector< std::string > { "sub rsp, 0x28" } );
    CHECK( decode_all( adj( 0, 4096 ) ) == std::vector< std::string > { "add rsp, 0x1000" } );
}

TEST_CASE( "frame prologue and epilogue mirror each other", "[x86][encode][frame]" )
{
    FrameLayout frame { };
    frame.saved        = { RBX, R12, R15 };
    frame.stack_adjust = 40;

    Buffer pro;
    emit_prologue( pro, frame );
    CHECK( take( pro ) == Bytes { 0x53, 0x41, 0x54, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x28 } );

    Buffer epi;
    emit_epilogue( epi, frame );
    CHECK( take( epi ) == Bytes { 0x48, 0x83, 0xC4, 0x28, 0x41, 0x5F, 0x41, 0x5C, 0x5B } );
}

TEST_CASE( "frame prologue with nothing to do is empty", "[x86][encode][frame]" )
{
    const FrameLayout frame { };
    Buffer            pro;
    emit_prologue( pro, frame );
    Buffer epi;
    emit_epilogue( epi, frame );
    CHECK( pro.size( ) == 0 );
    CHECK( epi.size( ) == 0 );
}

TEST_CASE( "frame prologue saves every callee saved register", "[x86][encode][frame]" )
{
    FrameLayout frame { };
    frame.saved = { RBX, RBP, RSI, RDI, R12, R13, R14, R15 };

    Buffer pro;
    emit_prologue( pro, frame );
    CHECK(
        decode_all( take( pro ) )
        == std::vector< std::string > { "push rbx", "push rbp", "push rsi", "push rdi", "push r12", "push r13", "push r14", "push r15" } );

    Buffer epi;
    emit_epilogue( epi, frame );
    CHECK( decode_all( take( epi ) )
           == std::vector< std::string > { "pop r15", "pop r14", "pop r13", "pop r12", "pop rdi", "pop rsi", "pop rbp", "pop rbx" } );
}

TEST_CASE( "frame stack adjust only without saved registers", "[x86][encode][frame]" )
{
    FrameLayout frame { };
    frame.stack_adjust = 0x200;
    Buffer pro;
    emit_prologue( pro, frame );
    CHECK( decode_all( take( pro ) ) == std::vector< std::string > { "sub rsp, 0x200" } );
}

TEST_CASE( "zydis sweep mov register to register", "[x86][encode][zydis]" )
{
    Sweep s;
    for ( const auto d : gprs )
        for ( const auto src : gprs )
            s.check( inst::movq_mr { rm( d ), r( src ) }, { d, src }, std::format( "mov {}, {}", q( d ), q( src ) ) );
    CHECK( s.failures == 0 );
    CHECK( s.checked == 256 );
}

TEST_CASE( "zydis sweep mov immediate", "[x86][encode][zydis]" )
{
    const int64_t imms[] = { 0,
                             1,
                             0x7f,
                             0x80,
                             0xffff,
                             0x7fffffff,
                             0x80000000LL,
                             0xffffffffLL,
                             -1,
                             -128,
                             std::numeric_limits< int32_t >::min( ),
                             -0x80000001LL,
                             0x100000000LL,
                             0x123456789abcdefLL,
                             std::numeric_limits< int64_t >::max( ),
                             std::numeric_limits< int64_t >::min( ) };
    Sweep         s;
    for ( const auto d : gprs )
        for ( const auto imm : imms )
        {
            std::string expected;
            if ( imm == 0 ) expected = std::format( "xor {}, {}", d_names[ d.hw( ) ], d_names[ d.hw( ) ] );
            else if ( imm > 0 && imm <= 0xffffffffLL )
                expected = std::format( "mov {}, {}", d_names[ d.hw( ) ], hex( static_cast< uint64_t >( imm ) ) );
            else
                expected = std::format( "mov {}, {}", q( d ), sx( imm ) );
            s.check( inst::MovImm { r( d ), imm }, { d }, expected );
        }
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep alu register forms", "[x86][encode][zydis]" )
{
    Sweep      s;
    const auto sweep = [ & ]< typename I >( const char* name )
    {
        for ( const auto d : gprs )
            for ( const auto src : gprs )
                s.check( I { rm( d ), r( src ) }, { d, src }, std::format( "{} {}, {}", name, q( d ), q( src ) ) );
    };

    sweep.operator( )< inst::addq_mr >( "add" );
    sweep.operator( )< inst::subq_mr >( "sub" );
    sweep.operator( )< inst::andq_mr >( "and" );
    sweep.operator( )< inst::orq_mr >( "or" );
    sweep.operator( )< inst::xorq_mr >( "xor" );
    sweep.operator( )< inst::cmpq_mr >( "cmp" );
    sweep.operator( )< inst::testq_mr >( "test" );
    for ( const auto d : gprs )
        for ( const auto src : gprs )
            s.check( inst::imulq_rm { r( d ), rm( src ) }, { d, src }, std::format( "imul {}, {}", q( d ), q( src ) ) );

    CHECK( s.failures == 0 );
    CHECK( s.checked == 8 * 256 );
}

TEST_CASE( "zydis sweep alu immediate forms", "[x86][encode][zydis]" )
{
    const int32_t imms[] = { 0, 1, -1, 5, 127, -128, 128, -129, 0x1234, -0x1234, 0x7fffffff, std::numeric_limits< int32_t >::min( ) };
    Sweep         s;
    const auto    sweep = [ & ]< typename Imm8, typename Imm32 >( const char* name )
    {
        for ( const auto d : gprs )
            for ( const auto imm : imms )
                s.check( alu_imm< Imm8, Imm32 >( d, imm ), { d }, std::format( "{} {}, {}", name, q( d ), sx( imm ) ) );
    };

    sweep.operator( )< inst::addq_mi8, inst::addq_mi32 >( "add" );
    sweep.operator( )< inst::subq_mi8, inst::subq_mi32 >( "sub" );
    sweep.operator( )< inst::andq_mi8, inst::andq_mi32 >( "and" );
    sweep.operator( )< inst::orq_mi8, inst::orq_mi32 >( "or" );
    sweep.operator( )< inst::xorq_mi8, inst::xorq_mi32 >( "xor" );
    sweep.operator( )< inst::cmpq_mi8, inst::cmpq_mi32 >( "cmp" );
    for ( const auto d : gprs )
        for ( const auto imm : imms ) s.check_raw( inst::testq_mi32 { rm( d ), imm }, std::format( "test {}, {}", q( d ), sx( imm ) ) );
    for ( const auto d : gprs )
        for ( const auto imm : imms ) s.check( imul_imm( d, imm ), { d, d }, std::format( "imul {}, {}, {}", q( d ), q( d ), sx( imm ) ) );
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep unary", "[x86][encode][zydis]" )
{
    Sweep s;
    for ( const auto d : gprs )
    {
        s.check( inst::notq_m { rm( d ) }, { d }, "not " + q( d ) );
        s.check( inst::negq_m { rm( d ) }, { d }, "neg " + q( d ) );
    }
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep shifts", "[x86][encode][zydis]" )
{
    const int8_t imms[] = { 0, 1, 2, 7, 31, 32, 63 };
    Sweep        s;
    const auto   sweep = [ & ]< typename ByCl, typename ByImm >( const char* name )
    {
        for ( const auto d : gprs )
        {
            s.check( ByCl { rm( d ), r( RCX ) }, { d, RCX }, std::format( "{} {}, cl", name, q( d ) ) );
            for ( const auto imm : imms ) s.check( ByImm { rm( d ), imm }, { d }, std::format( "{} {}, {}", name, q( d ), hex( imm ) ) );
        }
    };

    sweep.operator( )< inst::shlq_mc, inst::shlq_mi8 >( "shl" );
    sweep.operator( )< inst::shrq_mc, inst::shrq_mi8 >( "shr" );
    sweep.operator( )< inst::sarq_mc, inst::sarq_mi8 >( "sar" );
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep test register", "[x86][encode][zydis]" )
{
    Sweep s;
    for ( const auto bits : all_sizes )
        for ( const auto d : gprs ) s.check( test_of( d, bits ), { d, d }, std::format( "test {0}, {0}", sized( d, bits ) ) );
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep cmov", "[x86][encode][zydis]" )
{
    Sweep s;
    for ( const auto cc : all_ccs )
        for ( const auto d : gprs )
            for ( const auto src : gprs )
                s.check( inst::cmovq_rm { r( d ), rm( src ), cc }, { d, src },
                         std::format( "cmov{} {}, {}", cc_suffix( cc ), q( d ), q( src ) ) );
    CHECK( s.failures == 0 );
    CHECK( s.checked == 2560 );
}

TEST_CASE( "zydis sweep setcc", "[x86][encode][zydis]" )
{
    Sweep s;
    for ( const auto cc : all_ccs )
        for ( const auto d : gprs )
            s.check( inst::setcc_m8 { rm( d ), cc }, { d }, std::format( "set{} {}", cc_suffix( cc ), b_names[ d.hw( ) ] ) );
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep loads base and displacement", "[x86][encode][zydis][mem]" )
{
    Sweep s;
    for ( const auto bits : all_sizes )
        for ( const auto d : gprs )
            for ( const auto base : gprs )
                for ( const auto disp : displacements )
                {
                    const auto  m = mem_text( base, PReg::invalid( ), 0, disp );
                    std::string expected;
                    switch ( bits )
                    {
                        case 8: expected = std::format( "movzx {}, byte ptr {}", q( d ), m ); break;
                        case 16: expected = std::format( "movzx {}, word ptr {}", q( d ), m ); break;
                        case 32: expected = std::format( "mov {}, {}", d_names[ d.hw( ) ], m ); break;
                        default: expected = std::format( "mov {}, {}", q( d ), m ); break;
                    }
                    s.check( load_of( d, { .base = r( base ), .disp = disp }, bits ), { d, base }, expected );
                }
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep loads with index and scale", "[x86][encode][zydis][mem]" )
{
    const int32_t disps[] = { 0, 8, -8, 300, -300 };
    Sweep         s;
    for ( const auto bits : all_sizes )
        for ( const auto base : gprs )
            for ( const auto index : gprs )
            {
                if ( index == RSP ) continue;
                for ( uint8_t scale { 0 }; scale <= 3; ++scale )
                    for ( const auto disp : disps )
                    {
                        const auto  d = index == RAX ? RDX : RAX;
                        const auto  m = mem_text( base, index, scale, disp );
                        std::string expected;
                        switch ( bits )
                        {
                            case 8: expected = std::format( "movzx {}, byte ptr {}", q( d ), m ); break;
                            case 16: expected = std::format( "movzx {}, word ptr {}", q( d ), m ); break;
                            case 32: expected = std::format( "mov {}, {}", d_names[ d.hw( ) ], m ); break;
                            default: expected = std::format( "mov {}, {}", q( d ), m ); break;
                        }
                        s.check( load_of( d, { .base = r( base ), .index = r( index ), .scale = scale, .disp = disp }, bits ),
                                 { d, base, index }, expected );
                    }
            }
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep stores base and displacement", "[x86][encode][zydis][mem]" )
{
    Sweep s;
    for ( const auto bits : all_sizes )
        for ( const auto src : gprs )
            for ( const auto base : gprs )
                for ( const auto disp : displacements )
                    s.check( store_of( { .base = r( base ), .disp = disp }, src, bits ), { base, src },
                             std::format( "mov {}, {}", mem_text( base, PReg::invalid( ), 0, disp ), sized( src, bits ) ) );
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep stores with index and scale", "[x86][encode][zydis][mem]" )
{
    const int32_t disps[] = { 0, 16, -300 };
    Sweep         s;
    for ( const auto bits : all_sizes )
        for ( const auto base : gprs )
            for ( const auto index : gprs )
            {
                if ( index == RSP ) continue;
                for ( uint8_t scale { 0 }; scale <= 3; ++scale )
                    for ( const auto disp : disps )
                        for ( const auto src : { RAX, RSI, R9 } )
                            s.check( store_of( { .base = r( base ), .index = r( index ), .scale = scale, .disp = disp }, src, bits ),
                                     { base, index, src },
                                     std::format( "mov {}, {}", mem_text( base, index, scale, disp ), sized( src, bits ) ) );
            }
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep zero and sign extension", "[x86][encode][zydis]" )
{
    Sweep s;
    for ( const auto d : gprs )
        for ( const auto src : gprs )
        {
            s.check( inst::movzxq_rm8 { r( d ), rm( src ) }, { d, src }, std::format( "movzx {}, {}", q( d ), b_names[ src.hw( ) ] ) );
            s.check( inst::movzxq_rm16 { r( d ), rm( src ) }, { d, src }, std::format( "movzx {}, {}", q( d ), w_names[ src.hw( ) ] ) );
            s.check( inst::movl_rm { r( d ), rm( src ) }, { d, src },
                     std::format( "mov {}, {}", d_names[ d.hw( ) ], d_names[ src.hw( ) ] ) );
            s.check( inst::movsxq_rm8 { r( d ), rm( src ) }, { d, src }, std::format( "movsx {}, {}", q( d ), b_names[ src.hw( ) ] ) );
            s.check( inst::movsxq_rm16 { r( d ), rm( src ) }, { d, src }, std::format( "movsx {}, {}", q( d ), w_names[ src.hw( ) ] ) );
            s.check( inst::movsxdq_rm32 { r( d ), rm( src ) }, { d, src }, std::format( "movsxd {}, {}", q( d ), d_names[ src.hw( ) ] ) );
        }
    CHECK( s.failures == 0 );
    CHECK( s.checked == 6 * 256 );
}

TEST_CASE( "zydis sweep division push pop and indirect call", "[x86][encode][zydis]" )
{
    Sweep s;
    s.check( inst::cqo { r( RAX ), r( RDX ) }, { RAX, RDX }, "cqo" );
    for ( const auto d : gprs )
    {
        s.check( inst::divq_m { rm( d ), r( RAX ), r( RDX ) }, { d, RAX, RDX }, "div " + q( d ) );
        s.check( inst::idivq_m { rm( d ), r( RAX ), r( RDX ) }, { d, RAX, RDX }, "idiv " + q( d ) );
        s.check_raw( inst::pushq_r { r( d ) }, "push " + q( d ) );
        s.check_raw( inst::popq_r { r( d ) }, "pop " + q( d ) );
        s.check( inst::CallInd { .target = r( d ) }, { d }, "call " + q( d ) );
    }
    s.check( inst::ret { }, { }, "ret" );
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep stack addr", "[x86][encode][zydis]" )
{
    const int32_t offsets[] = { 0, 8, 127, 128, 4095, 0x10000, 0x7fffffff };
    Sweep         s;
    for ( const auto d : gprs )
        for ( const auto off : offsets )
            s.check( inst::StackAddr { r( d ), 0, 0 }, { d }, std::format( "lea {}, [rsp{}]", q( d ), disp_text( off ) ), { off } );
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep branches and calls", "[x86][encode][zydis]" )
{
    Sweep s;
    {
        Buffer buf;
        ( void ) buf.new_label( );
        X86Inst { inst::Jmp { { 0 } } }.emit( buf, { } );
        const auto d = decode( take( buf ) );
        CHECK( d.ok );
        CHECK( d.length == 5 );
        CHECK( d.text == "jmp 0x0000000000000005" );
    }
    for ( const auto cc : all_ccs )
    {
        Buffer buf;
        ( void ) buf.new_label( );
        X86Inst {
            inst::Jcc { cc, { 0 } }
        }
            .emit( buf, { } );
        const auto d = decode( take( buf ) );
        CHECK( d.ok );
        CHECK( d.length == 6 );
        CHECK( d.text == std::format( "j{} 0x0000000000000006", cc_suffix( cc ) ) );
    }
    s.check(
        inst::Call {
            .name = { 0, 0 }
    },
        { }, "call 0x0000000000000005" );
    for ( const auto d : gprs )
    {
        s.check(
            inst::LoadExtName {
                .dst      = r( d ),
                .name     = { 1, 0 },
                .distance = RelocDistance::Near
        },
            { d }, std::format( "lea {}, [0x0000000000000007]", q( d ) ) );
        s.check(
            inst::LoadExtName {
                .dst      = r( d ),
                .name     = { 1, 0 },
                .distance = RelocDistance::Far
        },
            { d }, std::format( "mov {}, 0x00", q( d ) ) );
    }
    CHECK( s.failures == 0 );
}

TEST_CASE( "zydis sweep spill loads stores and moves", "[x86][encode][zydis][frame]" )
{
    const int32_t offsets[] = { 0, 8, 120, 128, 256, 4096 };
    std::size_t   failures { 0 };
    for ( const auto p : gprs )
        for ( const auto off : offsets )
        {
            Buffer a;
            emit_spill_load( a, p, off );
            Buffer b;
            emit_spill_store( b, off, p );
            const auto la = decode_all( take( a ) );
            const auto lb = decode_all( take( b ) );
            const auto m  = mem_text( RSP, PReg::invalid( ), 0, off );
            if ( la != std::vector< std::string > { std::format( "mov {}, {}", q( p ), m ) } ) ++failures;
            if ( lb != std::vector< std::string > { std::format( "mov {}, {}", m, q( p ) ) } ) ++failures;
        }
    FrameLayout frame { };
    for ( const auto from : gprs )
        for ( const auto to : gprs )
        {
            Buffer buf;
            emit_move( buf, frame, regalloc::Move { { .preg = from }, { .preg = to } } );
            if ( decode_all( take( buf ) ) != std::vector< std::string > { std::format( "mov {}, {}", q( to ), q( from ) ) } ) ++failures;
        }
    CHECK( failures == 0 );
}
