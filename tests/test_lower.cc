#include "support.hh"

#include <xiao/isa/x86/abi.hh>
#include <xiao/isa/x86/lower.hh>
#include <xiao/isa/x86/inst/regs.hh>
#include <xiao/regalloc/regalloc.hh>

#include <memory>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

using namespace xt;
using namespace xiao::x86_64;
using codegen::CallConv;
using codegen::PReg;
using codegen::RegClass;
using codegen::VCode;
using Listing = std::vector< std::string >;

namespace
{
    struct Lowered
    {
        std::unique_ptr< ir::IRModule > irm { };
        ir::IRFunction*                 fn { };
        VCode< X86Inst >                vc { };
        CallConv                        cc { };
    };

    template < typename Body >
    [[nodiscard]] Lowered lower_fn( const Type ret, const std::vector< Type >& params, Body&& body,
                                    const CallConv cc = CallConv::WindowsFastcall, const bool check = true )
    {
        Lowered l;
        l.cc         = cc;
        l.irm        = std::make_unique< ir::IRModule >( );
        const auto r = l.irm->declare( "f", ret, std::span< const Type > { params }, cc );
        l.fn         = l.irm->get( r );
        {
            ir::IRBuilder b { *l.fn };
            b.switch_to_block( b.add_block( std::span< const Type > { params } ) );
            Args args;
            for ( uint16_t i { 0 }; i < params.size( ); ++i ) args.push_back( b.param( i ) );
            body( b, args );
            b.finalise( );
        }
        if ( check )
        {
            const auto errs = verify( *l.fn );
            for ( const auto& e : errs ) UNSCOPED_INFO( e );
            REQUIRE( errs.empty( ) );
        }
        const auto env = env_for( cc );
        IRLowerer  lowerer { env, host_abi( ) };
        l.vc = lowerer.lower( *l.fn );
        return l;
    }

    [[nodiscard]] Listing listing( const VCode< X86Inst >& vc )
    {
        Listing out;
        for ( const auto& i : vc.insts ) out.push_back( i.to_string( ) );
        return out;
    }

    [[nodiscard]] Listing block_listing( const VCode< X86Inst >& vc, const uint32_t b )
    {
        Listing     out;
        const auto& blk = vc.blocks[ b ];
        for ( uint32_t i { 0 }; i < blk.insts_count; ++i ) out.push_back( vc.insts[ blk.insts_offset + i ].to_string( ) );
        return out;
    }

    [[nodiscard]] std::vector< uint32_t > succs_of( const VCode< X86Inst >& vc, const uint32_t b )
    {
        std::vector< uint32_t > out;
        const auto&             blk = vc.blocks[ b ];
        for ( uint32_t s { 0 }; s < blk.succs_count; ++s ) out.push_back( vc.succs[ blk.succs_offset + s ].id );
        return out;
    }

    template < typename T >
    [[nodiscard]] std::vector< const T* > all_of( const VCode< X86Inst >& vc )
    {
        std::vector< const T* > out;
        for ( const auto& i : vc.insts )
            if ( const auto* p = i.inner.get_if< T >( ) ) out.push_back( p );
        return out;
    }

    template < bool Loads, typename... Ts >
    [[nodiscard]] std::vector< AddressMode > mem_of( const VCode< X86Inst >& vc )
    {
        std::vector< AddressMode > out;
        for ( const auto& i : vc.insts )
            i.inner.dispatch(
                [ & ]< typename T >( const T& x )
                {
                    if constexpr ( ( std::is_same_v< T, Ts > || ... ) )
                    {
                        const GprMem* side { };
                        if constexpr ( Loads ) side = &x.src;
                        else
                            side = &x.dst;
                        if ( const auto* m = std::get_if< AddressMode >( &side->value ) ) out.push_back( *m );
                    }
                } );
        return out;
    }

    [[nodiscard]] std::vector< AddressMode > loads_of( const VCode< X86Inst >& vc )
    { return mem_of< true, inst::movq_rm, inst::movl_rm, inst::movzxq_rm8, inst::movzxq_rm16 >( vc ); }

    [[nodiscard]] std::vector< AddressMode > stores_of( const VCode< X86Inst >& vc )
    { return mem_of< false, inst::movq_mr, inst::movl_mr, inst::movw_mr, inst::movb_mr >( vc ); }

    [[nodiscard]] std::size_t tests_of( const VCode< X86Inst >& vc )
    {
        return all_of< inst::testb_mr >( vc ).size( ) + all_of< inst::testw_mr >( vc ).size( ) + all_of< inst::testl_mr >( vc ).size( )
               + all_of< inst::testq_mr >( vc ).size( );
    }

    [[nodiscard]] const char* test_suffix( const VCode< X86Inst >& vc )
    {
        if ( !all_of< inst::testb_mr >( vc ).empty( ) ) return "b";
        if ( !all_of< inst::testw_mr >( vc ).empty( ) ) return "w";
        if ( !all_of< inst::testl_mr >( vc ).empty( ) ) return "l";
        if ( !all_of< inst::testq_mr >( vc ).empty( ) ) return "q";
        return "?";
    }

    void check_regalloc( const Lowered& l )
    {
        const auto env    = env_for( l.cc );
        const auto allocs = regalloc::allocate( l.vc, env );
        const auto cfg    = regalloc::Cfg::build( l.vc );
        CHECK( regalloc::check_allocations( l.vc, cfg, allocs.ops, allocs.edits, allocs.spill_slots ).empty( ) );
        REQUIRE( allocs.ops.size( ) == l.vc.operands.size( ) );
        for ( std::size_t o { 0 }; o < allocs.ops.size( ); ++o ) CHECK( allocs.ops[ o ].is_assigned( ) );
        for ( std::size_t o { 0 }; o < allocs.ops.size( ); ++o )
            if ( l.vc.operands[ o ].is_fixed( ) ) CHECK( allocs.ops[ o ].preg == *l.vc.operands[ o ].fixed_preg( ) );
    }

    [[nodiscard]] Lowered binop( const ir::IROp op, const bool const_lhs, const int64_t k )
    {
        return lower_fn( Type::i64, { Type::i64 },
                         [ & ]( ir::IRBuilder& b, const Args& a )
                         {
                             const auto c = b.iconst( Type::i64, static_cast< uint64_t >( k ) );
                             const auto x = const_lhs ? c : a[ 0 ];
                             const auto y = const_lhs ? a[ 0 ] : c;
                             ValueRef   r { };
                             switch ( op )
                             {
                                 case ir::IROp::iadd: r = b.iadd( x, y ); break;
                                 case ir::IROp::isub: r = b.isub( x, y ); break;
                                 case ir::IROp::imul: r = b.imul( x, y ); break;
                                 case ir::IROp::band: r = b.band( x, y ); break;
                                 case ir::IROp::bor: r = b.bor( x, y ); break;
                                 case ir::IROp::bxor: r = b.bxor( x, y ); break;
                                 case ir::IROp::ishl: r = b.ishl( x, y ); break;
                                 case ir::IROp::ushr: r = b.ushr( x, y ); break;
                                 case ir::IROp::sshr: r = b.sshr( x, y ); break;
                                 default: FAIL( "unsupported op" );
                             }
                             b.ret( r );
                         } );
    }

