#pragma once

#include <xiao/codegen/machinst.hh>
#include <xiao/codegen/operand.hh>
#include <xiao/codegen/vcode.hh>
#include <xiao/codegen/reg.hh>
#include <xiao/ir/types.hh>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#define XIAO_OPERANDS( ... )                                            \
    void get_operands( std::vector< codegen::MachOperand >& ops ) const \
    { __VA_ARGS__ }

#define USE( reg )          \
    if ( ( reg ).valid( ) ) \
        ops.push_back( codegen::MachOperand::any( ( reg ).as_vreg( ), codegen::OperandKind::Use, codegen::OperandPos::Before ) );
#define DEF( reg )          \
    if ( ( reg ).valid( ) ) \
        ops.push_back( codegen::MachOperand::any( ( reg ).as_vreg( ), codegen::OperandKind::Def, codegen::OperandPos::After ) );
#define USEDEF( reg )       \
    if ( ( reg ).valid( ) ) \
        ops.push_back( codegen::MachOperand::any( ( reg ).as_vreg( ), codegen::OperandKind::UseDef, codegen::OperandPos::After ) );

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
            AluOp        op { };
            codegen::Reg dst { };
            codegen::Reg src { };
            XIAO_OPERANDS( USEDEF( dst ) USE( src ) )
        };

        struct AluImm
        {
            AluOp        op { };
            codegen::Reg dst { };
            int32_t      imm { };
            XIAO_OPERANDS( USEDEF( dst ) )
        };

        struct Mov
        {
            codegen::Reg dst { };
            codegen::Reg src { };
            XIAO_OPERANDS( DEF( dst ) USE( src ) )
        };

        struct MovImm
        {
            codegen::Reg dst { };
            int64_t      imm { };
            XIAO_OPERANDS( DEF( dst ) )
        };

        struct MovLoad
        {
            codegen::Reg dst { };
            AddressMode  mem { };
            XIAO_OPERANDS( DEF( dst ) USE( mem.base ) USE( mem.index ) )
        };

        struct MovStore
        {
            AddressMode  mem { };
            codegen::Reg src { };
            XIAO_OPERANDS( USE( mem.base ) USE( mem.index ) USE( src ) )
        };

        struct Movzx
        {
            codegen::Reg dst { };
            codegen::Reg src { };
            OperandSize  size { };
            XIAO_OPERANDS( DEF( dst ) USE( src ) )
        };

        struct Movsx
        {
            codegen::Reg dst { };
            codegen::Reg src { };
            OperandSize  size { };
            XIAO_OPERANDS( DEF( dst ) USE( src ) )
        };

        struct Cdq
        {
            XIAO_OPERANDS( )
        };

        struct Div
        {
            codegen::Reg src { };
            XIAO_OPERANDS( USE( src ) )
        };

        struct Idiv
        {
            codegen::Reg src { };
            XIAO_OPERANDS( USE( src ) )
        };

        struct Push
        {
            codegen::Reg src { };
            XIAO_OPERANDS( USE( src ) )
        };

        struct Pop
        {
            codegen::Reg dst { };
            XIAO_OPERANDS( DEF( dst ) )
        };

        struct LeaFrame
        {
            codegen::Reg dst { };
            int32_t      offset { };
            XIAO_OPERANDS( DEF( dst ) )
        };

        struct SetCC
        {
            CC           cc { };
            codegen::Reg dst { };
            XIAO_OPERANDS( DEF( dst ) )
        };

        struct Jmp
        {
            codegen::BlockId target { };
            XIAO_OPERANDS( )
        };

        struct Jcc
        {
            CC               cc { };
            codegen::BlockId target { };
            XIAO_OPERANDS( )
        };

        struct Call
        {
            uint32_t func_id { };
            XIAO_OPERANDS( )
        };

        struct Ret
        {
            XIAO_OPERANDS( )
        };

        struct DummyUse
        {
            codegen::Reg reg { };
            XIAO_OPERANDS( USE( reg ) )
        };
    } // namespace inst

    enum class LabelUse : uint8_t
    {
        JmpRel32,
        PCRel32,
    };

    [[nodiscard]] constexpr std::string_view to_string( const AluOp op ) noexcept
    {
        switch ( op )
        {
            case AluOp::Add : return "add";
            case AluOp::Sub : return "sub";
            case AluOp::Mul : return "imul";
            case AluOp::And : return "and";
            case AluOp::Or : return "or";
            case AluOp::Xor : return "xor";
            case AluOp::Cmp : return "cmp";
            case AluOp::Test : return "test";
        }
        return "<?>";
    }

    [[nodiscard]] constexpr std::string_view to_string( const OperandSize size ) noexcept
    {
        switch ( size )
        {
            case OperandSize::S8 : return "b";
            case OperandSize::S16 : return "w";
            case OperandSize::S32 : return "l";
            case OperandSize::S64 : return "q";
        }
        return "?";
    }

    [[nodiscard]] constexpr std::string_view to_string( const CC cc ) noexcept
    {
        switch ( cc )
        {
            case CC::E : return "e";
            case CC::NE : return "ne";
            case CC::L : return "l";
            case CC::LE : return "le";
            case CC::G : return "g";
            case CC::GE : return "ge";
            case CC::B : return "b";
            case CC::BE : return "be";
            case CC::A : return "a";
            case CC::AE : return "ae";
        }
        return "<?>";
    }

    struct X86Inst : MachInst< X86Inst, LabelUse >
    {
        using Inst = std::variant< inst::Alu, inst::AluImm, inst::Mov, inst::MovImm, inst::MovLoad, inst::MovStore, inst::Movzx,
                                   inst::Movsx, inst::Cdq, inst::Div, inst::Idiv, inst::Push, inst::Pop, inst::LeaFrame, inst::SetCC,
                                   inst::Jmp, inst::Jcc, inst::Call, inst::Ret, inst::DummyUse >;

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
            if ( std::holds_alternative< inst::Jmp >( inner ) ) return MachTerminator::Branch;
            if ( std::holds_alternative< inst::Jcc >( inner ) ) return MachTerminator::CondBranch;
            return MachTerminator::None;
        }

        [[nodiscard]] std::optional< std::pair< Reg, Reg > > as_move( ) const noexcept
        {
            if ( const auto* mov = std::get_if< inst::Mov >( &inner ) ) return std::pair { mov->dst, mov->src };
            return std::nullopt;
        }

        [[nodiscard]] bool is_mem_access( ) const noexcept
        { return std::holds_alternative< inst::MovLoad >( inner ) || std::holds_alternative< inst::MovStore >( inner ); }

        [[nodiscard]] static X86Inst gen_move( codegen::Reg dst, codegen::Reg src, ir::Type ty ) = delete;

        [[nodiscard]] static X86Inst gen_dummy_use( codegen::Reg reg ) = delete;

        [[nodiscard]] static X86Inst gen_jump( uint32_t target ) = delete;

        [[nodiscard]] static X86Inst gen_nop( std::size_t preferred_size ) = delete;

        [[nodiscard]] static constexpr uint32_t worst_case_size( ) = delete;

        [[nodiscard]] static constexpr uint32_t worst_case_island_growth( ) = delete;

        template < typename State, typename Info >
        void emit( std::vector< uint8_t >& buf, const Info& info, State& state ) const = delete;

        template < typename State >
        [[nodiscard]] std::string fmt( State& state ) const = delete;

        [[nodiscard]] std::string to_string( ) const
        {
            return std::visit(
                []< typename Ty >( const Ty& i ) -> std::string
                {
                    using T = std::decay_t< Ty >;

                    if constexpr ( std::is_same_v< T, inst::Alu > )
                    {
                        return std::format( "{} {}, {}", x86_64::to_string( i.op ), i.src, i.dst );
                    }
                    else if constexpr ( std::is_same_v< T, inst::AluImm > )
                    {
                        return std::format( "{} ${}, {}", x86_64::to_string( i.op ), i.imm, i.dst );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Mov > ) { return std::format( "mov {}, {}", i.src, i.dst ); }
                    else if constexpr ( std::is_same_v< T, inst::MovImm > ) { return std::format( "mov ${}, {}", i.imm, i.dst ); }
                    else if constexpr ( std::is_same_v< T, inst::MovLoad > )
                    {
                        return std::format( "mov {}({}), {}", i.mem.disp, i.mem.base, i.dst );
                    }
                    else if constexpr ( std::is_same_v< T, inst::MovStore > )
                    {
                        return std::format( "mov {}, {}({})", i.src, i.mem.disp, i.mem.base );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Movzx > )
                    {
                        return std::format( "movz{}q {}, {}", x86_64::to_string( i.size ), i.src, i.dst );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Movsx > )
                    {
                        return std::format( "movs{}q {}, {}", x86_64::to_string( i.size ), i.src, i.dst );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Cdq > ) { return "cdq"; }
                    else if constexpr ( std::is_same_v< T, inst::Div > ) { return std::format( "div {}", i.src ); }
                    else if constexpr ( std::is_same_v< T, inst::Idiv > ) { return std::format( "idiv {}", i.src ); }
                    else if constexpr ( std::is_same_v< T, inst::Push > ) { return std::format( "push {}", i.src ); }
                    else if constexpr ( std::is_same_v< T, inst::Pop > ) { return std::format( "pop {}", i.dst ); }
                    else if constexpr ( std::is_same_v< T, inst::LeaFrame > ) { return std::format( "lea {}(%rbp), {}", i.offset, i.dst ); }
                    else if constexpr ( std::is_same_v< T, inst::SetCC > )
                    {
                        return std::format( "set{} {}", x86_64::to_string( i.cc ), i.dst );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Jmp > ) { return std::format( "jmp .L{}", i.target.id ); }
                    else if constexpr ( std::is_same_v< T, inst::Jcc > )
                    {
                        return std::format( "j{} .L{}", x86_64::to_string( i.cc ), i.target.id );
                    }
                    else if constexpr ( std::is_same_v< T, inst::Call > ) { return std::format( "call .Lfunc{}", i.func_id ); }
                    else if constexpr ( std::is_same_v< T, inst::Ret > ) { return "ret"; }
                    else if constexpr ( std::is_same_v< T, inst::DummyUse > ) { return std::format( "# keep-alive {}", i.reg ); }
                },
                inner );
        }
    };

} // namespace xiao::x86_64
