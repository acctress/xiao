#pragma once

#include <xiao/codegen/machbuffer.hh>
#include <xiao/codegen/call_conv.hh>
#include <xiao/codegen/machinst.hh>
#include <xiao/codegen/operand.hh>
#include <xiao/codegen/vcode.hh>
#include <xiao/codegen/reg.hh>
#include <xiao/isa/x86/fmt.hh>
#include <xiao/ir/types.hh>
#include <xiao/ir/extname.hh>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#define XIAO_OPERANDS( ... )                                   \
    void get_operands( std::vector< MachOperand >& ops ) const \
    { __VA_ARGS__ }

#define XIAO_OPERAND( reg, kind, pos )                                                                                  \
    if ( ( reg ).valid( ) )                                                                                             \
        ops.push_back( ( reg ).is_physical( ) ? MachOperand::fixed( ( reg ).as_vreg( ), kind, pos, ( reg ).as_preg( ) ) \
                                              : MachOperand::any( ( reg ).as_vreg( ), kind, pos ) );
#define USE( reg )    XIAO_OPERAND( reg, OperandKind::Use, OperandPos::Before )
#define DEF( reg )    XIAO_OPERAND( reg, OperandKind::Def, OperandPos::After )
#define USEDEF( reg ) XIAO_OPERAND( reg, OperandKind::UseDef, OperandPos::After )
#define FIXED_USE( reg, preg ) \
    if ( ( reg ).valid( ) )    \
        ops.push_back(         \
            codegen::MachOperand::fixed( ( reg ).as_vreg( ), codegen::OperandKind::Use, codegen::OperandPos::Before, ( preg ) ) );
#define FIXED_USEDEF( reg, preg ) \
    if ( ( reg ).valid( ) )       \
        ops.push_back(            \
            codegen::MachOperand::fixed( ( reg ).as_vreg( ), codegen::OperandKind::UseDef, codegen::OperandPos::After, ( preg ) ) );
#define FIXED_DEF( reg, preg ) \
    if ( ( reg ).valid( ) )    \
        ops.push_back( codegen::MachOperand::fixed( ( reg ).as_vreg( ), codegen::OperandKind::Def, codegen::OperandPos::After, ( preg ) ) );

namespace xiao::x86_64
{
    using namespace xiao::codegen;

    enum class CC : uint8_t
    {
        E,
        NE,
        L,
        LE,
        G,
        GE,
        B,
        BE,
        A,
        AE
    };

    enum class AluOp : uint8_t
    {
        Add,
        Sub,
        Mul,
        And,
        Or,
        Xor,
        Cmp,
        Test
    };

    enum class UnaryOp : uint8_t
    {
        Not,
        Neg
    };

    enum class ShiftOp : uint8_t
    {
        Shl,
        Shr,
        Sar
    };

    enum class RelocDistance : uint8_t
    {
        Near,
        Far,
    };

    enum class OperandSize : uint8_t
    {
        S8,
        S16,
        S32,
        S64
    };

    struct AddressMode
    {
        Reg     base { };
        Reg     index { };
        uint8_t scale { };
        int32_t disp { };
    };

    namespace inst
    {
        struct Alu
        {
            AluOp op { };
            Reg   dst { };
            Reg   src { };

            void get_operands( std::vector< MachOperand >& ops ) const
            {
                const bool writes_dst = op != AluOp::Cmp && op != AluOp::Test;
                if ( dst.valid( ) )
                    ops.push_back( writes_dst ? MachOperand::any( dst.as_vreg( ), OperandKind::UseDef, OperandPos::After )
                                              : MachOperand::any( dst.as_vreg( ), OperandKind::Use, OperandPos::Before ) );
                if ( src.valid( ) ) ops.push_back( MachOperand::any( src.as_vreg( ), OperandKind::Use, OperandPos::Before ) );
            }
        };

        struct AluImm
        {
            AluOp   op { };
            Reg     dst { };
            int32_t imm { };

            void get_operands( std::vector< MachOperand >& ops ) const
            {
                if ( op == AluOp::Cmp || op == AluOp::Test ) { USE( dst ) }
                else
                {
                    USEDEF( dst )
                }
            }
        };

        struct Unary
        {
            UnaryOp op { };
            Reg     dst { };
            XIAO_OPERANDS( USEDEF( dst ) )
        };

        struct Shift
        {
            ShiftOp op { };
            Reg     dst { };
            Reg     count { };
            XIAO_OPERANDS( USEDEF( dst ) USE( count ) )
        };

