#include "support.hh"

#include <xiao/isa/x86/inst/inst.hh>
#include <xiao/isa/x86/inst/regs.hh>

#include <cstddef>
#include <utility>
#include <vector>

using namespace xt;
using namespace xiao::x86_64;
using codegen::MachOperand;
using codegen::MachTerminator;
using codegen::OperandKind;
using codegen::OperandPos;
using codegen::PReg;
using codegen::Reg;
using codegen::RegClass;
using codegen::VReg;

namespace
{
    [[nodiscard]] Reg v( const uint32_t n )
    { return Reg::from_vreg( VReg::make( n, RegClass::Int ) ); }

    [[nodiscard]] Reg p( const PReg r )
    { return Reg::from_preg( r ); }

    [[nodiscard]] GprMem rm( const Reg r )
    { return GprMem { r }; }

    [[nodiscard]] GprMem mem( const AddressMode& m )
    { return GprMem { m }; }

    [[nodiscard]] std::vector< MachOperand > ops_of( const X86Inst& i )
    {
        std::vector< MachOperand > ops;
        i.get_operands( ops );
        return ops;
    }

    [[nodiscard]] MachOperand use( const uint32_t n )
    { return MachOperand::any( VReg::make( n, RegClass::Int ), OperandKind::Use, OperandPos::Before ); }

    [[nodiscard]] MachOperand def( const uint32_t n )
    { return MachOperand::any( VReg::make( n, RegClass::Int ), OperandKind::Def, OperandPos::After ); }

    [[nodiscard]] MachOperand usedef( const uint32_t n )
    { return MachOperand::any( VReg::make( n, RegClass::Int ), OperandKind::UseDef, OperandPos::After ); }

    [[nodiscard]] MachOperand fixed( const PReg r, const OperandKind k )
    {
        return MachOperand::fixed( VReg::make( r.index( ), r.rc( ) ), k, k == OperandKind::Use ? OperandPos::Before : OperandPos::After,
                                   r );
    }

    template < typename... Ts >
    struct EveryKind
    {
        [[nodiscard]] static std::vector< X86Inst > make( )
        {
            std::vector< X86Inst > all;
            all.reserve( sizeof...( Ts ) );
            ( all.emplace_back( Ts { } ), ... );
            return all;
        }
    };

    [[nodiscard]] std::vector< X86Inst > every_kind( )
    { return inst::all_kinds< EveryKind >::make( ); }
} // namespace

TEST_CASE( "x86 inst satisfies the machinst concept", "[x86][inst]" )
{
    STATIC_CHECK( codegen::MachInstC< X86Inst > );
    STATIC_CHECK( codegen::LabelUseC< LabelUse > );
    CHECK( every_kind( ).size( ) == inst::kind_count );
}

TEST_CASE( "x86 alu operands write dst except for cmp and test", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::addq_mr { rm( v( 200 ) ), v( 201 ) } ) == std::vector { usedef( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::subq_mr { rm( v( 200 ) ), v( 201 ) } ) == std::vector { usedef( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::andq_mr { rm( v( 200 ) ), v( 201 ) } ) == std::vector { usedef( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::orq_mr { rm( v( 200 ) ), v( 201 ) } ) == std::vector { usedef( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::xorq_mr { rm( v( 200 ) ), v( 201 ) } ) == std::vector { usedef( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::imulq_rm { v( 200 ), rm( v( 201 ) ) } ) == std::vector { usedef( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::cmpq_mr { rm( v( 200 ) ), v( 201 ) } ) == std::vector { use( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::testq_mr { rm( v( 200 ) ), v( 201 ) } ) == std::vector { use( 200 ), use( 201 ) } );
}

TEST_CASE( "x86 alu skips invalid registers", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::addq_mr { rm( Reg::invalid( ) ), v( 201 ) } ) == std::vector { use( 201 ) } );
    CHECK( ops_of( inst::addq_mr { rm( v( 200 ) ), Reg::invalid( ) } ) == std::vector { usedef( 200 ) } );
    CHECK( ops_of( inst::addq_mr { } ).empty( ) );
}

TEST_CASE( "x86 alu immediate operands", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::addq_mi8 { rm( v( 200 ) ), 3 } ) == std::vector { usedef( 200 ) } );
    CHECK( ops_of( inst::xorq_mi32 { rm( v( 200 ) ), 3 } ) == std::vector { usedef( 200 ) } );
    CHECK( ops_of( inst::cmpq_mi8 { rm( v( 200 ) ), 3 } ) == std::vector { use( 200 ) } );
    CHECK( ops_of( inst::cmpq_mi32 { rm( v( 200 ) ), 3 } ) == std::vector { use( 200 ) } );
    CHECK( ops_of( inst::subq_mi8 { rm( p( RSP ) ), 8 } ) == std::vector { fixed( RSP, OperandKind::UseDef ) } );
    CHECK( ops_of( inst::imulq_rmi8 { v( 200 ), rm( v( 201 ) ), 3 } ) == std::vector { def( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::imulq_rmi32 { v( 200 ), rm( v( 201 ) ), 300 } ) == std::vector { def( 200 ), use( 201 ) } );
}

TEST_CASE( "x86 unary and shift operands", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::notq_m { rm( v( 200 ) ) } ) == std::vector { usedef( 200 ) } );
    CHECK( ops_of( inst::negq_m { rm( v( 200 ) ) } ) == std::vector { usedef( 200 ) } );
    CHECK( ops_of( inst::sarq_mc { rm( v( 200 ) ), p( RCX ) } ) == std::vector { usedef( 200 ), fixed( RCX, OperandKind::Use ) } );
    CHECK( ops_of( inst::shrq_mc { rm( v( 200 ) ), v( 201 ) } )
           == std::vector { usedef( 200 ),
                            MachOperand::fixed( VReg::make( 201, RegClass::Int ), OperandKind::Use, OperandPos::Before, RCX ) } );
    CHECK( ops_of( inst::shlq_mi8 { rm( v( 200 ) ), 4 } ) == std::vector { usedef( 200 ) } );
}

