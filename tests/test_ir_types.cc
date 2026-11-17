#include "support.hh"

#include <catch2/generators/catch_generators.hpp>

#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <utility>

using namespace xt;

TEST_CASE( "is_int accepts integer and pointer types", "[ir][types]" )
{
    STATIC_REQUIRE( ir::is_int( Type::i8 ) );
    STATIC_REQUIRE( ir::is_int( Type::i16 ) );
    STATIC_REQUIRE( ir::is_int( Type::i32 ) );
    STATIC_REQUIRE( ir::is_int( Type::i64 ) );
    STATIC_REQUIRE( ir::is_int( Type::ptr ) );
}

TEST_CASE( "is_int rejects float and unit types", "[ir][types]" )
{
    STATIC_REQUIRE_FALSE( ir::is_int( Type::f32 ) );
    STATIC_REQUIRE_FALSE( ir::is_int( Type::f64 ) );
    STATIC_REQUIRE_FALSE( ir::is_int( Type::unit ) );
}

TEST_CASE( "bit_width of every type", "[ir][types]" )
{
    STATIC_REQUIRE( ir::bit_width( Type::i8 ) == 8 );
    STATIC_REQUIRE( ir::bit_width( Type::i16 ) == 16 );
    STATIC_REQUIRE( ir::bit_width( Type::i32 ) == 32 );
    STATIC_REQUIRE( ir::bit_width( Type::i64 ) == 64 );
    STATIC_REQUIRE( ir::bit_width( Type::f32 ) == 32 );
    STATIC_REQUIRE( ir::bit_width( Type::f64 ) == 64 );
    STATIC_REQUIRE( ir::bit_width( Type::ptr ) == 64 );
    STATIC_REQUIRE( ir::bit_width( Type::unit ) == 0 );
}

TEST_CASE( "bit_width of an out of range type is zero", "[ir][types]" )
{ CHECK( ir::bit_width( static_cast< Type >( 200 ) ) == 0 ); }

TEST_CASE( "type enum is one byte", "[ir][types]" )
{
    STATIC_REQUIRE( sizeof( Type ) == 1 );
    STATIC_REQUIRE( sizeof( ir::ICmpCC ) == 1 );
}

TEST_CASE( "Type formatter names every type", "[ir][types][fmt]" )
{
    const auto [ ty, name ] = GENERATE( values< std::pair< Type, std::string > >( {
        {  Type::i8,   "i8"},
        { Type::i16,  "i16"},
        { Type::i32,  "i32"},
        { Type::i64,  "i64"},
        { Type::f32,  "f32"},
        { Type::f64,  "f64"},
        { Type::ptr,  "ptr"},
        {Type::unit, "unit"},
    } ) );
    CHECK( std::format( "{}", ty ) == name );
}

TEST_CASE( "IROp formatter names every opcode", "[ir][types][fmt]" )
{
    using enum ir::IROp;
    const auto [ op, name ] = GENERATE( values< std::pair< ir::IROp, std::string > >( {
        {       iconst,        "iconst"},
        {         iadd,          "iadd"},
        {         isub,          "isub"},
        {         imul,          "imul"},
        {         idiv,          "idiv"},
        {         icmp,          "icmp"},
        {           br,            "br"},
        {         brif,          "brif"},
        {          ret,           "ret"},
        {         call,          "call"},
        {        store,         "store"},
        {         load,          "load"},
        {         band,          "band"},
        {          bor,           "bor"},
        {         bxor,          "bxor"},
        {         bnot,          "bnot"},
        {         ineg,          "ineg"},
        {         ishl,          "ishl"},
        {         ushr,          "ushr"},
        {         sshr,          "sshr"},
        {         udiv,          "udiv"},
        {         srem,          "srem"},
        {         urem,          "urem"},
        {       select,        "select"},
        {      uextend,       "uextend"},
        {      sextend,       "sextend"},
        {      ireduce,       "ireduce"},
        { symbol_value,  "symbol_value"},
        {    func_addr,     "func_addr"},
        {call_indirect, "call_indirect"},
        {   stack_addr,    "stack_addr"},
    } ) );
    CHECK( std::format( "{}", op ) == name );
}

TEST_CASE( "IROp formatter writes nothing for an unknown opcode", "[ir][types][fmt]" )
{ CHECK( std::format( "[{}]", static_cast< ir::IROp >( 999 ) ) == "[]" ); }

