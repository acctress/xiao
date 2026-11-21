#include "support.hh"

#include <xiao/codegen/operand.hh>
#include <xiao/codegen/reg.hh>
#include <xiao/isa/x86/inst/fmt.hh>
#include <xiao/isa/x86/inst/regs.hh>

#include <array>
#include <format>
#include <set>

using namespace xt;
using codegen::MachOperand;
using codegen::OperandKind;
using codegen::OperandPos;
using codegen::PReg;
using codegen::Reg;
using codegen::RegClass;
using codegen::VReg;

TEST_CASE( "preg default constructed is invalid", "[regs]" )
{
    const PReg p { };
    CHECK_FALSE( p.valid( ) );
    CHECK( p.bits == PReg::invalid_bits );
    CHECK( p == PReg::invalid( ) );
    CHECK_FALSE( PReg::invalid( ).valid( ) );
}

TEST_CASE( "preg packs every int hardware encoding", "[regs]" )
{
    for ( uint8_t hw { 0 }; hw <= PReg::max_hw_enc; ++hw )
    {
        const auto p = PReg::make( hw, RegClass::Int );
        CHECK( p.valid( ) );
        CHECK( p.hw( ) == hw );
        CHECK( p.rc( ) == RegClass::Int );
        CHECK( p.index( ) == hw );
        CHECK( p.bits == static_cast< uint8_t >( hw << 1 ) );
    }
}

TEST_CASE( "preg packs every float hardware encoding", "[regs]" )
{
    for ( uint8_t hw { 0 }; hw <= PReg::max_hw_enc; ++hw )
    {
        const auto p = PReg::make( hw, RegClass::Float );
        CHECK( p.valid( ) );
        CHECK( p.hw( ) == hw );
        CHECK( p.rc( ) == RegClass::Float );
        CHECK( p.index( ) == hw + 64 );
        CHECK( p.bits == static_cast< uint8_t >( ( hw << 1 ) | 1 ) );
    }
}

TEST_CASE( "preg indices are unique across classes", "[regs]" )
{
    std::set< uint8_t > seen;
    for ( uint8_t hw { 0 }; hw <= PReg::max_hw_enc; ++hw )
    {
        CHECK( seen.insert( PReg::make( hw, RegClass::Int ).index( ) ).second );
        CHECK( seen.insert( PReg::make( hw, RegClass::Float ).index( ) ).second );
    }
    CHECK( seen.size( ) == 128 );
    CHECK( *seen.rbegin( ) == 127 );
}

TEST_CASE( "preg equality compares class and encoding", "[regs]" )
{
    CHECK( PReg::make( 3, RegClass::Int ) == PReg::make( 3, RegClass::Int ) );
    CHECK_FALSE( PReg::make( 3, RegClass::Int ) == PReg::make( 3, RegClass::Float ) );
    CHECK_FALSE( PReg::make( 3, RegClass::Int ) == PReg::make( 4, RegClass::Int ) );
    CHECK_FALSE( PReg::make( 0, RegClass::Int ) == PReg::invalid( ) );
}

TEST_CASE( "preg is constexpr", "[regs]" )
{
    constexpr auto p = PReg::make( 13, RegClass::Float );
    static_assert( p.hw( ) == 13 );
    static_assert( p.rc( ) == RegClass::Float );
    static_assert( p.index( ) == 77 );
    static_assert( p.valid( ) );
    static_assert( !PReg::invalid( ).valid( ) );
    SUCCEED( );
}

TEST_CASE( "vreg default constructed is invalid", "[regs]" )
{
    const VReg v { };
    CHECK_FALSE( v.valid( ) );
    CHECK( v.bits == VReg::invalid_bits );
    CHECK( v == VReg::invalid( ) );
}

TEST_CASE( "vreg packs index and class", "[regs]" )
{
    const uint32_t indices[]
        = { 0, 1, 2, 63, 64, 127, 128, 129, 255, 256, 1000, 65535, 65536, 1u << 20, VReg::max_index - 1, VReg::max_index };
    for ( const auto idx : indices )
        for ( const auto rc : { RegClass::Int, RegClass::Float } )
        {
            const auto v = VReg::make( idx, rc );
            CHECK( v.valid( ) );
            CHECK( v.index( ) == idx );
            CHECK( v.rc( ) == rc );
            CHECK( v.bits == ( ( idx << 1 ) | static_cast< uint32_t >( rc ) ) );
        }
}