    [[nodiscard]] Lowered icmp_value( const ir::ICmpCC cc, const int mode )
    {
        return lower_fn( Type::i8, { Type::i64, Type::i64 },
                         [ & ]( ir::IRBuilder& b, const Args& a )
                         {
                             const auto k = b.iconst( Type::i64, 10 );
                             if ( mode == 0 ) b.ret( b.icmp( cc, a[ 0 ], a[ 1 ] ) );
                             else if ( mode == 1 )
                                 b.ret( b.icmp( cc, a[ 0 ], k ) );
                             else
                                 b.ret( b.icmp( cc, k, a[ 0 ] ) );
                         } );
    }

    [[nodiscard]] Lowered icmp_branch( const ir::ICmpCC cc, const int mode )
    {
        return lower_fn( Type::i64, { Type::i64, Type::i64 },
                         [ & ]( ir::IRBuilder& b, const Args& a )
                         {
                             const auto t = b.add_block( { } );
                             const auto e = b.add_block( { } );
                             const auto k = b.iconst( Type::i64, 10 );
                             ValueRef   c { };
                             if ( mode == 0 ) c = b.icmp( cc, a[ 0 ], a[ 1 ] );
                             else if ( mode == 1 )
                                 c = b.icmp( cc, a[ 0 ], k );
                             else
                                 c = b.icmp( cc, k, a[ 0 ] );
                             b.brif( c, t, { }, e, { } );
                             b.switch_to_block( t );
                             b.ret( b.iconst( Type::i64, 1 ) );
                             b.switch_to_block( e );
                             b.ret( b.iconst( Type::i64, 0 ) );
                         } );
    }

    const std::pair< ir::ICmpCC, const char* > cc_names[] = {
        { ir::ICmpCC::eq,  "e"},
        { ir::ICmpCC::ne, "ne"},
        {ir::ICmpCC::slt,  "l"},
        {ir::ICmpCC::sle, "le"},
        {ir::ICmpCC::sgt,  "g"},
        {ir::ICmpCC::sge, "ge"},
        {ir::ICmpCC::ult,  "b"},
        {ir::ICmpCC::ule, "be"},
        {ir::ICmpCC::ugt,  "a"},
        {ir::ICmpCC::uge, "ae"},
    };

    const std::pair< ir::ICmpCC, const char* > swapped_cc_names[] = {
        { ir::ICmpCC::eq,  "e"},
        { ir::ICmpCC::ne, "ne"},
        {ir::ICmpCC::slt,  "g"},
        {ir::ICmpCC::sle, "ge"},
        {ir::ICmpCC::sgt,  "l"},
        {ir::ICmpCC::sge, "le"},
        {ir::ICmpCC::ult,  "a"},
        {ir::ICmpCC::ule, "ae"},
        {ir::ICmpCC::ugt,  "b"},
        {ir::ICmpCC::uge, "be"},
    };
}

TEST_CASE( "lower win64 params come from rcx rdx r8 r9", "[lower]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( a[ 3 ] );
                             } );
    CHECK( listing( l.vc ) == Listing { "mov v128, rcx", "mov v129, rdx", "mov v130, r8", "mov v131, r9", "mov rax, v131", "ret" } );
    check_regalloc( l );
}

TEST_CASE( "lower sysv params come from rdi rsi rdx rcx r8 r9", "[lower]" )
{
    const auto l = lower_fn(
        Type::i64, std::vector< Type >( 6, Type::i64 ),
        []( ir::IRBuilder& b, const Args& a )
        {
            b.ret( a[ 0 ] );
        },
        CallConv::SystemV );
    CHECK( listing( l.vc )
           == Listing { "mov v128, rdi", "mov v129, rsi", "mov v130, rdx", "mov v131, rcx", "mov v132, r8", "mov v133, r9", "mov rax, v128",
                        "ret" } );
    check_regalloc( l );
}

TEST_CASE( "lower void return emits a bare ret", "[lower]" )
{
    const auto l = lower_fn( Type::unit, { },
                             []( ir::IRBuilder& b, const Args& )
                             {
                                 b.ret( );
                             } );
    CHECK( listing( l.vc ) == Listing { "ret" } );
    CHECK( l.vc.blocks.size( ) == 1 );
    CHECK( l.vc.operands.empty( ) );
    check_regalloc( l );
}

TEST_CASE( "lower plain binop copies then operates in place", "[lower]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.iadd( a[ 0 ], a[ 1 ] ) );
                             } );
    CHECK( listing( l.vc ) == Listing { "mov v128, rcx", "mov v129, rdx", "mov v130, v128", "add v130, v129", "mov rax, v130", "ret" } );
    check_regalloc( l );
}

TEST_CASE( "lower folds a constant on the rhs of commutative ops", "[lower][fold]" )
{
    const std::pair< ir::IROp, const char* > ops[] = {
        {ir::IROp::iadd,  "add"},
        {ir::IROp::band,  "and"},
        { ir::IROp::bor,   "or"},
        {ir::IROp::bxor,  "xor"},
        {ir::IROp::isub,  "sub"}
    };
    for ( const auto& [ op, name ] : ops )
    {
        const auto l = binop( op, false, 7 );
        CHECK( listing( l.vc ) == Listing { "mov v128, rcx", "mov v130, v128", std::format( "{} v130, 7", name ), "mov rax, v130", "ret" } );
        CHECK( all_of< inst::MovImm >( l.vc ).empty( ) );
        check_regalloc( l );
    }

    const auto m = binop( ir::IROp::imul, false, 7 );
    CHECK( listing( m.vc ) == Listing { "mov v128, rcx", "imul v130, v128, 7", "mov rax, v130", "ret" } );
    CHECK( all_of< inst::MovImm >( m.vc ).empty( ) );
    check_regalloc( m );
}

TEST_CASE( "lower folds a constant on the lhs of commutative ops", "[lower][fold]" )
{
    const std::pair< ir::IROp, const char* > ops[] = {
        {ir::IROp::iadd,  "add"},
        {ir::IROp::band,  "and"},
        { ir::IROp::bor,   "or"},
        {ir::IROp::bxor,  "xor"}
    };
    for ( const auto& [ op, name ] : ops )
    {
        const auto l = binop( op, true, -3 );
        CHECK( listing( l.vc ) == Listing { "mov v128, rcx", "mov v130, v128", std::format( "{} v130, -3", name ), "mov rax, v130", "ret" } );
        check_regalloc( l );
    }

    const auto m = binop( ir::IROp::imul, true, -3 );
    CHECK( listing( m.vc ) == Listing { "mov v128, rcx", "imul v130, v128, -3", "mov rax, v130", "ret" } );
    check_regalloc( m );
}

TEST_CASE( "lower does not fold a constant on the lhs of isub", "[lower][fold]" )
{
    const auto l = binop( ir::IROp::isub, true, 5 );
    CHECK( listing( l.vc ) == Listing { "mov v128, rcx", "mov v129, 5", "mov v130, v129", "sub v130, v128", "mov rax, v130", "ret" } );
    check_regalloc( l );
}