TEST_CASE( "x86 test cmov and setcc operands", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::testb_mr { rm( v( 200 ) ), v( 200 ) } ) == std::vector { use( 200 ), use( 200 ) } );
    CHECK( ops_of( inst::cmovq_rm { v( 200 ), rm( v( 201 ) ), CC::L } ) == std::vector { usedef( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::setcc_m8 { rm( v( 200 ) ), CC::G } ) == std::vector { def( 200 ) } );
}

TEST_CASE( "x86 mov operands with virtual and physical registers", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::movq_mr { rm( v( 200 ) ), v( 201 ) } ) == std::vector { def( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::movq_mr { rm( p( RAX ) ), v( 201 ) } ) == std::vector { fixed( RAX, OperandKind::Def ), use( 201 ) } );
    CHECK( ops_of( inst::movq_mr { rm( v( 200 ) ), p( RCX ) } ) == std::vector { def( 200 ), fixed( RCX, OperandKind::Use ) } );
    CHECK( ops_of( inst::movq_mr { rm( p( R8 ) ), p( R9 ) } )
           == std::vector { fixed( R8, OperandKind::Def ), fixed( R9, OperandKind::Use ) } );
    CHECK( ops_of( inst::MovImm { v( 200 ), -5 } ) == std::vector { def( 200 ) } );
    CHECK( ops_of( inst::MovImm { p( RDX ), 0 } ) == std::vector { fixed( RDX, OperandKind::Def ) } );
}

TEST_CASE( "x86 memory operands", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::movq_rm { v( 200 ), mem( { .base = v( 201 ) } ) } ) == std::vector { def( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::movq_rm { v( 200 ), mem( { .base = v( 201 ), .index = v( 202 ), .scale = 3 } ) } )
           == std::vector { def( 200 ), use( 201 ), use( 202 ) } );
    CHECK( ops_of( inst::movq_mr { mem( { .base = v( 201 ) } ), v( 200 ) } ) == std::vector { use( 201 ), use( 200 ) } );
    CHECK( ops_of( inst::movb_mr { mem( { .base = v( 201 ), .index = v( 202 ) } ), v( 200 ) } )
           == std::vector { use( 201 ), use( 202 ), use( 200 ) } );
    CHECK( ops_of( inst::movl_rm { v( 200 ), mem( { .base = p( RSP ), .disp = 8 } ) } )
           == std::vector { def( 200 ), fixed( RSP, OperandKind::Use ) } );
    CHECK( ops_of( inst::addq_mr { mem( { .base = v( 201 ) } ), v( 200 ) } ) == std::vector { use( 201 ), use( 200 ) } );
}