TEST_CASE( "ICmpCC formatter names every condition", "[ir][types][fmt]" )
{
    using enum ir::ICmpCC;
    const auto [ cc, name ] = GENERATE( values< std::pair< ir::ICmpCC, std::string > >( {
        { eq,  "eq"},
        { ne,  "ne"},
        {slt, "slt"},
        {sle, "sle"},
        {sgt, "sgt"},
        {sge, "sge"},
        {ult, "ult"},
        {ule, "ule"},
        {ugt, "ugt"},
        {uge, "uge"},
    } ) );
    CHECK( std::format( "{}", cc ) == name );
}

TEST_CASE( "Ref formatter prints the raw id", "[ir][types][fmt]" )
{
    CHECK( std::format( "{}", ir::ValueRef { 0 } ) == "0" );
    CHECK( std::format( "{}", ir::BlockRef { 7 } ) == "7" );
    CHECK( std::format( "{}", ir::InstRef { 123456 } ) == "123456" );
    CHECK( std::format( "{}", ir::FuncRef { 3 } ) == "3" );
    CHECK( std::format( "{}", ir::SigRef { 1 } ) == "1" );
    CHECK( std::format( "{}", ir::GlobalValueRef { 9 } ) == "9" );
    CHECK( std::format( "{}", ir::StringRef { 2 } ) == "2" );
    CHECK( std::format( "{}", ir::FunctionRef { 4 } ) == "4" );
    CHECK( std::format( "{}", ir::ImmRef { 5 } ) == "5" );
    CHECK( std::format( "{}", ir::ValueRef { ~0u } ) == "4294967295" );
}

TEST_CASE( "Ref is a plain four byte index", "[ir][types]" )
{
    STATIC_REQUIRE( sizeof( ir::ValueRef ) == 4 );
    STATIC_REQUIRE( sizeof( ir::BlockRef ) == 4 );
    STATIC_REQUIRE( std::is_trivially_copyable_v< ir::ValueRef > );
    STATIC_REQUIRE_FALSE( std::is_same_v< ir::ValueRef, ir::BlockRef > );
    STATIC_REQUIRE_FALSE( std::is_same_v< ir::FuncRef, ir::FunctionRef > );
}

TEST_CASE( "Value formatter prints type and owning instruction", "[ir][types][fmt]" )
{
    CHECK( std::format( "{}", ir::Value { .type = Type::i32, .inst = ir::InstRef { 12 } } ) == "i32:v12" );
    CHECK( std::format( "{}", ir::Value { .type = Type::ptr, .inst = ir::InstRef { 0 } } ) == "ptr:v0" );
    CHECK( std::format( "{}", ir::Value { .type = Type::unit, .inst = ir::InstRef { ~0u } } ) == "unit:v4294967295" );
}

TEST_CASE( "IRBlock formatter prints its ranges", "[ir][types][fmt]" )
{
    const ir::IRBlock b {
        .insts_offset  = 2,
        .insts_count   = 3,
        .succs_offset  = 1,
        .succs_count   = 2,
        .params_offset = 9,
        .params_count  = 4,
    };
    CHECK( std::format( "{}", b ) == "block(insts=2..5, succs=1..3, params=4)" );
    CHECK( std::format( "{}", ir::IRBlock { } ) == "block(insts=0..0, succs=0..0, params=0)" );
}

TEST_CASE( "IRInst formatter for iconst prints the raw immediate", "[ir][types][fmt]" )
{
    ir::IRInst i { };
    i.op     = ir::IROp::iconst;
    i.result = ir::ValueRef { 4 };
    i.imm    = 99;
    CHECK( std::format( "{}", i ) == "v4 = iconst i64 #99" );
    i.imm = ~uint64_t { 0 };
    CHECK( std::format( "{}", i ) == "v4 = iconst i64 #18446744073709551615" );
}

TEST_CASE( "IRInst formatter for icmp prints condition and operand slots", "[ir][types][fmt]" )
{
    ir::IRInst i { };
    i.op         = ir::IROp::icmp;
    i.cc         = ir::ICmpCC::sge;
    i.result     = ir::ValueRef { 6 };
    i.ops_offset = 10;
    i.ops_count  = 2;
    CHECK( std::format( "{}", i ) == "v6 = icmp sge v10 v11" );
}