TEST_CASE( "lower folds only constants that fit in imm32", "[lower][fold]" )
{
    const int64_t folded[] = { 0, 1, -1, 0x7fffffff, std::numeric_limits< int32_t >::min( ) };
    const int64_t unfolded[]
        = { 0x80000000LL, 0x100000000LL, -0x80000001LL, std::numeric_limits< int64_t >::max( ), std::numeric_limits< int64_t >::min( ) };
    for ( const auto k : folded )
    {
        const auto l = binop( ir::IROp::iadd, false, k );
        CHECK( listing( l.vc ) == Listing { "mov v128, rcx", "mov v130, v128", std::format( "add v130, {}", k ), "mov rax, v130", "ret" } );
    }
    for ( const auto k : unfolded )
    {
        const auto l = binop( ir::IROp::iadd, false, k );
        CHECK( listing( l.vc )
               == Listing { "mov v128, rcx", std::format( "mov v129, {}", k ), "mov v130, v128", "add v130, v129", "mov rax, v130", "ret" } );
        check_regalloc( l );
    }
}

TEST_CASE( "lower with two constant operands materialises only the lhs", "[lower][fold]" )
{
    const auto l = lower_fn( Type::i64, { },
                             []( ir::IRBuilder& b, const Args& )
                             {
                                 const auto x = b.iconst( Type::i64, 3 );
                                 const auto y = b.iconst( Type::i64, 4 );
                                 b.ret( b.iadd( x, y ) );
                             } );
    CHECK( listing( l.vc ) == Listing { "mov v128, 3", "mov v130, v128", "add v130, 4", "mov rax, v130", "ret" } );
    check_regalloc( l );
}

TEST_CASE( "lower materialises a constant once when any use needs a register", "[lower][fold]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto c = b.iconst( Type::i64, 5 );
                                 const auto x = b.iadd( a[ 0 ], c );
                                 b.ret( b.isub( c, x ) );
                             } );
    CHECK( listing( l.vc )
           == Listing { "mov v128, rcx", "mov v129, 5", "mov v130, v128", "add v130, 5", "mov v131, v129", "sub v131, v130", "mov rax, v131",
                        "ret" } );
    CHECK( all_of< inst::MovImm >( l.vc ).size( ) == 1 );
    check_regalloc( l );
}

TEST_CASE( "lower unused constants emit nothing", "[lower][fold]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 ( void ) b.iconst( Type::i64, 99 );
                                 ( void ) b.iconst( Type::i64, 0x123456789 );
                                 b.ret( a[ 0 ] );
                             } );
    CHECK( listing( l.vc ) == Listing { "mov v128, rcx", "mov rax, v128", "ret" } );
}

TEST_CASE( "lower constant shift counts are folded and masked", "[lower][shift]" )
{
    const std::tuple< ir::IROp, const char*, int64_t, int > cases[] = {
        {ir::IROp::ishl, "shl",             3,  3},
        {ir::IROp::ishl, "shl",            63, 63},
        {ir::IROp::ishl, "shl",            64,  0},
        {ir::IROp::ishl, "shl",            65,  1},
        {ir::IROp::ushr, "shr", 0x100000003LL,  3},
        {ir::IROp::sshr, "sar",            -1, 63},
        {ir::IROp::sshr, "sar",             1,  1},
        {ir::IROp::ushr, "shr",             0,  0},
    };
    for ( const auto& [ op, name, k, expect ] : cases )
    {
        const auto l = binop( op, false, k );
        CHECK( listing( l.vc )
               == Listing { "mov v128, rcx", "mov v130, v128", std::format( "{} v130, {}", name, expect ), "mov rax, v130", "ret" } );
        check_regalloc( l );
    }
}

TEST_CASE( "lower variable shifts route the count through rcx", "[lower][shift]" )
{
    const std::pair< ir::IROp, const char* > ops[] = {
        {ir::IROp::ishl, "shl"},
        {ir::IROp::ushr, "shr"},
        {ir::IROp::sshr, "sar"}
    };
    for ( const auto& [ op, name ] : ops )
    {
        const auto l = lower_fn( Type::i64, { Type::i64, Type::i64 },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                     switch ( op )
                                     {
                                         case ir::IROp::ishl: b.ret( b.ishl( a[ 0 ], a[ 1 ] ) ); break;
                                         case ir::IROp::ushr: b.ret( b.ushr( a[ 0 ], a[ 1 ] ) ); break;
                                         default: b.ret( b.sshr( a[ 0 ], a[ 1 ] ) ); break;
                                     }
                                 } );
        CHECK( listing( l.vc )
               == Listing { "mov v128, rcx", "mov v129, rdx", "mov v130, v128", "mov rcx, v129", std::format( "{} v130", name ),
                            "mov rax, v130", "ret" } );
        check_regalloc( l );
    }
}

TEST_CASE( "lower constant shifted value is materialised", "[lower][shift]" )
{
    const auto l = binop( ir::IROp::ishl, true, 1 );
    CHECK( listing( l.vc )
           == Listing { "mov v128, rcx", "mov v129, 1", "mov v130, v129", "mov rcx, v128", "shl v130", "mov rax, v130", "ret" } );
    check_regalloc( l );
}

TEST_CASE( "lower signed division and remainder", "[lower][div]" )
{
    const auto d = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.idiv( a[ 0 ], a[ 1 ] ) );
                             } );
    CHECK( listing( d.vc )
           == Listing { "mov v128, rcx", "mov v129, rdx", "mov rax, v128", "cqo", "idiv v129", "mov v130, rax", "mov rax, v130", "ret" } );
    check_regalloc( d );

    const auto r = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.srem( a[ 0 ], a[ 1 ] ) );
                             } );
    CHECK( listing( r.vc )
           == Listing { "mov v128, rcx", "mov v129, rdx", "mov rax, v128", "cqo", "idiv v129", "mov v130, rdx", "mov rax, v130", "ret" } );
    check_regalloc( r );
}

TEST_CASE( "lower unsigned division and remainder zero rdx", "[lower][div]" )
{
    const auto d = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.udiv( a[ 0 ], a[ 1 ] ) );
                             } );
    CHECK( listing( d.vc )
           == Listing { "mov v128, rcx", "mov v129, rdx", "mov rax, v128", "mov rdx, 0", "div v129", "mov v130, rax", "mov rax, v130", "ret" } );
    check_regalloc( d );

    const auto r = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.urem( a[ 0 ], a[ 1 ] ) );
                             } );
    CHECK( listing( r.vc )
           == Listing { "mov v128, rcx", "mov v129, rdx", "mov rax, v128", "mov rdx, 0", "div v129", "mov v130, rdx", "mov rax, v130", "ret" } );
    check_regalloc( r );
}

TEST_CASE( "lower division by a constant never folds the divisor", "[lower][div]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.idiv( a[ 0 ], b.iconst( Type::i64, 7 ) ) );
                             } );
    CHECK( listing( l.vc )
           == Listing { "mov v128, rcx", "mov v129, 7", "mov rax, v128", "cqo", "idiv v129", "mov v130, rax", "mov rax, v130", "ret" } );
    check_regalloc( l );
}