TEST_CASE( "x86 extension operands", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::movzxq_rm8 { v( 200 ), rm( v( 201 ) ) } ) == std::vector { def( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::movzxq_rm16 { v( 200 ), rm( v( 201 ) ) } ) == std::vector { def( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::movl_rm { v( 200 ), rm( v( 201 ) ) } ) == std::vector { def( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::movsxq_rm8 { v( 200 ), rm( v( 201 ) ) } ) == std::vector { def( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::movsxq_rm16 { v( 200 ), rm( v( 201 ) ) } ) == std::vector { def( 200 ), use( 201 ) } );
    CHECK( ops_of( inst::movsxdq_rm32 { v( 200 ), rm( v( 201 ) ) } ) == std::vector { def( 200 ), use( 201 ) } );
}

TEST_CASE( "x86 division operands pin rax and rdx", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::cqo { p( RAX ), p( RDX ) } ) == std::vector { fixed( RAX, OperandKind::Use ), fixed( RDX, OperandKind::Def ) } );
    CHECK( ops_of( inst::cqo { v( 300 ), v( 301 ) } )
           == std::vector { MachOperand::fixed( VReg::make( 300, RegClass::Int ), OperandKind::Use, OperandPos::Before, RAX ),
                            MachOperand::fixed( VReg::make( 301, RegClass::Int ), OperandKind::Def, OperandPos::After, RDX ) } );
    CHECK( ops_of( inst::divq_m { rm( v( 200 ) ), p( RAX ), p( RDX ) } )
           == std::vector { use( 200 ), fixed( RAX, OperandKind::UseDef ), fixed( RDX, OperandKind::UseDef ) } );
    CHECK( ops_of( inst::idivq_m { rm( v( 200 ) ), p( RAX ), p( RDX ) } )
           == std::vector { use( 200 ), fixed( RAX, OperandKind::UseDef ), fixed( RDX, OperandKind::UseDef ) } );
}

TEST_CASE( "x86 stack and frame operands", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::leaq_rm { v( 200 ), stack_slot( 0, 16 ) } ) == std::vector { def( 200 ) } );
    CHECK( ops_of( inst::DummyUse { v( 200 ) } ) == std::vector { use( 200 ) } );
    CHECK( ops_of( inst::DummyUse { } ).empty( ) );
}

TEST_CASE( "x86 control flow has no register operands", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::Jmp { { 4 } } ).empty( ) );
    CHECK( ops_of( inst::Jcc { CC::NE, { 4 } } ).empty( ) );
    CHECK( ops_of( inst::ret { } ).empty( ) );
}

TEST_CASE( "x86 call operands are fixed argument uses and a fixed return def", "[x86][inst][operands]" )
{
    const inst::Call call {
        .name = { 0, 2 },
        .info
        = xiao::support::Box { inst::CallInfo { .args = { RCX, RDX, R8 }, .ret = RAX, .clobbers = { RAX, RCX, RDX, R8, R9, R10, R11 } } }
    };
    CHECK( ops_of( call )
           == std::vector { fixed( RCX, OperandKind::Use ), fixed( RDX, OperandKind::Use ), fixed( R8, OperandKind::Use ),
                            fixed( RAX, OperandKind::Def ) } );
    CHECK( ops_of( inst::Call {
                       .name = { 0, 2 }
    } )
               .empty( ) );
    CHECK( ops_of( inst::Call {
               .name = { 0, 2 },
               .info = xiao::support::Box { inst::CallInfo { .ret = RAX } }
    } )
           == std::vector { fixed( RAX, OperandKind::Def ) } );
}

TEST_CASE( "x86 indirect call uses the target first", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::CallInd { .target = v( 200 ), .info = xiao::support::Box { inst::CallInfo { .args = { RCX }, .ret = RAX } } } )
           == std::vector { use( 200 ), fixed( RCX, OperandKind::Use ), fixed( RAX, OperandKind::Def ) } );
    CHECK( ops_of( inst::CallInd { .target = v( 200 ) } ) == std::vector { use( 200 ) } );
}

TEST_CASE( "x86 load ext name defines its destination", "[x86][inst][operands]" )
{
    CHECK( ops_of( inst::LoadExtName {
               .dst      = v( 200 ),
               .name     = { 1, 0 },
               .distance = RelocDistance::Near
    } )
           == std::vector { def( 200 ) } );
    CHECK( ops_of( inst::LoadExtName {
               .dst  = v( 200 ),
               .name = { 0, 0 }
    } )
           == std::vector { def( 200 ) } );
}

TEST_CASE( "x86 get_operands appends to an existing list", "[x86][inst][operands]" )
{
    std::vector< MachOperand > ops { use( 999 ) };
    X86Inst {
        inst::movq_mr { rm( v( 200 ) ), v( 201 ) }
    }
        .get_operands( ops );
    CHECK( ops == std::vector { use( 999 ), def( 200 ), use( 201 ) } );
}