        struct ShiftImm
        {
            ShiftOp op { };
            Reg     dst { };
            uint8_t imm { };
            XIAO_OPERANDS( USEDEF( dst ) )
        };

        struct TestReg
        {
            Reg         reg { };
            OperandSize size { OperandSize::S64 };
            XIAO_OPERANDS( USE( reg ) )
        };

        struct CMov
        {
            CC  cc { };
            Reg dst { };
            Reg src { };
            XIAO_OPERANDS( USEDEF( dst ) USE( src ) )
        };

        struct Mov
        {
            Reg dst { };
            Reg src { };
            XIAO_OPERANDS( DEF( dst ) USE( src ) )
        };

        struct MovImm
        {
            Reg     dst { };
            int64_t imm { };
            XIAO_OPERANDS( DEF( dst ) )
        };

        struct MovLoad
        {
            Reg         dst { };
            AddressMode mem { };
            OperandSize size { OperandSize::S64 };
            XIAO_OPERANDS( DEF( dst ) USE( mem.base ) USE( mem.index ) )
        };

        struct MovStore
        {
            AddressMode mem { };
            Reg         src { };
            OperandSize size { OperandSize::S64 };
            XIAO_OPERANDS( USE( mem.base ) USE( mem.index ) USE( src ) )
        };

        struct Movzx
        {
            Reg         dst { };
            Reg         src { };
            OperandSize size { };
            XIAO_OPERANDS( DEF( dst ) USE( src ) )
        };

        struct Movsx
        {
            Reg         dst { };
            Reg         src { };
            OperandSize size { };
            XIAO_OPERANDS( DEF( dst ) USE( src ) )
        };

        struct Cdq
        {
            Reg rax { };
            Reg rdx { };
            XIAO_OPERANDS( FIXED_USEDEF( rax, RAX ) FIXED_DEF( rdx, RDX ) )
        };

        struct Div
        {
            Reg src { };
            Reg rax { };
            Reg rdx { };
            XIAO_OPERANDS( USE( src ) FIXED_USEDEF( rax, RAX ) FIXED_USEDEF( rdx, RDX ) )
        };

        struct Idiv
        {
            Reg src { };
            Reg rax { };
            Reg rdx { };
            XIAO_OPERANDS( USE( src ) FIXED_USEDEF( rax, RAX ) FIXED_USEDEF( rdx, RDX ) )
        };

        struct Push
        {
            Reg src { };
            XIAO_OPERANDS( USE( src ) )
        };

        struct Pop
        {
            Reg dst { };
            XIAO_OPERANDS( DEF( dst ) )
        };

        struct StackAddr
        {
            Reg      dst { };
            uint32_t slot { };
            int32_t  offset { };
            XIAO_OPERANDS( DEF( dst ) )
        };

        struct SetCC
        {
            CC  cc { };
            Reg dst { };
            XIAO_OPERANDS( DEF( dst ) )
        };

        struct Jmp
        {
            BlockId target { };
            XIAO_OPERANDS( )
        };

        struct Jcc
        {
            CC      cc { };
            BlockId taken { };
            BlockId not_taken { };
            XIAO_OPERANDS( )
        };

        struct Call
        {
            ir::ExternalName    name { };
            std::vector< PReg > args { };
            PReg                ret { };
            std::vector< PReg > clobbers { };
            CallConv            cc { };

            void get_operands( std::vector< MachOperand >& ops ) const
            {
                for ( const auto arg : args ) { FIXED_USE( Reg::from_preg( arg ), arg ) }
                if ( ret.valid( ) ) { FIXED_DEF( Reg::from_preg( ret ), ret ) }
            }
        };

        struct LoadExtName
        {
            Reg              dst { };
            ir::ExternalName name { };
            int64_t          offset { };
            RelocDistance    distance { RelocDistance::Far };
            XIAO_OPERANDS( DEF( dst ) )
        };

        struct CallInd
        {
            Reg                 target { };
            std::vector< PReg > args { };
            PReg                ret { };
            std::vector< PReg > clobbers { };
            CallConv            cc { };

            void get_operands( std::vector< MachOperand >& ops ) const
            {
                USE( target )
                for ( const auto arg : args ) { FIXED_USE( Reg::from_preg( arg ), arg ) }
                if ( ret.valid( ) ) { FIXED_DEF( Reg::from_preg( ret ), ret ) }
            }
        };