TEST_CASE( "lower division chains survive register allocation", "[lower][div]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto q = b.idiv( a[ 0 ], a[ 1 ] );
                                 const auto r = b.urem( a[ 2 ], q );
                                 const auto s = b.srem( r, a[ 0 ] );
                                 b.ret( b.iadd( b.udiv( s, a[ 1 ] ), a[ 2 ] ) );
                             } );
    CHECK( all_of< inst::idivq_m >( l.vc ).size( ) == 2 );
    CHECK( all_of< inst::divq_m >( l.vc ).size( ) == 2 );
    CHECK( all_of< inst::cqo >( l.vc ).size( ) == 2 );
    check_regalloc( l );
}

TEST_CASE( "lower unary ops", "[lower]" )
{
    const auto n = lower_fn( Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.bnot( a[ 0 ] ) );
                             } );
    CHECK( listing( n.vc ) == Listing { "mov v128, rcx", "mov v129, v128", "not v129", "mov rax, v129", "ret" } );
    const auto g = lower_fn( Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.ineg( a[ 0 ] ) );
                             } );
    CHECK( listing( g.vc ) == Listing { "mov v128, rcx", "mov v129, v128", "neg v129", "mov rax, v129", "ret" } );
    check_regalloc( n );
    check_regalloc( g );
}

TEST_CASE( "lower select uses test and cmovne", "[lower][select]" )
{
    const auto l = lower_fn( Type::i64, { Type::i8, Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.select( a[ 0 ], a[ 1 ], a[ 2 ] ) );
                             } );
    CHECK( listing( l.vc )
           == Listing { "mov v128, rcx", "mov v129, rdx", "mov v130, r8", "mov v131, v130", "test v128, v128", "cmovne v131, v129",
                        "mov rax, v131", "ret" } );
    check_regalloc( l );
}

TEST_CASE( "lower select tests with the condition width", "[lower][select]" )
{
    const std::pair< Type, const char* > widths[] = {
        { Type::i8, "b"},
        {Type::i16, "w"},
        {Type::i32, "l"},
        {Type::i64, "q"}
    };
    for ( const auto& [ ty, suffix ] : widths )
    {
        const auto l     = lower_fn( Type::i64, { ty, Type::i64, Type::i64 },
                                     []( ir::IRBuilder& b, const Args& a )
                                     {
                                     b.ret( b.select( a[ 0 ], a[ 1 ], a[ 2 ] ) );
                                     } );
        REQUIRE( tests_of( l.vc ) == 1 );
        CHECK( std::string { test_suffix( l.vc ) } == suffix );
    }
}

TEST_CASE( "lower extensions pick the source width", "[lower][extend]" )
{
    const std::tuple< Type, bool, const char* > cases[] = {
        { Type::i8, false, "movzx v129, v128"},
        {Type::i16, false, "movzx v129, v128"},
        {Type::i32, false, "mov v129, v128"},
        { Type::i8,  true, "movsx v129, v128"},
        {Type::i16,  true, "movsx v129, v128"},
        {Type::i32,  true, "movsxd v129, v128"},
    };
    for ( const auto& [ from, is_signed, text ] : cases )
    {
        const auto l = lower_fn( Type::i64, { from },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                     b.ret( is_signed ? b.sextend( Type::i64, a[ 0 ] ) : b.uextend( Type::i64, a[ 0 ] ) );
                                 } );
        CHECK( listing( l.vc ) == Listing { "mov v128, rcx", text, "mov rax, v129", "ret" } );
        check_regalloc( l );
    }
}

TEST_CASE( "lower narrow to narrow extension", "[lower][extend]" )
{
    const auto l = lower_fn( Type::i32, { Type::i8 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.sextend( Type::i32, a[ 0 ] ) );
                             } );
    CHECK( listing( l.vc ) == Listing { "mov v128, rcx", "movsx v129, v128", "mov rax, v129", "ret" } );
}

TEST_CASE( "lower ireduce is a plain move", "[lower][extend]" )
{
    for ( const auto to : { Type::i8, Type::i16, Type::i32 } )
    {
        const auto l = lower_fn( to, { Type::i64 },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                     b.ret( b.ireduce( to, a[ 0 ] ) );
                                 } );
        CHECK( listing( l.vc ) == Listing { "mov v128, rcx", "mov v129, v128", "mov rax, v129", "ret" } );
        check_regalloc( l );
    }
}

TEST_CASE( "lower unfused icmp produces setcc and zero extension", "[lower][icmp]" )
{
    for ( const auto& [ cc, name ] : cc_names )
    {
        const auto l = icmp_value( cc, 0 );
        CHECK( listing( l.vc )
               == Listing { "mov v128, rcx", "mov v129, rdx", "cmp v128, v129", std::format( "set{} v131", name ), "movzx v131, v131",
                            "mov rax, v131", "ret" } );
        check_regalloc( l );
    }
}

TEST_CASE( "lower unfused icmp with constant rhs", "[lower][icmp]" )
{
    for ( const auto& [ cc, name ] : cc_names )
    {
        const auto l = icmp_value( cc, 1 );
        CHECK( listing( l.vc )
               == Listing { "mov v128, rcx", "mov v129, rdx", "cmp v128, 10", std::format( "set{} v131", name ), "movzx v131, v131",
                            "mov rax, v131", "ret" } );
    }
}

TEST_CASE( "lower unfused icmp with constant lhs swaps the condition", "[lower][icmp]" )
{
    for ( const auto& [ cc, name ] : swapped_cc_names )
    {
        const auto l = icmp_value( cc, 2 );
        CHECK( listing( l.vc )
               == Listing { "mov v128, rcx", "mov v129, rdx", "cmp v128, 10", std::format( "set{} v131", name ), "movzx v131, v131",
                            "mov rax, v131", "ret" } );
    }
}

TEST_CASE( "lower fused icmp and brif emits cmp and jcc", "[lower][icmp][branch]" )
{
    for ( const auto& [ cc, name ] : cc_names )
    {
        const auto l = icmp_branch( cc, 0 );
        CHECK( block_listing( l.vc, 0 )
               == Listing { "mov v128, rcx", "mov v129, rdx", "cmp v128, v129", std::format( "j{} .L1; jmp .L2", name ) } );
        CHECK( block_listing( l.vc, 1 ) == Listing { "mov v133, 1", "mov rax, v133", "ret" } );
        CHECK( block_listing( l.vc, 2 ) == Listing { "mov v135, 0", "mov rax, v135", "ret" } );
        CHECK( all_of< inst::setcc_m8 >( l.vc ).empty( ) );
        CHECK( tests_of( l.vc ) == 0 );
        CHECK( succs_of( l.vc, 0 ) == std::vector< uint32_t > { 1, 2 } );
        CHECK( succs_of( l.vc, 1 ).empty( ) );
        CHECK( l.vc.blocks.size( ) == 3 );
        check_regalloc( l );
    }
}