TEST_CASE( "x86 is_term classifies terminators", "[x86][inst]" )
{
    CHECK( X86Inst { inst::ret { } }.is_term( ) == MachTerminator::Ret );
    CHECK( X86Inst { inst::Jmp { { 0 } } }.is_term( ) == MachTerminator::Branch );
    CHECK( X86Inst {
               inst::Jcc { CC::E, { 0 } }
    }
               .is_term( )
           == MachTerminator::Branch );

    std::size_t terms { 0 };
    for ( const auto& i : every_kind( ) )
        if ( i.is_term( ) != MachTerminator::None ) ++terms;
    CHECK( terms == 3 );
}

TEST_CASE( "x86 is_call only for direct and indirect calls", "[x86][inst]" )
{
    std::size_t calls { 0 };
    for ( const auto& i : every_kind( ) )
        if ( i.is_call( ) ) ++calls;
    CHECK( calls == 2 );
    CHECK( X86Inst { inst::Call { } }.is_call( ) );
    CHECK( X86Inst { inst::CallInd { } }.is_call( ) );
    CHECK_FALSE( X86Inst { inst::LoadExtName { } }.is_call( ) );
}

TEST_CASE( "x86 is_mem_access when an operand is memory", "[x86][inst]" )
{
    CHECK( X86Inst {
        inst::movq_rm { v( 200 ), mem( { .base = v( 201 ) } ) }
    }
               .is_mem_access( ) );
    CHECK( X86Inst {
        inst::movb_mr { mem( { .base = v( 201 ) } ), v( 200 ) }
    }
               .is_mem_access( ) );
    CHECK( X86Inst {
        inst::addq_mr { mem( { .base = v( 201 ) } ), v( 200 ) }
    }
               .is_mem_access( ) );
    CHECK_FALSE( X86Inst {
        inst::movq_rm { v( 200 ), rm( v( 201 ) ) }
    }
                     .is_mem_access( ) );
    CHECK_FALSE( X86Inst { inst::MovImm { } }.is_mem_access( ) );

    CHECK_FALSE( X86Inst { inst::leaq_rm { } }.is_mem_access( ) );
}

TEST_CASE( "x86 as_move only recognises a register to register movq_mr", "[x86][inst]" )
{
    const auto m = X86Inst { inst::movq_mr { rm( v( 200 ) ), p( RCX ) } }.as_move( );
    REQUIRE( m.has_value( ) );
    CHECK( m->first == v( 200 ) );
    CHECK( m->second == p( RCX ) );

    std::size_t moves { 0 };
    for ( const auto& i : every_kind( ) )
        if ( i.as_move( ) ) ++moves;
    CHECK( moves == 1 );
    CHECK_FALSE( X86Inst {
        inst::movq_mr { mem( { .base = v( 201 ) } ), v( 200 ) }
    }
                     .as_move( ) );
    CHECK_FALSE( X86Inst {
        inst::movq_rm { v( 200 ), rm( v( 201 ) ) }
    }
                     .as_move( ) );
    CHECK_FALSE( X86Inst {
        inst::MovImm { v( 200 ), 0 }
    }
                     .as_move( ) );
    CHECK_FALSE( X86Inst {
        inst::movl_rm { v( 200 ), rm( v( 201 ) ) }
    }
                     .as_move( ) );
    CHECK_FALSE( X86Inst {
        inst::cmovq_rm { v( 200 ), rm( v( 201 ) ), CC::E }
    }
                     .as_move( ) );
}

TEST_CASE( "x86 clobbers come from calls only", "[x86][inst]" )
{
    const X86Inst call {
        inst::Call { .name = { 0, 0 }, .info = xiao::support::Box { inst::CallInfo { .clobbers = { RAX, RCX, R11 } } } }
    };
    const auto c = call.clobbers( );
    CHECK( std::vector< PReg >( c.begin( ), c.end( ) ) == std::vector< PReg > { RAX, RCX, R11 } );

    const X86Inst ind {
        inst::CallInd { .target = v( 200 ), .info = xiao::support::Box { inst::CallInfo { .clobbers = { RDX } } } }
    };
    CHECK( ind.clobbers( ).size( ) == 1 );
    CHECK( ind.clobbers( )[ 0 ] == RDX );

    for ( const auto& i : every_kind( ) )
        if ( !i.is_call( ) ) CHECK( i.clobbers( ).empty( ) );
}