        struct Ret
        {
            XIAO_OPERANDS( )
        };

        struct DummyUse
        {
            Reg reg { };
            XIAO_OPERANDS( USE( reg ) )
        };
    } // namespace inst

    struct LabelUse
    {
        enum class Kind : uint8_t
        {
            JmpRel32
        } kind;

        explicit constexpr LabelUse( const Kind k ) : kind { k } { }

        [[nodiscard]] static int32_t encode( const LabelUse use, const uint32_t from, const uint32_t to )
        {
            switch ( use.kind )
            {
                case Kind::JmpRel32: return static_cast< int32_t >( to ) - static_cast< int32_t >( from + 4 );
            }

            std::unreachable( );
        }
    };

    [[nodiscard]] constexpr std::string_view to_string( const AluOp op ) noexcept
    {
        switch ( op )
        {
            case AluOp::Add: return "add";
            case AluOp::Sub: return "sub";
            case AluOp::Mul: return "imul";
            case AluOp::And: return "and";
            case AluOp::Or: return "or";
            case AluOp::Xor: return "xor";
            case AluOp::Cmp: return "cmp";
            case AluOp::Test: return "test";
        }
        return "<?>";
    }

    [[nodiscard]] constexpr std::string_view to_string( const UnaryOp op ) noexcept
    {
        switch ( op )
        {
            case UnaryOp::Not: return "not";
            case UnaryOp::Neg: return "neg";
        }
        return "<?>";
    }

    [[nodiscard]] constexpr std::string_view to_string( const ShiftOp op ) noexcept
    {
        switch ( op )
        {
            case ShiftOp::Shl: return "shl";
            case ShiftOp::Shr: return "shr";
            case ShiftOp::Sar: return "sar";
        }
        return "<?>";
    }

    [[nodiscard]] constexpr std::string_view to_string( const OperandSize size ) noexcept
    {
        switch ( size )
        {
            case OperandSize::S8: return "b";
            case OperandSize::S16: return "w";
            case OperandSize::S32: return "l";
            case OperandSize::S64: return "q";
        }
        return "?";
    }

    [[nodiscard]] constexpr std::string_view to_string( const CC cc ) noexcept
    {
        switch ( cc )
        {
            case CC::E: return "e";
            case CC::NE: return "ne";
            case CC::L: return "l";
            case CC::LE: return "le";
            case CC::G: return "g";
            case CC::GE: return "ge";
            case CC::B: return "b";
            case CC::BE: return "be";
            case CC::A: return "a";
            case CC::AE: return "ae";
        }
        return "<?>";
    }

    struct X86Inst : MachInst< X86Inst, LabelUse >
    {
        using Inst = std::variant< inst::Alu, inst::AluImm, inst::Unary, inst::Shift, inst::ShiftImm, inst::TestReg, inst::CMov, inst::Mov,
                                   inst::MovImm, inst::MovLoad, inst::MovStore, inst::Movzx, inst::Movsx, inst::Cdq, inst::Div, inst::Idiv,
                                   inst::Push, inst::Pop, inst::StackAddr, inst::SetCC, inst::Jmp, inst::Jcc, inst::Call, inst::LoadExtName,
                                   inst::CallInd, inst::Ret, inst::DummyUse >;

        Inst inner;

        //! @brief Implicit conversion from any variant member
        template < typename I >
        X86Inst( I&& i ) : inner { std::forward< I >( i ) }
        {
        }

        void get_operands( std::vector< MachOperand >& ops ) const
        {
            std::visit(
                [ & ]( const auto& i )
                {
                    i.get_operands( ops );
                },
                inner );
        }

        [[nodiscard]] MachTerminator is_term( ) const noexcept
        {
            if ( std::holds_alternative< inst::Ret >( inner ) ) return MachTerminator::Ret;
            if ( std::holds_alternative< inst::Jmp >( inner ) || std::holds_alternative< inst::Jcc >( inner ) )
                return MachTerminator::Branch;
            return MachTerminator::None;
        }

        [[nodiscard]] bool is_call( ) const noexcept
        { return std::holds_alternative< inst::Call >( inner ) || std::holds_alternative< inst::CallInd >( inner ); }

        [[nodiscard]] std::optional< std::pair< Reg, Reg > > as_move( ) const noexcept
        {
            if ( const auto* mov = std::get_if< inst::Mov >( &inner ) ) return std::pair { mov->dst, mov->src };
            return std::nullopt;
        }