TEST_CASE( "vreg equality", "[regs]" )
{
    CHECK( VReg::make( 200, RegClass::Int ) == VReg::make( 200, RegClass::Int ) );
    CHECK_FALSE( VReg::make( 200, RegClass::Int ) == VReg::make( 200, RegClass::Float ) );
    CHECK_FALSE( VReg::make( 200, RegClass::Int ) == VReg::make( 201, RegClass::Int ) );
}

TEST_CASE( "reg default constructed is invalid", "[regs]" )
{
    const Reg r { };
    CHECK_FALSE( r.valid( ) );
    CHECK( r == Reg::invalid( ) );
    CHECK( r.bits == VReg::invalid_bits );
}

TEST_CASE( "reg from every preg round trips", "[regs]" )
{
    for ( uint8_t hw { 0 }; hw <= PReg::max_hw_enc; ++hw )
        for ( const auto rc : { RegClass::Int, RegClass::Float } )
        {
            const auto p = PReg::make( hw, rc );
            const auto r = Reg::from_preg( p );
            CHECK( r.valid( ) );
            CHECK( r.is_physical( ) );
            CHECK_FALSE( r.is_virtual( ) );
            CHECK( r.rc( ) == rc );
            CHECK( r.as_preg( ) == p );
            CHECK( r.as_vreg( ).index( ) == p.index( ) );
            CHECK( r.as_vreg( ).rc( ) == rc );
        }
}

TEST_CASE( "reg physical virtual boundary sits at pinned_count", "[regs]" )
{
    CHECK( Reg::pinned_count == 128 );
    CHECK( Reg::from_vreg( VReg::make( 0, RegClass::Int ) ).is_physical( ) );
    CHECK( Reg::from_vreg( VReg::make( 127, RegClass::Int ) ).is_physical( ) );
    CHECK( Reg::from_vreg( VReg::make( 127, RegClass::Float ) ).is_physical( ) );
    CHECK( Reg::from_vreg( VReg::make( 128, RegClass::Int ) ).is_virtual( ) );
    CHECK( Reg::from_vreg( VReg::make( 128, RegClass::Float ) ).is_virtual( ) );
    CHECK( Reg::from_vreg( VReg::make( 129, RegClass::Int ) ).is_virtual( ) );
    CHECK( Reg::from_vreg( VReg::make( VReg::max_index, RegClass::Int ) ).is_virtual( ) );
}

TEST_CASE( "reg from vreg round trips", "[regs]" )
{
    for ( const uint32_t idx : { 128u, 129u, 500u, 4096u, VReg::max_index } )
        for ( const auto rc : { RegClass::Int, RegClass::Float } )
        {
            const auto v = VReg::make( idx, rc );
            const auto r = Reg::from_vreg( v );
            CHECK( r.bits == v.bits );
            CHECK( r.as_vreg( ) == v );
            CHECK( r.rc( ) == rc );
            CHECK( r.valid( ) );
        }
}

TEST_CASE( "reg equality distinguishes physical from virtual", "[regs]" )
{
    CHECK( Reg::from_preg( x86_64::RAX ) == Reg::from_preg( x86_64::RAX ) );
    CHECK_FALSE( Reg::from_preg( x86_64::RAX ) == Reg::from_preg( x86_64::RCX ) );
    CHECK_FALSE( Reg::from_preg( x86_64::RAX ) == Reg::from_preg( x86_64::XMM0 ) );
    CHECK_FALSE( Reg::from_preg( x86_64::RAX ) == Reg::from_vreg( VReg::make( 128, RegClass::Int ) ) );
}

TEST_CASE( "preg formatter", "[regs][fmt]" )
{
    CHECK( std::format( "{}", PReg::make( 0, RegClass::Int ) ) == "r0" );
    CHECK( std::format( "{}", PReg::make( 15, RegClass::Int ) ) == "r15" );
    CHECK( std::format( "{}", PReg::make( 63, RegClass::Int ) ) == "r63" );
    CHECK( std::format( "{}", PReg::make( 0, RegClass::Float ) ) == "f0" );
    CHECK( std::format( "{}", PReg::make( 9, RegClass::Float ) ) == "f9" );
}

