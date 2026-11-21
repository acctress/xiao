#pragma once

#include <xiao/isa/x86/inst/encoding.hh>
#include <xiao/isa/x86/inst/fmt.hh>

#include <catch2/catch_test_macros.hpp>
#include <Zydis.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <variant>

namespace isa_check
{
    using namespace xiao::x86_64;
    using xiao::codegen::Reg;

    struct Expect
    {
        enum class Kind : uint8_t
        {
            Reg,
            Mem,
            Imm
        };

        Kind        kind { };
        Reg         reg { };
        AddressMode mem { };
        uint8_t     bits { };
        int64_t     imm { };
    };

    [[nodiscard]] inline Reg sample_gpr( const int i )
    {
        constexpr std::array regs { RAX, RCX, R12, RSI, R9, R15, RBP, RDX, R8, RDI };
        return Reg::from_preg( regs[ static_cast< std::size_t >( i ) % regs.size( ) ] );
    }

    [[nodiscard]] inline AddressMode sample_mem( const int i )
    {
        const auto r = []( const PReg p )
        {
            return Reg::from_preg( p );
        };
        switch ( i % 5 )
        {
            case 0: return { .base = r( R13 ), .disp = 0x10 };
            case 1: return { .base = r( RSP ), .disp = -8 };
            case 2: return { .base = r( RAX ), .index = r( R12 ), .scale = 3, .disp = 0x100 };
            case 3: return { .base = r( RBP ) };
            default: return { .base = r( R12 ), .disp = 0x12345 };
        }
    }

    [[nodiscard]] inline GprMem sample_rm( const int i )
    {
        if ( i % 2 == 0 ) return GprMem { sample_gpr( i / 2 ) };
        return GprMem { sample_mem( i / 2 ) };
    }

    [[nodiscard]] inline int64_t sample_imm( const int bits, const int i )
    {
        constexpr std::array< int64_t, 4 > i8 { -1, 7, std::numeric_limits< int8_t >::max( ), std::numeric_limits< int8_t >::min( ) };
        constexpr std::array< int64_t, 4 > i16 { -2, 0x1234, std::numeric_limits< int16_t >::max( ),
                                                 std::numeric_limits< int16_t >::min( ) };
        constexpr std::array< int64_t, 4 > i32 { 0x12345, -2, std::numeric_limits< int32_t >::max( ),
                                                 std::numeric_limits< int32_t >::min( ) };
        constexpr std::array< int64_t, 4 > i64 { 0x1122334455667788, -1, std::numeric_limits< int64_t >::min( ), 42 };

        const auto k = static_cast< std::size_t >( i ) % 4;
        switch ( bits )
        {
            case 8: return i8[ k ];
            case 16: return i16[ k ];
            case 32: return i32[ k ];
            default: return i64[ k ];
        }
    }

    [[nodiscard]] inline CC sample_cc( const int i )
    {
        constexpr std::array ccs { CC::E, CC::NE, CC::L, CC::LE, CC::G, CC::GE, CC::B, CC::BE, CC::A, CC::AE };
        return ccs[ static_cast< std::size_t >( i ) % ccs.size( ) ];
    }

    [[nodiscard]] inline Expect expect_gpr( const Reg reg, const uint8_t bits )
    { return { .kind = Expect::Kind::Reg, .reg = reg, .bits = bits }; }

    [[nodiscard]] inline Expect expect_rm( const GprMem& rm, const uint8_t bits )
    {
        if ( const auto* reg = std::get_if< Reg >( &rm.value ) ) return expect_gpr( *reg, bits );
        return { .kind = Expect::Kind::Mem, .mem = std::get< AddressMode >( rm.value ), .bits = bits };
    }

    [[nodiscard]] inline Expect expect_mem( const AddressMode& mem, const uint8_t bits )
    { return { .kind = Expect::Kind::Mem, .mem = mem, .bits = bits }; }

    [[nodiscard]] inline Expect expect_imm( const int64_t value )
    { return { .kind = Expect::Kind::Imm, .imm = value }; }

    [[nodiscard]] inline std::string_view zydis_name( const ZydisRegister reg )
    { return reg == ZYDIS_REGISTER_NONE ? std::string_view { } : std::string_view { ZydisRegisterGetString( reg ) }; }

    [[nodiscard]] inline std::string_view our_name( const Reg reg, const uint8_t bits )
    { return reg.valid( ) ? gpr_name( reg.as_preg( ), bits ) : std::string_view { }; }

    inline void check_operand( const ZydisDecodedOperand& op, const Expect& e )
    {
        switch ( e.kind )
        {
            case Expect::Kind::Reg:
                REQUIRE( op.type == ZYDIS_OPERAND_TYPE_REGISTER );
                CHECK( zydis_name( op.reg.value ) == our_name( e.reg, e.bits ) );
                break;

            case Expect::Kind::Mem:
                REQUIRE( op.type == ZYDIS_OPERAND_TYPE_MEMORY );
                if ( e.bits ) CHECK( op.size == e.bits );
                CHECK( zydis_name( op.mem.base ) == our_name( e.mem.base, 64 ) );
                CHECK( zydis_name( op.mem.index ) == our_name( e.mem.index, 64 ) );
                CHECK( op.mem.scale == ( e.mem.index.valid( ) ? 1u << e.mem.scale : 0u ) );
                CHECK( op.mem.disp.value == e.mem.disp );
                break;

            case Expect::Kind::Imm:
            {
                REQUIRE( op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE );
                const uint64_t mask = op.size >= 64 ? ~uint64_t { } : ( uint64_t { 1 } << op.size ) - 1;
                CHECK( ( op.imm.value.u & mask ) == ( static_cast< uint64_t >( e.imm ) & mask ) );
                break;
            }
        }
    }

    template < typename I >
    void check( const I& inst, std::string_view mnemonic, const bool cc_suffix, const std::initializer_list< Expect > expects )
    {
        MachBuffer< LabelUse > buf;
        inst.emit( buf );
        const auto bytes = buf.data( );

        std::string hex;
        for ( const auto b : bytes ) std::format_to( std::back_inserter( hex ), "{:02X} ", b );
        INFO( inst.to_string( ) << "  [ " << hex << "]" );

        ZydisDisassembledInstruction d { };
        REQUIRE( ZYAN_SUCCESS( ZydisDisassembleIntel( ZYDIS_MACHINE_MODE_LONG_64, 0, bytes.data( ), bytes.size( ), &d ) ) );
        INFO( "zydis: " << d.text );
        CHECK( d.info.length == bytes.size( ) );

        const std::string_view got { ZydisMnemonicGetString( d.info.mnemonic ) };
        for ( const std::string_view rep : { "rep ", "repe ", "repne " } )
            if ( mnemonic.starts_with( rep ) ) mnemonic.remove_prefix( rep.size( ) );
        if ( cc_suffix ) CHECK( got.starts_with( mnemonic ) );
        else
            CHECK( got == ( mnemonic == "movabs" ? std::string_view { "mov" } : mnemonic ) );

        std::size_t next { };
        for ( std::size_t i = 0; i < d.info.operand_count; ++i )
        {
            const auto& op = d.operands[ i ];
            if ( op.visibility != ZYDIS_OPERAND_VISIBILITY_EXPLICIT ) continue;
            REQUIRE( next < expects.size( ) );
            check_operand( op, expects.begin( )[ next++ ] );
        }
        CHECK( next == expects.size( ) );
    }
} // namespace isa_check