        [[nodiscard]] std::span< const PReg > clobbers( ) const noexcept
        {
            if ( const auto* call = std::get_if< inst::Call >( &inner ) ) return call->clobbers;
            if ( const auto* call = std::get_if< inst::CallInd >( &inner ) ) return call->clobbers;
            return { };
        }

        [[nodiscard]] bool is_mem_access( ) const noexcept
        { return std::holds_alternative< inst::MovLoad >( inner ) || std::holds_alternative< inst::MovStore >( inner ); }

        [[nodiscard]] static X86Inst gen_move( Reg dst, Reg src, ir::Type ty ) = delete;

        [[nodiscard]] static X86Inst gen_dummy_use( Reg reg ) = delete;

        [[nodiscard]] static X86Inst gen_jump( uint32_t target ) = delete;

        [[nodiscard]] static X86Inst gen_nop( std::size_t preferred_size ) = delete;

        [[nodiscard]] static constexpr uint32_t worst_case_size( ) = delete;

        [[nodiscard]] static constexpr uint32_t worst_case_island_growth( ) = delete;

        void emit( MachBuffer< LabelUse >& buf, std::span< const PReg > pregs, std::span< const int32_t > slot_offsets = { } ) const;

        void apply_alloc( const std::span< const PReg > pregs )
        {
            uint32_t i { };
            auto     next = [ & ]
            {
                return Reg::from_preg( pregs[ i++ ] );
            };

            std::visit(
                [ & ]< typename T >( T& inst )
                {
                    if constexpr ( std::is_same_v< T, inst::Alu > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                        if ( inst.src.valid( ) ) inst.src = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::AluImm > || std::is_same_v< T, inst::Unary >
                                        || std::is_same_v< T, inst::ShiftImm > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Shift > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                        if ( inst.count.valid( ) ) inst.count = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::TestReg > )
                    {
                        if ( inst.reg.valid( ) ) inst.reg = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Mov > || std::is_same_v< T, inst::CMov > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                        if ( inst.src.valid( ) ) inst.src = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::MovImm > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::MovLoad > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                        if ( inst.mem.base.valid( ) ) inst.mem.base = next( );
                        if ( inst.mem.index.valid( ) ) inst.mem.index = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::MovStore > )
                    {
                        if ( inst.mem.base.valid( ) ) inst.mem.base = next( );
                        if ( inst.mem.index.valid( ) ) inst.mem.index = next( );
                        if ( inst.src.valid( ) ) inst.src = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Movzx > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                        if ( inst.src.valid( ) ) inst.src = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Movsx > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                        if ( inst.src.valid( ) ) inst.src = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Cdq > )
                    {
                        if ( inst.rax.valid( ) ) inst.rax = next( );
                        if ( inst.rdx.valid( ) ) inst.rdx = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Div > || std::is_same_v< T, inst::Idiv > )
                    {
                        if ( inst.src.valid( ) ) inst.src = next( );
                        if ( inst.rax.valid( ) ) inst.rax = next( );
                        if ( inst.rdx.valid( ) ) inst.rdx = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Push > )
                    {
                        if ( inst.src.valid( ) ) inst.src = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Pop > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::StackAddr > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::SetCC > )
                    {
                        if ( inst.dst.valid( ) ) inst.dst = next( );
                    }
                    else if constexpr ( std::is_same_v< T, inst::DummyUse > )
                    {
                        if ( inst.reg.valid( ) ) inst.reg = next( );
                    }
                },
                inner );
        }