TEST_CASE( "lower fused icmp with constant rhs", "[lower][icmp][branch]" )
{
    for ( const auto& [ cc, name ] : cc_names )
    {
        const auto l = icmp_branch( cc, 1 );
        CHECK( block_listing( l.vc, 0 )
               == Listing { "mov v128, rcx", "mov v129, rdx", "cmp v128, 10", std::format( "j{} .L1; jmp .L2", name ) } );
        CHECK( all_of< inst::MovImm >( l.vc ).size( ) == 2 );
    }
}

TEST_CASE( "lower fused icmp with constant lhs swaps the condition", "[lower][icmp][branch]" )
{
    for ( const auto& [ cc, name ] : swapped_cc_names )
    {
        const auto l = icmp_branch( cc, 2 );
        CHECK( block_listing( l.vc, 0 )
               == Listing { "mov v128, rcx", "mov v129, rdx", "cmp v128, 10", std::format( "j{} .L1; jmp .L2", name ) } );
        check_regalloc( l );
    }
}

TEST_CASE( "lower icmp with two uses is not fused", "[lower][icmp][branch]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto t = b.add_block( { } );
                                 const auto e = b.add_block( { } );
                                 const auto c = b.icmp( ir::ICmpCC::slt, a[ 0 ], a[ 1 ] );
                                 b.brif( c, t, { }, e, { } );
                                 b.switch_to_block( t );
                                 b.ret( b.uextend( Type::i64, c ) );
                                 b.switch_to_block( e );
                                 b.ret( a[ 1 ] );
                             } );
    CHECK( block_listing( l.vc, 0 )
           == Listing { "mov v128, rcx", "mov v129, rdx", "cmp v128, v129", "setl v130", "movzx v130, v130", "test v130, v130", "jne .L1; jmp .L2" } );
    check_regalloc( l );
}

TEST_CASE( "lower icmp defined in another block is not fused", "[lower][icmp][branch]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto mid = b.add_block( { } );
                                 const auto t   = b.add_block( { } );
                                 const auto e   = b.add_block( { } );
                                 const auto c   = b.icmp( ir::ICmpCC::eq, a[ 0 ], a[ 1 ] );
                                 b.br( mid );
                                 b.switch_to_block( mid );
                                 b.brif( c, t, { }, e, { } );
                                 b.switch_to_block( t );
                                 b.ret( a[ 0 ] );
                                 b.switch_to_block( e );
                                 b.ret( a[ 1 ] );
                             } );
    CHECK( block_listing( l.vc, 0 )
           == Listing { "mov v128, rcx", "mov v129, rdx", "cmp v128, v129", "sete v130", "movzx v130, v130", "jmp .L1" } );
    CHECK( block_listing( l.vc, 1 ) == Listing { "test v130, v130", "jne .L2; jmp .L3" } );
    check_regalloc( l );
}

TEST_CASE( "lower brif on a plain value tests its width", "[lower][branch]" )
{
    const std::pair< Type, const char* > widths[] = {
        { Type::i8, "b"},
        {Type::i16, "w"},
        {Type::i32, "l"},
        {Type::i64, "q"}
    };
    for ( const auto& [ ty, suffix ] : widths )
    {
        const auto l = lower_fn( Type::i64, { ty },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto t = b.add_block( { } );
                                     const auto e = b.add_block( { } );
                                     b.brif( a[ 0 ], t, { }, e, { } );
                                     b.switch_to_block( t );
                                     b.ret( b.iconst( Type::i64, 1 ) );
                                     b.switch_to_block( e );
                                     b.ret( b.iconst( Type::i64, 2 ) );
                                 } );
        CHECK( block_listing( l.vc, 0 ) == Listing { "mov v128, rcx", "test v128, v128", "jne .L1; jmp .L2" } );
        CHECK( std::string { test_suffix( l.vc ) } == suffix );
        check_regalloc( l );
    }
}

TEST_CASE( "lower br copies block arguments then jumps", "[lower][branch]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto     next   = b.add_block( { Type::i64, Type::i64 } );
                                 const ValueRef args[] = { a[ 1 ], a[ 0 ] };
                                 b.br( next, args );
                                 b.switch_to_block( next );
                                 b.ret( b.isub( b.block_param( next, 0 ), b.block_param( next, 1 ) ) );
                             } );
    CHECK( block_listing( l.vc, 0 ) == Listing { "mov v128, rcx", "mov v129, rdx", "mov v130, v129", "mov v131, v128", "jmp .L1" } );
    CHECK( block_listing( l.vc, 1 ) == Listing { "mov v133, v130", "sub v133, v131", "mov rax, v133", "ret" } );
    CHECK( l.vc.blocks[ 1 ].params_count == 2 );
    CHECK( l.vc.block_params[ l.vc.blocks[ 1 ].params_offset ] == codegen::VReg::make( 130, RegClass::Int ) );
    CHECK( l.vc.block_params[ l.vc.blocks[ 1 ].params_offset + 1 ] == codegen::VReg::make( 131, RegClass::Int ) );
    check_regalloc( l );
}

TEST_CASE( "lower then arguments go through a new edge block", "[lower][branch][edges]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto     t      = b.add_block( { Type::i64 } );
                                 const auto     e      = b.add_block( { } );
                                 const ValueRef args[] = { a[ 0 ] };
                                 b.brif( a[ 0 ], t, args, e, { } );
                                 b.switch_to_block( t );
                                 b.ret( b.block_param( t, 0 ) );
                                 b.switch_to_block( e );
                                 b.ret( b.iconst( Type::i64, 0 ) );
                             } );
    REQUIRE( l.vc.blocks.size( ) == 4 );
    CHECK( block_listing( l.vc, 0 ) == Listing { "mov v128, rcx", "test v128, v128", "jne .L1; jmp .L3" } );
    CHECK( block_listing( l.vc, 1 ) == Listing { "mov v129, v128", "jmp .L2" } );
    CHECK( block_listing( l.vc, 2 ) == Listing { "mov rax, v129", "ret" } );
    CHECK( block_listing( l.vc, 3 ) == Listing { "mov v132, 0", "mov rax, v132", "ret" } );
    CHECK( succs_of( l.vc, 0 ) == std::vector< uint32_t > { 1, 3 } );
    CHECK( succs_of( l.vc, 1 ) == std::vector< uint32_t > { 2 } );
    check_regalloc( l );
}

TEST_CASE( "lower else arguments go through a new edge block", "[lower][branch][edges]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto     t      = b.add_block( { } );
                                 const auto     e      = b.add_block( { Type::i64 } );
                                 const ValueRef args[] = { a[ 1 ] };
                                 b.brif( a[ 0 ], t, { }, e, args );
                                 b.switch_to_block( t );
                                 b.ret( a[ 0 ] );
                                 b.switch_to_block( e );
                                 b.ret( b.block_param( e, 0 ) );
                             } );
    REQUIRE( l.vc.blocks.size( ) == 4 );
    CHECK( block_listing( l.vc, 0 ) == Listing { "mov v128, rcx", "mov v129, rdx", "test v128, v128", "jne .L2; jmp .L1" } );
    CHECK( block_listing( l.vc, 1 ) == Listing { "mov v130, v129", "jmp .L3" } );
    check_regalloc( l );
}