TEST_CASE( "x86 to_string for arithmetic", "[x86][inst][fmt]" )
{
    CHECK( X86Inst {
               inst::addq_mr { rm( v( 200 ) ), v( 201 ) }
    }
               .to_string( )
           == "add v200, v201" );
    CHECK( X86Inst {
               inst::subq_mr { rm( p( RAX ) ), p( RCX ) }
    }
               .to_string( )
           == "sub rax, rcx" );
    CHECK( X86Inst {
               inst::imulq_rm { v( 200 ), rm( v( 201 ) ) }
    }
               .to_string( )
           == "imul v200, v201" );
    CHECK( X86Inst {
               inst::imulq_rmi8 { v( 200 ), rm( v( 201 ) ), 3 }
    }
               .to_string( )
           == "imul v200, v201, 3" );
    CHECK( X86Inst {
               inst::andq_mr { rm( v( 200 ) ), v( 201 ) }
    }
               .to_string( )
           == "and v200, v201" );
    CHECK( X86Inst {
               inst::orq_mr { rm( v( 200 ) ), v( 201 ) }
    }
               .to_string( )
           == "or v200, v201" );
    CHECK( X86Inst {
               inst::xorq_mr { rm( v( 200 ) ), v( 201 ) }
    }
               .to_string( )
           == "xor v200, v201" );
    CHECK( X86Inst {
               inst::cmpq_mr { rm( v( 200 ) ), v( 201 ) }
    }
               .to_string( )
           == "cmp v200, v201" );
    CHECK( X86Inst {
               inst::testq_mr { rm( v( 200 ) ), v( 201 ) }
    }
               .to_string( )
           == "test v200, v201" );
    CHECK( X86Inst {
               inst::addq_mi8 { rm( v( 200 ) ), 5 }
    }
               .to_string( )
           == "add v200, 5" );
    CHECK( X86Inst {
               inst::cmpq_mi32 { rm( v( 200 ) ), -7 }
    }
               .to_string( )
           == "cmp v200, -7" );
    CHECK( X86Inst { inst::notq_m { rm( v( 200 ) ) } }.to_string( ) == "not v200" );
    CHECK( X86Inst { inst::negq_m { rm( v( 200 ) ) } }.to_string( ) == "neg v200" );
    CHECK( X86Inst {
               inst::shlq_mc { rm( v( 200 ) ), p( RCX ) }
    }
               .to_string( )
           == "shl v200" );
    CHECK( X86Inst {
               inst::shrq_mc { rm( v( 200 ) ), p( RCX ) }
    }
               .to_string( )
           == "shr v200" );
    CHECK( X86Inst {
               inst::sarq_mi8 { rm( v( 200 ) ), 3 }
    }
               .to_string( )
           == "sar v200, 3" );
}

TEST_CASE( "x86 to_string for tests moves and conditions", "[x86][inst][fmt]" )
{
    CHECK( X86Inst {
               inst::testq_mr { rm( v( 200 ) ), v( 200 ) }
    }
               .to_string( )
           == "test v200, v200" );
    CHECK( X86Inst {
               inst::testl_mr { rm( p( RAX ) ), p( RAX ) }
    }
               .to_string( )
           == "test eax, eax" );
    CHECK( X86Inst {
               inst::testw_mr { rm( p( RSI ) ), p( RSI ) }
    }
               .to_string( )
           == "test si, si" );
    CHECK( X86Inst {
               inst::testb_mr { rm( p( RSI ) ), p( RSI ) }
    }
               .to_string( )
           == "test sil, sil" );
    CHECK( X86Inst {
               inst::cmovq_rm { v( 200 ), rm( v( 201 ) ), CC::E }
    }
               .to_string( )
           == "cmove v200, v201" );
    CHECK( X86Inst {
               inst::cmovq_rm { v( 200 ), rm( v( 201 ) ), CC::AE }
    }
               .to_string( )
           == "cmovae v200, v201" );
    CHECK( X86Inst {
               inst::movq_mr { rm( v( 200 ) ), p( RDX ) }
    }
               .to_string( )
           == "mov v200, rdx" );
    CHECK( X86Inst {
               inst::MovImm { v( 200 ), -1 }
    }
               .to_string( )
           == "mov v200, -1" );
    CHECK( X86Inst {
               inst::MovImm { v( 200 ), 1LL << 40 }
    }
               .to_string( )
           == "mov v200, 1099511627776" );
    CHECK( X86Inst {
               inst::setcc_m8 { rm( v( 200 ) ), CC::LE }
    }
               .to_string( )
           == "setle v200" );
    CHECK( X86Inst {
               inst::movzxq_rm8 { v( 200 ), rm( v( 201 ) ) }
    }
               .to_string( )
           == "movzx v200, v201" );
    CHECK( X86Inst {
               inst::movsxdq_rm32 { v( 200 ), rm( v( 201 ) ) }
    }
               .to_string( )
           == "movsxd v200, v201" );
    CHECK( X86Inst {
               inst::movzxq_rm8 { p( RAX ), rm( p( RSI ) ) }
    }
               .to_string( )
           == "movzx rax, sil" );
}