TEST_CASE( "vreg formatter", "[regs][fmt]" )
{
    CHECK( std::format( "{}", VReg::make( 0, RegClass::Int ) ) == "v0" );
    CHECK( std::format( "{}", VReg::make( 128, RegClass::Int ) ) == "v128" );
    CHECK( std::format( "{}", VReg::make( 777, RegClass::Float ) ) == "v777" );
}

TEST_CASE( "reg formatter", "[regs][fmt]" )
{
    CHECK( std::format( "{}", Reg::from_preg( x86_64::RAX ) ) == "r0" );
    CHECK( std::format( "{}", Reg::from_preg( x86_64::R12 ) ) == "r12" );
    CHECK( std::format( "{}", Reg::from_preg( x86_64::XMM3 ) ) == "f3" );
    CHECK( std::format( "{}", Reg::from_vreg( VReg::make( 128, RegClass::Int ) ) ) == "v128" );
    CHECK( std::format( "{}", Reg::from_vreg( VReg::make( 4000, RegClass::Float ) ) ) == "v4000" );
}

TEST_CASE( "machoperand any round trips every kind and position", "[regs][operand]" )
{
    const uint32_t indices[] = { 0, 1, 127, 128, 129, 1000, 65536, VReg::max_index };
    for ( const auto idx : indices )
        for ( const auto rc : { RegClass::Int, RegClass::Float } )
            for ( const auto kind : { OperandKind::Use, OperandKind::Def, OperandKind::UseDef } )
                for ( const auto pos : { OperandPos::Before, OperandPos::After } )
                {
                    const auto v  = VReg::make( idx, rc );
                    const auto op = MachOperand::any( v, kind, pos );
                    CHECK( op.vreg( ) == v );
                    CHECK( op.kind( ) == kind );
                    CHECK( op.pos( ) == pos );
                    CHECK_FALSE( op.is_fixed( ) );
                    CHECK_FALSE( op.fixed_preg( ).has_value( ) );
                }
}

TEST_CASE( "machoperand fixed round trips every preg", "[regs][operand]" )
{
    for ( uint8_t hw { 0 }; hw <= PReg::max_hw_enc; ++hw )
        for ( const auto rc : { RegClass::Int, RegClass::Float } )
            for ( const auto kind : { OperandKind::Use, OperandKind::Def, OperandKind::UseDef } )
                for ( const auto pos : { OperandPos::Before, OperandPos::After } )
                {
                    const auto p  = PReg::make( hw, rc );
                    const auto v  = VReg::make( 300 + hw, rc );
                    const auto op = MachOperand::fixed( v, kind, pos, p );
                    CHECK( op.is_fixed( ) );
                    REQUIRE( op.fixed_preg( ).has_value( ) );
                    CHECK( *op.fixed_preg( ) == p );
                    CHECK( op.vreg( ) == v );
                    CHECK( op.kind( ) == kind );
                    CHECK( op.pos( ) == pos );
                }
}

TEST_CASE( "machoperand fixed with max vreg keeps every field", "[regs][operand]" )
{
    const auto v  = VReg::make( VReg::max_index, RegClass::Float );
    const auto op = MachOperand::fixed( v, OperandKind::UseDef, OperandPos::After, x86_64::XMM15 );
    CHECK( op.vreg( ) == v );
    CHECK( op.kind( ) == OperandKind::UseDef );
    CHECK( op.pos( ) == OperandPos::After );
    CHECK( *op.fixed_preg( ) == x86_64::XMM15 );
}

TEST_CASE( "machoperand is eight bytes and default zero", "[regs][operand]" )
{
    STATIC_CHECK( sizeof( MachOperand ) == 8 );
    const MachOperand op { };
    CHECK( op.bits == 0 );
    CHECK( op.kind( ) == OperandKind::Use );
    CHECK( op.pos( ) == OperandPos::Before );
    CHECK_FALSE( op.is_fixed( ) );
    CHECK( op.vreg( ) == VReg::make( 0, RegClass::Int ) );
}