TEST_CASE( "lower then target with several predecessors gets an edge block", "[lower][branch][edges]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto join = b.add_block( { } );
                                 const auto mid  = b.add_block( { } );
                                 b.brif( a[ 0 ], join, { }, mid, { } );
                                 b.switch_to_block( join );
                                 b.ret( a[ 0 ] );
                                 b.switch_to_block( mid );
                                 b.br( join );
                             } );
    REQUIRE( l.vc.blocks.size( ) == 4 );
    CHECK( block_listing( l.vc, 0 ) == Listing { "mov v128, rcx", "test v128, v128", "jne .L1; jmp .L3" } );
    CHECK( block_listing( l.vc, 1 ) == Listing { "jmp .L2" } );
    CHECK( block_listing( l.vc, 3 ) == Listing { "jmp .L2" } );
    CHECK( succs_of( l.vc, 0 ) == std::vector< uint32_t > { 1, 3 } );
    check_regalloc( l );
}

TEST_CASE( "lower swapped block arguments go through temporaries", "[lower][branch][parallel]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto     loop    = b.add_block( { Type::i64, Type::i64 } );
                                 const auto     done    = b.add_block( { Type::i64 } );
                                 const ValueRef entry[] = { a[ 0 ], a[ 1 ] };
                                 b.br( loop, entry );
                                 b.switch_to_block( loop );
                                 const auto     x      = b.block_param( loop, 0 );
                                 const auto     y      = b.block_param( loop, 1 );
                                 const ValueRef out[]  = { y };
                                 const ValueRef back[] = { y, x };
                                 b.brif( x, done, out, loop, back );
                                 b.switch_to_block( done );
                                 b.ret( b.block_param( done, 0 ) );
                             } );
    REQUIRE( l.vc.blocks.size( ) == 5 );
    CHECK( block_listing( l.vc, 0 ) == Listing { "mov v128, rcx", "mov v129, rdx", "mov v130, v128", "mov v131, v129", "jmp .L1" } );
    CHECK( block_listing( l.vc, 1 ) == Listing { "test v130, v130", "jne .L3; jmp .L2" } );
    CHECK( block_listing( l.vc, 2 ) == Listing { "mov v136, v131", "mov v137, v130", "mov v130, v136", "mov v131, v137", "jmp .L1" } );
    CHECK( block_listing( l.vc, 3 ) == Listing { "mov v132, v131", "jmp .L4" } );
    CHECK( block_listing( l.vc, 4 ) == Listing { "mov rax, v132", "ret" } );
    CHECK( l.vc.vclasses.size( ) == 138 );
    CHECK( succs_of( l.vc, 1 ) == std::vector< uint32_t > { 3, 2 } );
    check_regalloc( l );
}

TEST_CASE( "lower chained block arguments also use temporaries", "[lower][branch][parallel]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto     loop    = b.add_block( { Type::i64, Type::i64, Type::i64 } );
                                 const ValueRef entry[] = { a[ 0 ], a[ 1 ], a[ 2 ] };
                                 b.br( loop, entry );
                                 b.switch_to_block( loop );
                                 const auto     x      = b.block_param( loop, 0 );
                                 const auto     y      = b.block_param( loop, 1 );
                                 const auto     z      = b.block_param( loop, 2 );
                                 const auto     exit   = b.add_block( { } );
                                 const ValueRef back[] = { y, z, z };
                                 b.brif( x, exit, { }, loop, back );
                                 b.switch_to_block( exit );
                                 b.ret( y );
                             } );
    CHECK( block_listing( l.vc, 1 ) == Listing { "test v131, v131", "jne .L3; jmp .L2" } );
    CHECK( block_listing( l.vc, 2 ) == Listing { "mov v137, v132", "mov v138, v133", "mov v131, v137", "mov v132, v138", "jmp .L1" } );
    check_regalloc( l );
}

TEST_CASE( "lower identity block arguments emit no copies", "[lower][branch][parallel]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto     loop    = b.add_block( { Type::i64 } );
                                 const auto     exit    = b.add_block( { } );
                                 const ValueRef entry[] = { a[ 0 ] };
                                 b.br( loop, entry );
                                 b.switch_to_block( loop );
                                 const auto     x      = b.block_param( loop, 0 );
                                 const ValueRef same[] = { x };
                                 b.brif( x, exit, { }, loop, same );
                                 b.switch_to_block( exit );
                                 b.ret( x );
                             } );
    CHECK( block_listing( l.vc, 1 ) == Listing { "test v129, v129", "jne .L3; jmp .L2" } );
    CHECK( block_listing( l.vc, 2 ) == Listing { "jmp .L1" } );
    check_regalloc( l );
}

TEST_CASE( "lower a counting loop allocates cleanly", "[lower][branch]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto     head    = b.add_block( { Type::i64, Type::i64 } );
                                 const auto     body    = b.add_block( { } );
                                 const auto     exit    = b.add_block( { } );
                                 const ValueRef entry[] = { b.iconst( Type::i64, 0 ), b.iconst( Type::i64, 0 ) };
                                 b.br( head, entry );
                                 b.switch_to_block( head );
                                 const auto i   = b.block_param( head, 0 );
                                 const auto acc = b.block_param( head, 1 );
                                 b.brif( b.icmp( ir::ICmpCC::slt, i, a[ 0 ] ), body, { }, exit, { } );
                                 b.switch_to_block( body );
                                 const ValueRef next[] = { b.iadd( i, b.iconst( Type::i64, 1 ) ), b.iadd( acc, i ) };
                                 b.br( head, next );
                                 b.switch_to_block( exit );
                                 b.ret( acc );
                             } );
    CHECK( all_of< inst::setcc_m8 >( l.vc ).empty( ) );
    CHECK( all_of< inst::Jcc >( l.vc ).size( ) == 1 );
    CHECK( all_of< inst::Jcc >( l.vc )[ 0 ]->cc == CC::L );
    check_regalloc( l );
}

TEST_CASE( "lower loads and stores carry size and displacement", "[lower][mem]" )
{
    const std::pair< Type, const char* > widths[] = {
        { Type::i8, "b"},
        {Type::i16, "w"},
        {Type::i32, "l"},
        {Type::i64, "q"}
    };
    for ( const auto& [ ty, suffix ] : widths )
    {
        const auto l = lower_fn( Type::unit, { Type::ptr, Type::ptr },
                                 [ & ]( ir::IRBuilder& b, const Args& a )
                                 {
                                     const auto v = b.load( ty, a[ 0 ], 8 );
                                     b.store( v, a[ 1 ], -4 );
                                     b.ret( );
                                 } );
        CHECK( listing( l.vc )
               == Listing { "mov v128, rcx", "mov v129, rdx",
                            std::format( "{} v130, {} ptr [v128+0x8]", *suffix == 'b' || *suffix == 'w' ? "movzx" : "mov", ptr_size( ir::bit_width( ty ) ) ),
                            std::format( "mov {} ptr [v129-0x4], v130", ptr_size( ir::bit_width( ty ) ) ), "ret" } );
        CHECK( loads_of( l.vc ).size( ) == 1 );
        CHECK( stores_of( l.vc ).size( ) == 1 );
        check_regalloc( l );
    }
}