TEST_CASE( "x86 to_string for every condition code", "[x86][inst][fmt]" )
{
    const std::pair< CC, const char* > ccs[] = {
        { CC::E,  "e"},
        {CC::NE, "ne"},
        { CC::L,  "l"},
        {CC::LE, "le"},
        { CC::G,  "g"},
        {CC::GE, "ge"},
        { CC::B,  "b"},
        {CC::BE, "be"},
        { CC::A,  "a"},
        {CC::AE, "ae"}
    };
    for ( const auto& [ cc, name ] : ccs )
    {
        CHECK( x86_64::to_string( cc ) == name );
        CHECK( X86Inst {
                   inst::Jcc { cc, { 2 } }
        }
                   .to_string( )
               == std::format( "j{} .L2; jmp .L0", name ) );
        CHECK( X86Inst {
                   inst::setcc_m8 { rm( v( 200 ) ), cc }
        }
                   .to_string( )
               == std::format( "set{} v200", name ) );
        CHECK( X86Inst {
                   inst::cmovq_rm { v( 200 ), rm( v( 201 ) ), cc }
        }
                   .to_string( )
               == std::format( "cmov{} v200, v201", name ) );
    }
}

TEST_CASE( "x86 to_string for memory addresses", "[x86][inst][fmt]" )
{
    CHECK( X86Inst {
               inst::movq_rm { v( 200 ), mem( { .base = v( 201 ) } ) }
    }
               .to_string( )
           == "mov v200, qword ptr [v201]" );
    CHECK( X86Inst {
               inst::movl_rm { v( 200 ), mem( { .base = v( 201 ), .disp = 8 } ) }
    }
               .to_string( )
           == "mov v200, dword ptr [v201+0x8]" );
    CHECK( X86Inst {
               inst::movzxq_rm8 { v( 200 ), mem( { .base = v( 201 ), .disp = -16 } ) }
    }
               .to_string( )
           == "movzx v200, byte ptr [v201-0x10]" );
    CHECK( X86Inst {
               inst::movzxq_rm16 { v( 200 ), mem( { .base = v( 201 ), .index = v( 202 ) } ) }
    }
               .to_string( )
           == "movzx v200, word ptr [v201+v202]" );
    CHECK( X86Inst {
               inst::movq_rm { v( 200 ), mem( { .base = p( RSP ), .index = v( 202 ), .scale = 2, .disp = 4 } ) }
    }
               .to_string( )
           == "mov v200, qword ptr [rsp+v202*4+0x4]" );
    CHECK( X86Inst {
               inst::movq_mr { mem( { .base = v( 201 ) } ), v( 200 ) }
    }
               .to_string( )
           == "mov qword ptr [v201], v200" );
    CHECK( X86Inst {
               inst::movb_mr { mem( { .base = v( 201 ), .disp = 24 } ), v( 200 ) }
    }
               .to_string( )
           == "mov byte ptr [v201+0x18], v200" );
    CHECK( X86Inst {
               inst::movw_mr { mem( { .index = v( 202 ) } ), p( RCX ) }
    }
               .to_string( )
           == "mov word ptr [v202], cx" );
    CHECK( X86Inst {
               inst::movq_mr { mem( { } ), v( 200 ) }
    }
               .to_string( )
           == "mov qword ptr [], v200" );
}