TEST_CASE( "machoperand equality", "[regs][operand]" )
{
    const auto v = VReg::make( 200, RegClass::Int );
    CHECK( MachOperand::any( v, OperandKind::Use, OperandPos::Before ) == MachOperand::any( v, OperandKind::Use, OperandPos::Before ) );
    CHECK_FALSE( MachOperand::any( v, OperandKind::Use, OperandPos::Before )
                 == MachOperand::any( v, OperandKind::Def, OperandPos::Before ) );
    CHECK_FALSE( MachOperand::any( v, OperandKind::Use, OperandPos::Before )
                 == MachOperand::any( v, OperandKind::Use, OperandPos::After ) );
    CHECK_FALSE( MachOperand::any( v, OperandKind::Use, OperandPos::Before )
                 == MachOperand::fixed( v, OperandKind::Use, OperandPos::Before, x86_64::RAX ) );
    CHECK_FALSE( MachOperand::fixed( v, OperandKind::Use, OperandPos::Before, x86_64::RAX )
                 == MachOperand::fixed( v, OperandKind::Use, OperandPos::Before, x86_64::RCX ) );
}

TEST_CASE( "machoperand formatter", "[regs][operand][fmt]" )
{
    const auto v = VReg::make( 200, RegClass::Int );
    CHECK( std::format( "{}", MachOperand::any( v, OperandKind::Use, OperandPos::Before ) ) == "use(v200, before)" );
    CHECK( std::format( "{}", MachOperand::any( v, OperandKind::Def, OperandPos::After ) ) == "def(v200, after)" );
    CHECK( std::format( "{}", MachOperand::any( v, OperandKind::UseDef, OperandPos::After ) ) == "usedef(v200, after)" );
    CHECK( std::format( "{}", MachOperand::fixed( v, OperandKind::Use, OperandPos::Before, x86_64::RCX ) )
           == "use(v200, fixed=r1, before)" );
    CHECK( std::format( "{}", MachOperand::fixed( v, OperandKind::Def, OperandPos::After, x86_64::XMM2 ) )
           == "def(v200, fixed=f2, after)" );
}

TEST_CASE( "x86 integer register constants", "[regs][x86]" )
{
    const std::array< PReg, 16 > regs
        = { x86_64::RAX, x86_64::RCX, x86_64::RDX, x86_64::RBX, x86_64::RSP, x86_64::RBP, x86_64::RSI, x86_64::RDI,
            x86_64::R8,  x86_64::R9,  x86_64::R10, x86_64::R11, x86_64::R12, x86_64::R13, x86_64::R14, x86_64::R15 };
    for ( uint8_t i { 0 }; i < regs.size( ); ++i )
    {
        CHECK( regs[ i ].hw( ) == i );
        CHECK( regs[ i ].rc( ) == RegClass::Int );
    }
}

TEST_CASE( "x86 float register constants", "[regs][x86]" )
{
    const std::array< PReg, 16 > regs
        = { x86_64::XMM0, x86_64::XMM1, x86_64::XMM2,  x86_64::XMM3,  x86_64::XMM4,  x86_64::XMM5,  x86_64::XMM6,  x86_64::XMM7,
            x86_64::XMM8, x86_64::XMM9, x86_64::XMM10, x86_64::XMM11, x86_64::XMM12, x86_64::XMM13, x86_64::XMM14, x86_64::XMM15 };
    for ( uint8_t i { 0 }; i < regs.size( ); ++i )
    {
        CHECK( regs[ i ].hw( ) == i );
        CHECK( regs[ i ].rc( ) == RegClass::Float );
    }
}

TEST_CASE( "x86 preg names for integer registers", "[regs][x86][fmt]" )
{
    const char* names[] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
    for ( uint8_t i { 0 }; i < 16; ++i ) CHECK( x86_64::preg_name( PReg::make( i, RegClass::Int ) ) == names[ i ] );
}

TEST_CASE( "x86 preg names for float registers", "[regs][x86][fmt]" )
{
    for ( uint8_t i { 0 }; i < 16; ++i ) CHECK( x86_64::preg_name( PReg::make( i, RegClass::Float ) ) == std::format( "xmm{}", i ) );
}

TEST_CASE( "x86 preg names out of range", "[regs][x86][fmt]" )
{
    CHECK( x86_64::preg_name( PReg::make( 16, RegClass::Int ) ) == "<?>" );
    CHECK( x86_64::preg_name( PReg::make( 63, RegClass::Int ) ) == "<?>" );
    CHECK( x86_64::preg_name( PReg::make( 16, RegClass::Float ) ) == "<?>" );
    CHECK( x86_64::preg_name( PReg::make( 40, RegClass::Float ) ) == "<?>" );
}