TEST_CASE( "lower memory offsets at the int32 limits", "[lower][mem]" )
{
    const auto l     = lower_fn( Type::i64, { Type::ptr },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                 const auto x = b.load( Type::i64, a[ 0 ], std::numeric_limits< int32_t >::max( ) );
                                 const auto y = b.load( Type::i64, a[ 0 ], std::numeric_limits< int32_t >::min( ) );
                                 b.ret( b.iadd( x, y ) );
                                 } );
    const auto loads = loads_of( l.vc );
    REQUIRE( loads.size( ) == 2 );
    CHECK( loads[ 0 ].disp == std::numeric_limits< int32_t >::max( ) );
    CHECK( loads[ 1 ].disp == std::numeric_limits< int32_t >::min( ) );
    CHECK_FALSE( loads[ 0 ].index.valid( ) );
}

TEST_CASE( "lower colocated call is a direct call", "[lower][call]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto fn
                                     = b.import_function( { 0, 7 }, { Type::i64, Type::i64 }, Type::i64, CallConv::WindowsFastcall, true );
                                 const ValueRef args[] = { a[ 0 ], a[ 1 ] };
                                 b.ret( b.call( fn, args ) );
                             } );
    CHECK(
        listing( l.vc )
        == Listing { "mov v128, rcx", "mov v129, rdx", "mov rcx, v128", "mov rdx, v129", "call u0:7", "mov v130, rax", "mov rax, v130", "ret" } );
    const auto calls = all_of< inst::Call >( l.vc );
    REQUIRE( calls.size( ) == 1 );
    CHECK( calls[ 0 ]->info->args == std::vector< PReg > { RCX, RDX } );
    CHECK( calls[ 0 ]->info->ret == RAX );
    CHECK( calls[ 0 ]->info->clobbers == std::vector< PReg > { RAX, RCX, RDX, R8, R9, R10, R11, XMM0, XMM1, XMM2, XMM3, XMM4, XMM5 } );
    check_regalloc( l );
}

TEST_CASE( "lower imported call loads the address then calls indirectly", "[lower][call]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto fn = b.import_function( { 0, 2 }, { Type::i64 }, Type::i64, CallConv::WindowsFastcall, false );
                                 const ValueRef args[] = { a[ 0 ] };
                                 b.ret( b.call( fn, args ) );
                             } );
    CHECK( listing( l.vc )
           == Listing { "mov v128, rcx", "mov v131, u0:2", "mov rcx, v128", "call *v131", "mov v129, rax", "mov rax, v129", "ret" } );
    const auto ind = all_of< inst::CallInd >( l.vc );
    REQUIRE( ind.size( ) == 1 );
    CHECK( ind[ 0 ]->info->args == std::vector< PReg > { RCX } );
    CHECK( ind[ 0 ]->info->clobbers.size( ) == 13 );
    CHECK( all_of< inst::LoadExtName >( l.vc )[ 0 ]->distance == RelocDistance::Far );
    CHECK( l.vc.vclasses.size( ) == 132 );
    check_regalloc( l );
}

TEST_CASE( "lower void call has no return register", "[lower][call]" )
{
    const auto l = lower_fn( Type::unit, { },
                             []( ir::IRBuilder& b, const Args& )
                             {
                                 const auto fn = b.import_function( { 0, 0 }, { }, Type::unit, CallConv::WindowsFastcall, true );
                                 ( void ) b.call( fn, { } );
                                 b.ret( );
                             } );
    CHECK( listing( l.vc ) == Listing { "call u0:0", "ret" } );
    CHECK_FALSE( all_of< inst::Call >( l.vc )[ 0 ]->info->ret.valid( ) );
    check_regalloc( l );
}

TEST_CASE( "lower call with a different callee convention", "[lower][call]" )
{
    const auto l     = lower_fn( Type::i64, { Type::i64, Type::i64 },
                                 []( ir::IRBuilder& b, const Args& a )
                                 {
                                 const auto fn
                                     = b.import_function( { 0, 3 }, { Type::i64, Type::i64 }, Type::i64, CallConv::SystemV, true );
                                 const ValueRef args[] = { a[ 0 ], a[ 1 ] };
                                 b.ret( b.call( fn, args ) );
                                 } );
    const auto calls = all_of< inst::Call >( l.vc );
    REQUIRE( calls.size( ) == 1 );
    CHECK( calls[ 0 ]->info->args == std::vector< PReg > { RDI, RSI } );
    CHECK( calls[ 0 ]->info->clobbers.size( ) == 9 + 16 );
    CHECK( listing( l.vc )[ 2 ] == "mov rdi, v128" );
    CHECK( listing( l.vc )[ 3 ] == "mov rsi, v129" );
    check_regalloc( l );
}

TEST_CASE( "lower values live across a call survive allocation", "[lower][call]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto fn = b.import_function( { 0, 1 }, { Type::i64 }, Type::i64, CallConv::WindowsFastcall, true );
                                 const ValueRef args[] = { a[ 0 ] };
                                 const auto     r      = b.call( fn, args );
                                 b.ret( b.iadd( b.iadd( r, a[ 1 ] ), b.iadd( a[ 2 ], a[ 3 ] ) ) );
                             } );
    check_regalloc( l );
    const auto env    = env_for( l.cc );
    const auto allocs = regalloc::allocate( l.vc, env );
    const auto cfg    = regalloc::Cfg::build( l.vc );
    CHECK( regalloc::check_allocations( l.vc, cfg, allocs.ops, allocs.edits, allocs.spill_slots ).empty( ) );
}

TEST_CASE( "lower call_indirect calls through the callee value", "[lower][call]" )
{
    const auto l = lower_fn( Type::i64, { Type::ptr, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 const auto     sig    = b.import_signature( { Type::i64 }, Type::i64, CallConv::WindowsFastcall );
                                 const ValueRef args[] = { a[ 1 ] };
                                 b.ret( b.call_indirect( sig, a[ 0 ], args ) );
                             } );
    CHECK( listing( l.vc )
           == Listing { "mov v128, rcx", "mov v129, rdx", "mov rcx, v129", "call *v128", "mov v130, rax", "mov rax, v130", "ret" } );
    CHECK( all_of< inst::LoadExtName >( l.vc ).empty( ) );
    check_regalloc( l );
}

TEST_CASE( "lower func_addr distance follows colocation", "[lower][reloc]" )
{
    for ( const bool colocated : { true, false } )
    {
        const auto l = lower_fn( Type::ptr, { },
                                 [ & ]( ir::IRBuilder& b, const Args& )
                                 {
                                     const auto fn = b.import_function( { 0, 3 }, { }, Type::i64, CallConv::WindowsFastcall, colocated );
                                     b.ret( b.func_addr( Type::ptr, fn ) );
                                 } );
        CHECK( listing( l.vc )[ 0 ] == ( colocated ? "lea v128, [rip+u0:3]" : "mov v128, u0:3" ) );
        check_regalloc( l );
    }
}