TEST_CASE( "x86 to_string for division stack and frame", "[x86][inst][fmt]" )
{
    CHECK( X86Inst {
               inst::cqo { p( RAX ), p( RDX ) }
    }
               .to_string( )
           == "cqo" );
    CHECK( X86Inst {
               inst::divq_m { rm( v( 200 ) ), p( RAX ), p( RDX ) }
    }
               .to_string( )
           == "div v200" );
    CHECK( X86Inst {
               inst::idivq_m { rm( v( 200 ) ), p( RAX ), p( RDX ) }
    }
               .to_string( )
           == "idiv v200" );
    CHECK( X86Inst {
               inst::leaq_rm { v( 200 ), stack_slot( 2, 8 ) }
    }
               .to_string( )
           == "lea v200, [ss2+0x8]" );
    CHECK( X86Inst {
               inst::leaq_rm { v( 200 ), stack_slot( 0, 0 ) }
    }
               .to_string( )
           == "lea v200, [ss0]" );
    CHECK( X86Inst { inst::DummyUse { v( 200 ) } }.to_string( ) == "# keep-alive v200" );
}

TEST_CASE( "x86 to_string for control flow and calls", "[x86][inst][fmt]" )
{
    CHECK( X86Inst { inst::Jmp { { 3 } } }.to_string( ) == "jmp .L3" );
    CHECK( X86Inst {
               inst::Jcc { CC::L, { 0 }, { 1 } }
    }
               .to_string( )
           == "jl .L0; jmp .L1" );
    CHECK( X86Inst { inst::ret { } }.to_string( ) == "ret" );
    CHECK( X86Inst { inst::Call { .name = { 0, 5 } } }.to_string( ) == "call u0:5" );
    CHECK( X86Inst { inst::CallInd { .target = v( 200 ) } }.to_string( ) == "call *v200" );
    CHECK( X86Inst { inst::CallInd { .target = p( R11 ) } }.to_string( ) == "call *r11" );
    CHECK( X86Inst {
               inst::LoadExtName { .dst = v( 200 ), .name = { 0, 3 } }
    }
               .to_string( )
           == "mov v200, u0:3" );
    CHECK( X86Inst {
               inst::LoadExtName { .dst = v( 200 ), .name = { 0, 3 }, .offset = -8 }
    }
               .to_string( )
           == "mov v200, u0:3-8" );
    CHECK( X86Inst {
               inst::LoadExtName { .dst = v( 200 ), .name = { 1, 2 }, .distance = RelocDistance::Near }
    }
               .to_string( )
           == "lea v200, [rip+u1:2]" );
    CHECK( X86Inst {
               inst::LoadExtName { .dst = p( RAX ), .name = { 1, 2 }, .offset = 16, .distance = RelocDistance::Near }
    }
               .to_string( )
           == "lea rax, [rip+u1:2+16]" );
}

TEST_CASE( "x86 every kind formats to a non empty string", "[x86][inst][fmt]" )
{
    for ( const auto& i : every_kind( ) ) CHECK_FALSE( i.to_string( ).empty( ) );
}

TEST_CASE( "x86 apply_alloc rewrites registers in operand order", "[x86][inst][alloc]" )
{
    const auto check = []( X86Inst i, const std::vector< PReg >& pregs, const std::string& expected )
    {
        i.apply_alloc( pregs );
        CHECK( i.to_string( ) == expected );
    };

    check( inst::addq_mr { rm( v( 200 ) ), v( 201 ) }, { RAX, RCX }, "add rax, rcx" );
    check( inst::subq_mi8 { rm( v( 200 ) ), 4 }, { RDX }, "sub rdx, 4" );
    check( inst::notq_m { rm( v( 200 ) ) }, { R8 }, "not r8" );
    check( inst::shlq_mc { rm( v( 200 ) ), p( RCX ) }, { R9, RCX }, "shl r9" );
    check( inst::shrq_mi8 { rm( v( 200 ) ), 2 }, { R10 }, "shr r10, 2" );
    check( inst::testq_mr { rm( v( 200 ) ), v( 200 ) }, { RBX, RBX }, "test rbx, rbx" );
    check( inst::cmovq_rm { v( 200 ), rm( v( 201 ) ), CC::E }, { RSI, RDI }, "cmove rsi, rdi" );
    check( inst::movq_mr { rm( v( 200 ) ), v( 201 ) }, { R12, R13 }, "mov r12, r13" );
    check( inst::MovImm { v( 200 ), 9 }, { R14 }, "mov r14, 9" );
    check( inst::movq_rm { v( 200 ), mem( { .base = v( 201 ), .disp = 8 } ) }, { RAX, RBX }, "mov rax, qword ptr [rbx+0x8]" );
    check( inst::movq_rm { v( 200 ), mem( { .base = v( 201 ), .index = v( 202 ) } ) }, { RAX, RBX, RCX }, "mov rax, qword ptr [rbx+rcx]" );
    check( inst::movq_mr { mem( { .base = v( 201 ) } ), v( 200 ) }, { RBX, RAX }, "mov qword ptr [rbx], rax" );
    check( inst::movq_mr { mem( { .base = v( 201 ), .index = v( 202 ) } ), v( 200 ) }, { RBX, RCX, RAX }, "mov qword ptr [rbx+rcx], rax" );
    check( inst::movzxq_rm8 { v( 200 ), rm( v( 201 ) ) }, { RAX, RCX }, "movzx rax, cl" );
    check( inst::movsxq_rm16 { v( 200 ), rm( v( 201 ) ) }, { RAX, RCX }, "movsx rax, cx" );
    check( inst::divq_m { rm( v( 200 ) ), p( RAX ), p( RDX ) }, { R11, RAX, RDX }, "div r11" );
    check( inst::idivq_m { rm( v( 200 ) ), p( RAX ), p( RDX ) }, { R15, RAX, RDX }, "idiv r15" );
    check( inst::leaq_rm { v( 200 ), stack_slot( 3, 16 ) }, { RDI }, "lea rdi, [ss3+0x10]" );
    check( inst::setcc_m8 { rm( v( 200 ) ), CC::NE }, { RCX }, "setne cl" );
    check( inst::DummyUse { v( 200 ) }, { RAX }, "# keep-alive rax" );
}