        [[nodiscard]] std::string to_string( ) const
        {
            return std::visit(
                []< typename Ty >( const Ty& i ) -> std::string
                {
                    using T = std::decay_t< Ty >;

                    const auto address = []( const AddressMode& mem ) -> std::string
                    {
                        std::string result = "[";

                        if ( mem.base.valid( ) ) result += std::format( "{}", mem.base );

                        if ( mem.index.valid( ) )
                        {
                            if ( result.size( ) > 1 ) result += "+";

                            result += std::format( "{}", mem.index );

                            if ( mem.scale > 1 ) result += std::format( "*{}", mem.scale );
                        }

                        if ( mem.disp > 0 ) result += std::format( "+{}", mem.disp );
                        else if ( mem.disp < 0 )
                            result += std::format( "{}", mem.disp );

                        result += "]";
                        return result;
                    };

                    if constexpr ( std::is_same_v< T, inst::Alu > )
                    {
                        return std::format( "{} {}, {}", x86_64::to_string( i.op ), i.dst, i.src );
                    }
                    else if constexpr ( std::is_same_v< T, inst::AluImm > )
                    {
                        return std::format( "{} {}, {}", x86_64::to_string( i.op ), i.dst, i.imm );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Unary > )
                    {
                        return std::format( "{} {}", x86_64::to_string( i.op ), i.dst );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Shift > )
                    {
                        return std::format( "{} {}, {}", x86_64::to_string( i.op ), i.dst, i.count );
                    }
                    else if constexpr ( std::is_same_v< T, inst::ShiftImm > )
                    {
                        return std::format( "{} {}, {}", x86_64::to_string( i.op ), i.dst, i.imm );
                    }
                    else if constexpr ( std::is_same_v< T, inst::TestReg > )
                    {
                        return std::format( "test.{} {}, {}", x86_64::to_string( i.size ), i.reg, i.reg );
                    }
                    else if constexpr ( std::is_same_v< T, inst::CMov > )
                    {
                        return std::format( "cmov{} {}, {}", x86_64::to_string( i.cc ), i.dst, i.src );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Mov > ) { return std::format( "mov {}, {}", i.dst, i.src ); }
                    else if constexpr ( std::is_same_v< T, inst::MovImm > ) { return std::format( "mov {}, {}", i.dst, i.imm ); }
                    else if constexpr ( std::is_same_v< T, inst::MovLoad > )
                    {
                        return std::format( "mov.{} {}, {}", x86_64::to_string( i.size ), i.dst, address( i.mem ) );
                    }
                    else if constexpr ( std::is_same_v< T, inst::MovStore > )
                    {
                        return std::format( "mov.{} {}, {}", x86_64::to_string( i.size ), address( i.mem ), i.src );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Movzx > )
                    {
                        return std::format( "movzx.{} {}, {}", x86_64::to_string( i.size ), i.dst, i.src );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Movsx > )
                    {
                        return std::format( "movsx.{} {}, {}", x86_64::to_string( i.size ), i.dst, i.src );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Cdq > ) { return "cdq"; }
                    else if constexpr ( std::is_same_v< T, inst::Div > ) { return std::format( "div {}", i.src ); }
                    else if constexpr ( std::is_same_v< T, inst::Idiv > ) { return std::format( "idiv {}", i.src ); }
                    else if constexpr ( std::is_same_v< T, inst::Push > ) { return std::format( "push {}", i.src ); }
                    else if constexpr ( std::is_same_v< T, inst::Pop > ) { return std::format( "pop {}", i.dst ); }
                    else if constexpr ( std::is_same_v< T, inst::StackAddr > )
                    {
                        return i.offset ? std::format( "lea {}, [ss{}{:+}]", i.dst, i.slot, i.offset )
                                        : std::format( "lea {}, [ss{}]", i.dst, i.slot );
                    }
                    else if constexpr ( std::is_same_v< T, inst::SetCC > )
                    {
                        return std::format( "set{} {}", x86_64::to_string( i.cc ), i.dst );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Jmp > ) { return std::format( "jmp .L{}", i.target.id ); }
                    else if constexpr ( std::is_same_v< T, inst::Jcc > )
                    {
                        return std::format( "j{} .L{}; jmp .L{}", x86_64::to_string( i.cc ), i.taken.id, i.not_taken.id );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Call > ) { return std::format( "call u{}:{}", i.name.ns, i.name.index ); }
                    else if constexpr ( std::is_same_v< T, inst::LoadExtName > )
                    {
                        const auto off = i.offset ? std::format( "{:+}", i.offset ) : std::string { };
                        if ( i.distance == RelocDistance::Near )
                            return std::format( "lea {}, [rip+u{}:{}{}]", i.dst, i.name.ns, i.name.index, off );
                        return std::format( "mov {}, u{}:{}{}", i.dst, i.name.ns, i.name.index, off );
                    }
                    else if constexpr ( std::is_same_v< T, inst::CallInd > ) { return std::format( "call *{}", i.target ); }
                    else if constexpr ( std::is_same_v< T, inst::Ret > ) { return "ret"; }
                    else if constexpr ( std::is_same_v< T, inst::DummyUse > ) { return std::format( "# keep-alive {}", i.reg ); }
                },
                inner );
        }
    };
} // namespace xiao::x86_64