TEST_CASE( "lower symbol_value keeps the offset", "[lower][reloc]" )
{
    const auto l     = lower_fn( Type::ptr, { },
                                 []( ir::IRBuilder& b, const Args& )
                                 {
                                 const auto gv_near = b.create_global_value( {
                                     .name      = { 1, 2 },
                                     .offset    = 8,
                                     .colocated = true
                                 } );
                                 const auto gv_far  = b.create_global_value( {
                                     .name      = { 1, 5 },
                                     .offset    = -16,
                                     .colocated = false
                                 } );
                                 const auto x       = b.symbol_value( Type::ptr, gv_near );
                                 const auto y       = b.symbol_value( Type::ptr, gv_far );
                                 b.ret( b.iadd( x, b.isub( y, y ) ) );
                                 } );
    const auto names = all_of< inst::LoadExtName >( l.vc );
    REQUIRE( names.size( ) == 2 );
    CHECK( names[ 0 ]->offset == 8 );
    CHECK( names[ 0 ]->distance == RelocDistance::Near );
    CHECK( names[ 0 ]->name == ir::ExternalName { 1, 2 } );
    CHECK( names[ 1 ]->offset == -16 );
    CHECK( names[ 1 ]->distance == RelocDistance::Far );
    CHECK( listing( l.vc )[ 0 ] == "lea v128, [rip+u1:2+8]" );
    CHECK( listing( l.vc )[ 1 ] == "mov v129, u1:5-16" );
    check_regalloc( l );
}

TEST_CASE( "lower registers a class for every vreg", "[lower][vclasses]" )
{
    const auto l = lower_fn( Type::i64, { Type::i64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& a )
                             {
                                 b.ret( b.imul( b.iadd( a[ 0 ], a[ 1 ] ), a[ 0 ] ) );
                             } );
    REQUIRE( l.vc.vclasses.size( ) == 128 + l.fn->values.size( ) );
    for ( std::size_t v { 128 }; v < l.vc.vclasses.size( ); ++v ) CHECK( l.vc.vclasses[ v ] == RegClass::Int );
}

TEST_CASE( "lower f64 values get float vregs", "[lower][vclasses]" )
{
    const auto l = lower_fn( Type::unit, { Type::f64, Type::i64 },
                             []( ir::IRBuilder& b, const Args& )
                             {
                                 b.ret( );
                             } );
    REQUIRE( l.vc.vclasses.size( ) == 131 );
    CHECK( l.vc.vclasses[ 128 ] == RegClass::Float );
    CHECK( l.vc.vclasses[ 129 ] == RegClass::Int );
    CHECK( listing( l.vc ) == Listing { "mov v128, xmm0", "mov v129, rdx", "ret" } );
}

TEST_CASE( "lower operand ranges cover every operand exactly once", "[lower][vcode]" )
{
    const auto l = icmp_branch( ir::ICmpCC::ult, 0 );
    REQUIRE( l.vc.operand_ranges.size( ) == l.vc.insts.size( ) );
    uint32_t expected { 0 };
    for ( const auto& [ off, cnt ] : l.vc.operand_ranges )
    {
        CHECK( off == expected );
        expected += cnt;
    }
    CHECK( expected == l.vc.operands.size( ) );
}

TEST_CASE( "lower blocks tile the instruction list", "[lower][vcode]" )
{
    const auto          l = lower_fn( Type::i64, { Type::i64 },
                                      []( ir::IRBuilder& b, const Args& a )
                                      {
                                 const auto     t    = b.add_block( { Type::i64 } );
                                 const auto     e    = b.add_block( { Type::i64 } );
                                 const ValueRef ta[] = { a[ 0 ] };
                                 const ValueRef ea[] = { b.iconst( Type::i64, 3 ) };
                                 b.brif( a[ 0 ], t, ta, e, ea );
                                 b.switch_to_block( t );
                                 b.ret( b.block_param( t, 0 ) );
                                 b.switch_to_block( e );
                                 b.ret( b.block_param( e, 0 ) );
                                      } );
    std::vector< bool > seen( l.vc.insts.size( ), false );
    for ( const auto& blk : l.vc.blocks )
    {
        REQUIRE( blk.insts_count > 0 );
        for ( uint32_t i { 0 }; i < blk.insts_count; ++i )
        {
            CHECK_FALSE( seen[ blk.insts_offset + i ] );
            seen[ blk.insts_offset + i ] = true;
        }
        CHECK( l.vc.insts[ blk.insts_offset + blk.insts_count - 1 ].is_term( ) != codegen::MachTerminator::None );
    }
    CHECK( std::ranges::all_of( seen,
                                []( const bool s )
                                {
                                    return s;
                                } ) );
    check_regalloc( l );
}

TEST_CASE( "lower the same lowerer can be reused", "[lower]" )
{
    const auto env = env_for( CallConv::WindowsFastcall );
    IRLowerer  lowerer { env, host_abi( ) };

    ir::IRModule irm;
    const auto   a = irm.declare( "a", Type::i64, { Type::i64 }, CallConv::WindowsFastcall );
    const auto   c = irm.declare( "c", Type::i64, { Type::i64, Type::i64 }, CallConv::WindowsFastcall );
    {
        ir::IRBuilder b { *irm.get( a ) };
        b.switch_to_block( b.add_block( { Type::i64 } ) );
        b.ret( b.param( 0 ) );
        b.finalise( );
    }
    {
        ir::IRBuilder b { *irm.get( c ) };
        b.switch_to_block( b.add_block( { Type::i64, Type::i64 } ) );
        b.ret( b.iadd( b.param( 0 ), b.param( 1 ) ) );
        b.finalise( );
    }
    const auto va = lowerer.lower( *irm.get( a ) );
    const auto vc = lowerer.lower( *irm.get( c ) );
    const auto vb = lowerer.lower( *irm.get( a ) );
    CHECK( listing( va ) == Listing { "mov v128, rcx", "mov rax, v128", "ret" } );
    CHECK( listing( vb ) == listing( va ) );
    CHECK( listing( vc ) == Listing { "mov v128, rcx", "mov v129, rdx", "mov v130, v128", "add v130, v129", "mov rax, v130", "ret" } );
    CHECK( vb.vclasses.size( ) == va.vclasses.size( ) );
}

TEST_CASE( "lower high register pressure still allocates", "[lower][regalloc]" )
{
    const auto l = lower_fn(
        Type::i64, { Type::i64, Type::i64, Type::i64, Type::i64 },
        []( ir::IRBuilder& b, const Args& a )
        {
            std::vector< ValueRef > live;
            for ( int i { 0 }; i < 40; ++i )
                live.push_back( b.imul( b.iadd( a[ i % 4 ], b.iconst( Type::i64, static_cast< uint64_t >( i ) ) ), a[ ( i + 1 ) % 4 ] ) );
            auto acc = live[ 0 ];
            for ( std::size_t i { 1 }; i < live.size( ); ++i ) acc = b.bxor( acc, live[ i ] );
            b.ret( acc );
        } );
    const auto env    = env_for( l.cc );
    const auto allocs = regalloc::allocate( l.vc, env );
    CHECK( allocs.spill_slots > 0 );
    check_regalloc( l );
}