TEST_CASE( "x86 apply_alloc leaves cqo on its fixed registers", "[x86][inst][alloc]" )
{
    X86Inst i {
        inst::cqo { p( RAX ), p( RDX ) }
    };
    i.apply_alloc( std::vector< PReg > { RAX, RDX } );
    const auto& c = i.inner.get< inst::cqo >( );
    CHECK( c.rax == p( RAX ) );
    CHECK( c.rdx == p( RDX ) );
}

TEST_CASE( "x86 apply_alloc skips invalid registers", "[x86][inst][alloc]" )
{
    X86Inst i {
        inst::addq_mr { rm( Reg::invalid( ) ), v( 201 ) }
    };
    i.apply_alloc( std::vector< PReg > { R9 } );
    const auto& a = i.inner.get< inst::addq_mr >( );
    CHECK_FALSE( std::get< Reg >( a.dst.value ).valid( ) );
    CHECK( a.src == p( R9 ) );
}

TEST_CASE( "x86 apply_alloc ignores instructions without registers", "[x86][inst][alloc]" )
{
    X86Inst j { inst::Jmp { { 7 } } };
    j.apply_alloc( { } );
    CHECK( j.to_string( ) == "jmp .L7" );
    X86Inst r { inst::ret { } };
    r.apply_alloc( { } );
    CHECK( r.to_string( ) == "ret" );
}

TEST_CASE( "x86 inst converts from every variant member", "[x86][inst]" )
{
    const X86Inst a = inst::movq_mr { rm( v( 200 ) ), v( 201 ) };
    CHECK( a.inner.is< inst::movq_mr >( ) );
    const X86Inst b = inst::ret { };
    CHECK( b.inner.is< inst::ret >( ) );
    const auto kinds = every_kind( );
    for ( std::size_t i { 0 }; i < kinds.size( ); ++i ) CHECK( static_cast< std::size_t >( kinds[ i ].inner.kind( ) ) == i );
}

TEST_CASE( "x86 call payload is boxed and deep copied", "[x86][inst]" )
{
    const inst::Call a {
        .name = { 0, 1 },
        .info = xiao::support::Box { inst::CallInfo { .args = { RCX }, .ret = RAX, .clobbers = { RAX } } }
    };
    inst::Call b = a;
    CHECK( &*b.info != &*a.info );
    CHECK( b.info->args == a.info->args );
    b.info->args.push_back( RDX );
    CHECK( a.info->args.size( ) == 1 );

    const X86Inst x { a };
    const X86Inst y = x;
    CHECK( &*y.inner.get< inst::Call >( ).info != &*x.inner.get< inst::Call >( ).info );
    CHECK( y.clobbers( ).size( ) == 1 );

    X86Inst z {
        inst::CallInd { .target = v( 200 ), .info = xiao::support::Box { inst::CallInfo { .clobbers = { RDX, R8 } } } }
    };
    z = y;
    CHECK( z.inner.is< inst::Call >( ) );
    CHECK( z.clobbers( ).size( ) == 1 );

    STATIC_CHECK( sizeof( inst::Call ) <= 16 );
    STATIC_CHECK( sizeof( inst::CallInd ) <= 16 );
    STATIC_CHECK( sizeof( inst::MInst ) <= 48 );
}
