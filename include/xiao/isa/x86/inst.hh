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

    [[nodiscard]] constexpr bool writes_dst( const AluOp op ) noexcept
    { return op != AluOp::Cmp && op != AluOp::Test; }

    namespace inst
    {
        struct Alu
        {
            AluOp op { };
            Reg   dst { };
            Reg   src { };

            void visit( this auto& self, auto& v )
            {
                writes_dst( self.op ) ? v.usedef( self.dst ) : v.use( self.dst );
                v.use( self.src );
            }
        };

        struct AluImm
        {
            AluOp   op { };
            Reg     dst { };
            int32_t imm { };

            void visit( this auto& self, auto& v )
            { writes_dst( self.op ) ? v.usedef( self.dst ) : v.use( self.dst ); }
        };

        struct Unary
        {
            UnaryOp op { };
            Reg     dst { };

            void visit( this auto& self, auto& v )
            { v.usedef( self.dst ); }
        };

        struct Shift
        {
            ShiftOp op { };
            Reg     dst { };
            Reg     count { };

            void visit( this auto& self, auto& v )
            {
                v.usedef( self.dst );
                v.use( self.count );
            }
        };

        struct ShiftImm
        {
            ShiftOp op { };
            Reg     dst { };
            uint8_t imm { };

            void visit( this auto& self, auto& v )
            { v.usedef( self.dst ); }
        };

        struct TestReg
        {
            Reg         reg { };
            OperandSize size { OperandSize::S64 };

            void visit( this auto& self, auto& v )
            { v.use( self.reg ); }
        };

        struct CMov
        {
            CC  cc { };
            Reg dst { };
            Reg src { };

            void visit( this auto& self, auto& v )
            {
                v.usedef( self.dst );
                v.use( self.src );
            }
        };

        struct Mov
        {
            Reg dst { };
            Reg src { };

            void visit( this auto& self, auto& v )
            {
                v.def( self.dst );
                v.use( self.src );
            }
        };

        struct MovImm
        {
            Reg     dst { };
            int64_t imm { };

            void visit( this auto& self, auto& v )
            { v.def( self.dst ); }
        };

        struct MovLoad
        {
            Reg         dst { };
            AddressMode mem { };
            OperandSize size { OperandSize::S64 };

            void visit( this auto& self, auto& v )
            {
                v.def( self.dst );
                v.use( self.mem.base );
                v.use( self.mem.index );
            }
        };

        struct MovStore
        {
            AddressMode mem { };
            Reg         src { };
            OperandSize size { OperandSize::S64 };

            void visit( this auto& self, auto& v )
            {
                v.use( self.mem.base );
                v.use( self.mem.index );
                v.use( self.src );
            }
        };

        struct Movzx
        {
            Reg         dst { };
            Reg         src { };
            OperandSize size { };

            void visit( this auto& self, auto& v )
            {
                v.def( self.dst );
                v.use( self.src );
            }
        };

        struct Movsx
        {
            Reg         dst { };
            Reg         src { };
            OperandSize size { };

            void visit( this auto& self, auto& v )
            {
                v.def( self.dst );
                v.use( self.src );
            }
        };

        struct Cdq
        {
            Reg rax { };
            Reg rdx { };

            void visit( this auto& self, auto& v )
            {
                v.usedef( self.rax, RAX );
                v.def( self.rdx, RDX );
            }
        };

        struct Div
        {
            Reg src { };
            Reg rax { };
            Reg rdx { };

            void visit( this auto& self, auto& v )
            {
                v.use( self.src );
                v.usedef( self.rax, RAX );
                v.usedef( self.rdx, RDX );
            }
        };

        struct Idiv
        {
            Reg src { };
            Reg rax { };
            Reg rdx { };

            void visit( this auto& self, auto& v )
            {
                v.use( self.src );
                v.usedef( self.rax, RAX );
                v.usedef( self.rdx, RDX );
            }
        };

        struct Push
        {
            Reg src { };

            void visit( this auto& self, auto& v )
            { v.use( self.src ); }
        };

        struct Pop
        {
            Reg dst { };

            void visit( this auto& self, auto& v )
            { v.def( self.dst ); }
        };

        struct StackAddr
        {
            Reg      dst { };
            uint32_t slot { };
            int32_t  offset { };

            void visit( this auto& self, auto& v )
            { v.def( self.dst ); }
        };

        struct SetCC
        {
            CC  cc { };
            Reg dst { };

            void visit( this auto& self, auto& v )
            { v.def( self.dst ); }
        };

        struct Jmp
        {
            BlockId target { };

            void visit( this auto&, auto& ) { }
        };

        struct Jcc
        {
            CC      cc { };
            BlockId taken { };
            BlockId not_taken { };

            void visit( this auto&, auto& ) { }
        };

        template < typename V >
        void visit_call( const std::span< const PReg > args, const PReg ret, V& v )
        {
            for ( const auto arg : args )
            {
                Reg r { Reg::from_preg( arg ) };
                v.use( r );
            }

            if ( !ret.valid( ) ) return;
            Reg r { Reg::from_preg( ret ) };
            v.def( r );
        }

        struct Call
        {
            ir::ExternalName    name { };
            std::vector< PReg > args { };
            PReg                ret { };
            std::vector< PReg > clobbers { };
            CallConv            cc { };

            void visit( this auto& self, auto& v )
            { visit_call( self.args, self.ret, v ); }
        };

        struct LoadExtName
        {
            Reg              dst { };
            ir::ExternalName name { };
            int64_t          offset { };
            RelocDistance    distance { RelocDistance::Far };

            void visit( this auto& self, auto& v )
            { v.def( self.dst ); }
        };

        struct CallInd
        {
            Reg                 target { };
            std::vector< PReg > args { };
            PReg                ret { };
            std::vector< PReg > clobbers { };
            CallConv            cc { };

            void visit( this auto& self, auto& v )
            {
                v.use( self.target );
                visit_call( self.args, self.ret, v );
            }
        };

        struct Ret
        {
            void visit( this auto&, auto& ) { }
        };

        struct DummyUse
        {
            Reg reg { };

            void visit( this auto& self, auto& v )
            { v.use( self.reg ); }
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

        void visit( this auto& self, auto& v )
        {
            std::visit(
                [ & ]( auto& i )
                {
                    i.visit( v );
                },
                self.inner );
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

        void emit( MachBuffer< LabelUse >& buf, std::span< const int32_t > slot_offsets = { } ) const;

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