TEST_CASE( "IRInst formatter for other ops prints the operand range", "[ir][types][fmt]" )
{
    ir::IRInst i { };
    i.op         = ir::IROp::iadd;
    i.result     = ir::ValueRef { 3 };
    i.ops_offset = 4;
    i.ops_count  = 2;
    CHECK( std::format( "{}", i ) == "v3 = iadd [ops 4..6]" );

    i.op         = ir::IROp::ret;
    i.result     = ir::ValueRef { 1 };
    i.ops_offset = 0;
    i.ops_count  = 0;
    CHECK( std::format( "{}", i ) == "v1 = ret [ops 0..0]" );

    i.op         = ir::IROp::call_indirect;
    i.result     = ir::ValueRef { 8 };
    i.ops_offset = 5;
    i.ops_count  = 3;
    CHECK( std::format( "{}", i ) == "v8 = call_indirect [ops 5..8]" );
}

TEST_CASE( "IRInst defaults", "[ir][types]" )
{
    const ir::IRInst i { };
    CHECK( i.op == ir::IROp::iconst );
    CHECK( i.cc == ir::ICmpCC::eq );
    CHECK( i.result.id == 0 );
    CHECK( i.ops_offset == 0 );
    CHECK( i.ops_count == 0 );
    CHECK( i.imm == 0 );
}

TEST_CASE( "mem_offset sign extends the low 32 bits of the immediate", "[ir][types]" )
{
    const auto [ imm, expect ] = GENERATE( values< std::pair< uint64_t, int32_t > >( {
        {                                                                            0,                                      0},
        {                                                                            8,                                      8},
        {                                    static_cast< uint64_t >( int64_t { -1 } ),                                     -1},
        {                                 static_cast< uint64_t >( int64_t { -4096 } ),                                  -4096},
        {static_cast< uint64_t >( int64_t { std::numeric_limits< int32_t >::min( ) } ), std::numeric_limits< int32_t >::min( )},
        {            static_cast< uint64_t >( std::numeric_limits< int32_t >::max( ) ), std::numeric_limits< int32_t >::max( )},
        {                                                                0x80000000ull, std::numeric_limits< int32_t >::min( )},
        {                                                                0xFFFFFFFFull,                                     -1},
        {                                                             0x1'0000'0005ull,                                      5},
    } ) );
    ir::IRInst i { };
    i.imm = imm;
    CHECK( ir::mem_offset( i ) == expect );
}

TEST_CASE( "mem_offset is usable in constant expressions", "[ir][types]" )
{
    constexpr ir::IRInst i { .imm = 0xFFFFFFFFFFFFFFF0ull };
    STATIC_REQUIRE( ir::mem_offset( i ) == -16 );
}

TEST_CASE( "ExternalName equality compares namespace and index", "[ir][types]" )
{
    constexpr ir::ExternalName a { .ns = 0, .index = 3 };
    constexpr ir::ExternalName b { .ns = 0, .index = 3 };
    constexpr ir::ExternalName c { .ns = 1, .index = 3 };
    constexpr ir::ExternalName d { .ns = 0, .index = 4 };
    STATIC_REQUIRE( a == b );
    STATIC_REQUIRE_FALSE( a == c );
    STATIC_REQUIRE_FALSE( a == d );
    STATIC_REQUIRE( a != c );
    STATIC_REQUIRE( ir::ExternalName { } == ir::ExternalName { 0, 0 } );
}

TEST_CASE( "external name namespaces for functions and data", "[ir][types]" )
{
    STATIC_REQUIRE( ir::extns::function == 0 );
    STATIC_REQUIRE( ir::extns::data == 1 );
    STATIC_REQUIRE( ir::extns::function != ir::extns::data );
}

TEST_CASE( "host_call_conv matches the platform", "[ir][types]" )
{
#if defined( _WIN64 )
    STATIC_REQUIRE( codegen::host_call_conv( ) == codegen::CallConv::WindowsFastcall );
#else
    STATIC_REQUIRE( codegen::host_call_conv( ) == codegen::CallConv::SystemV );
#endif
}

TEST_CASE( "IRSig defaults to the host calling convention", "[ir][types]" )
{
    const ir::IRSig sig { };
    CHECK( sig.cc == codegen::host_call_conv( ) );
    CHECK( sig.params.empty( ) );
}
