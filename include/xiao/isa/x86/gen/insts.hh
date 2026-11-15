#pragma once

#include <xiao/isa/x86/asm.hh>
#include <xiao/isa/x86/pseudo.hh>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace xiao::x86_64::inst
{
    struct addw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x01 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x03 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct addw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addw_mi16
    {
        GprMem  dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addw_i16
    {
        Reg     acc { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x05 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}", imm ); }
    };

    struct orw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x09 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x0B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct orw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orw_mi16
    {
        GprMem  dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orw_i16
    {
        Reg     acc { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x0D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}", imm ); }
    };

    struct adcw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x11 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x13 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct adcw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcw_mi16
    {
        GprMem  dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcw_i16
    {
        Reg     acc { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x15 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}", imm ); }
    };

    struct sbbw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x19 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x1B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct sbbw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbw_mi16
    {
        GprMem  dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbw_i16
    {
        Reg     acc { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x1D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}", imm ); }
    };

    struct andw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x21 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x23 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct andw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andw_mi16
    {
        GprMem  dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andw_i16
    {
        Reg     acc { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x25 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}", imm ); }
    };

    struct subw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x29 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x2B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct subw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subw_mi16
    {
        GprMem  dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subw_i16
    {
        Reg     acc { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x2D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}", imm ); }
    };

    struct xorw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x31 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x33 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct xorw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 6, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorw_mi16
    {
        GprMem  dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 6, dst, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorw_i16
    {
        Reg     acc { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x35 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}", imm ); }
    };

    struct cmpw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x39 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x3B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct cmpw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpw_mi16
    {
        GprMem  dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpw_i16
    {
        Reg     acc { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { v.use( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x3D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}", imm ); }
    };

    struct testw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x85 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct testw_mi16
    {
        GprMem  dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct testw_i16
    {
        Reg     acc { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { v.use( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xA9 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}", imm ); }
    };

    struct addl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x01 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x03 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct addl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addl_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addl_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x05 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}", imm ); }
    };

    struct orl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x09 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x0B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct orl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orl_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orl_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x0D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}", imm ); }
    };

    struct adcl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x11 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x13 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct adcl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcl_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcl_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x15 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}", imm ); }
    };

    struct sbbl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x19 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x1B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct sbbl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbl_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbl_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x1D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}", imm ); }
    };

    struct andl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x21 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x23 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct andl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andl_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andl_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x25 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}", imm ); }
    };

    struct subl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x29 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x2B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct subl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subl_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subl_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x2D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}", imm ); }
    };

    struct xorl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x31 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x33 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct xorl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 6, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorl_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 6, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorl_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x35 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}", imm ); }
    };

    struct cmpl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x39 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x3B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct cmpl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpl_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpl_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.use( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x3D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}", imm ); }
    };

    struct testl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x85 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct testl_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct testl_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.use( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xA9 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}", imm ); }
    };

    struct addq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x01 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x03 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct addq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addq_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addq_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x05 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}", imm ); }
    };

    struct orq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x09 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x0B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct orq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orq_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orq_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x0D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}", imm ); }
    };

    struct adcq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x11 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x13 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct adcq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcq_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcq_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x15 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}", imm ); }
    };

    struct sbbq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x19 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x1B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct sbbq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbq_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbq_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x1D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}", imm ); }
    };

    struct andq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x21 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x23 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct andq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andq_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andq_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x25 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}", imm ); }
    };

    struct subq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x29 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x2B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct subq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subq_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subq_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x2D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}", imm ); }
    };

    struct xorq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x31 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x33 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct xorq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 6, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorq_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 6, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorq_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x35 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}", imm ); }
    };

    struct cmpq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x39 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x3B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct cmpq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x83 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpq_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x81 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpq_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.use( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x3D }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}", imm ); }
    };

    struct testq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x85 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct testq_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct testq_i32
    {
        Reg     acc { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.use( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xA9 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}", imm ); }
    };

    struct addb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x00 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addb_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x02 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr( dst, 8 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct addb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x80 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct addb_i8
    {
        Reg    acc { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x04 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "add {}", imm ); }
    };

    struct orb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x08 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orb_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x0A }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr( dst, 8 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct orb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x80 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct orb_i8
    {
        Reg    acc { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x0C }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "or {}", imm ); }
    };

    struct adcb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x10 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcb_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x12 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr( dst, 8 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct adcb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x80 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct adcb_i8
    {
        Reg    acc { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x14 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "adc {}", imm ); }
    };

    struct sbbb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x18 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbb_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x1A }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr( dst, 8 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct sbbb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x80 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sbbb_i8
    {
        Reg    acc { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x1C }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sbb {}", imm ); }
    };

    struct andb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x20 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andb_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x22 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr( dst, 8 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct andb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x80 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct andb_i8
    {
        Reg    acc { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x24 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "and {}", imm ); }
    };

    struct subb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x28 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subb_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x2A }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr( dst, 8 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct subb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x80 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct subb_i8
    {
        Reg    acc { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x2C }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sub {}", imm ); }
    };

    struct xorb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x30 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorb_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x32 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr( dst, 8 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct xorb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x80 }, .len = 1 };
            emit_rm_digit( buf, op, 6, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xorb_i8
    {
        Reg    acc { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x34 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xor {}", imm ); }
    };

    struct cmpb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x38 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpb_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x3A }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr( dst, 8 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct cmpb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x80 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpb_i8
    {
        Reg    acc { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { v.use( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x3C }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmp {}", imm ); }
    };

    struct testb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x84 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct testb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF6 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct testb_i8
    {
        Reg    acc { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { v.use( self.acc, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xA8 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "test {}", imm ); }
    };

    struct btw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xA3 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bt {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bt {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btsw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xAB },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bts {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btsw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bts {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btrw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xB3 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btr {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btrw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 6, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btr {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btcw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xBB },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btc {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btcw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btc {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct bsfw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xBC },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bsf {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct bsrw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xBD },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bsr {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct btl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xA3 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bt {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bt {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btsl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xAB },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bts {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btsl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bts {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btrl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xB3 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btr {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btrl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 6, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btr {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btcl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xBB },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btc {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btcl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btc {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct bsfl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xBC },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bsf {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct bsrl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xBD },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bsr {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct btq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Use );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xA3 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bt {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bt {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btsq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xAB },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bts {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btsq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bts {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btrq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xB3 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btr {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btrq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 6, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btr {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btcq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xBB },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btc {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct btcq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xBA },
                .len   = 2
            };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "btc {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct bsfq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xBC },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bsf {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct bsrq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xBD },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bsr {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct leaw_rm
    {
        Reg         dst { };
        AddressMode src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            v.use( self.src.base );
            v.use( self.src.index );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x8D }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), GprMem { src }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "lea {}, {}", fmt_gpr( dst, 16 ), fmt_mem( src, 0 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct leal_rm
    {
        Reg         dst { };
        AddressMode src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            v.use( self.src.base );
            v.use( self.src.index );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x8D }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), GprMem { src }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "lea {}, {}", fmt_gpr( dst, 32 ), fmt_mem( src, 0 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct leaq_rm
    {
        Reg         dst { };
        AddressMode src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            v.use( self.src.base );
            v.use( self.src.index );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x8D }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), GprMem { src }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "lea {}, {}", fmt_gpr( dst, 64 ), fmt_mem( src, 0 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct movntil_mr
    {
        AddressMode dst { };
        Reg         src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.dst.base );
            v.use( self.dst.index );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xC3 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), GprMem { dst }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movnti {}, {}", fmt_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct movntiq_mr
    {
        AddressMode dst { };
        Reg         src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.dst.base );
            v.use( self.dst.index );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xC3 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), GprMem { dst }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movnti {}, {}", fmt_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct prefetchnta_m8
    {
        AddressMode src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.src.base );
            v.use( self.src.index );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0x18 },
                .len   = 2
            };
            emit_rm_digit( buf, op, 0, GprMem { src }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "prefetchnta {}", fmt_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct prefetcht0_m8
    {
        AddressMode src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.src.base );
            v.use( self.src.index );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0x18 },
                .len   = 2
            };
            emit_rm_digit( buf, op, 1, GprMem { src }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "prefetcht0 {}", fmt_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct prefetcht1_m8
    {
        AddressMode src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.src.base );
            v.use( self.src.index );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0x18 },
                .len   = 2
            };
            emit_rm_digit( buf, op, 2, GprMem { src }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "prefetcht1 {}", fmt_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct prefetcht2_m8
    {
        AddressMode src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.src.base );
            v.use( self.src.index );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0x18 },
                .len   = 2
            };
            emit_rm_digit( buf, op, 3, GprMem { src }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "prefetcht2 {}", fmt_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct clflush_m
    {
        AddressMode src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.src.base );
            v.use( self.src.index );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xAE },
                .len   = 2
            };
            emit_rm_digit( buf, op, 7, GprMem { src }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "clflush {}", fmt_mem( src, 0 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct ldmxcsr_m32
    {
        AddressMode src { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.src.base );
            v.use( self.src.index );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xAE },
                .len   = 2
            };
            emit_rm_digit( buf, op, 2, GprMem { src }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ldmxcsr {}", fmt_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct stmxcsr_m32
    {
        AddressMode dst { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.dst.base );
            v.use( self.dst.index );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xAE },
                .len   = 2
            };
            emit_rm_digit( buf, op, 3, GprMem { dst }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "stmxcsr {}", fmt_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct cmpxchg8b_m64
    {
        AddressMode dst { };
        Reg         rax { };
        Reg         rdx { };
        Reg         rbx { };
        Reg         rcx { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.dst.base );
            v.use( self.dst.index );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
            v.use( self.rbx, RBX );
            v.use( self.rcx, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xC7 },
                .len   = 2
            };
            emit_rm_digit( buf, op, 1, GprMem { dst }, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmpxchg8b {}", fmt_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return true; }
    };

    struct nop
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x90 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "nop" }; }
    };

    struct ret
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC3 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "ret" }; }
    };

    struct leave
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC9 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "leave" }; }
    };

    struct hlt
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF4 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "hlt" }; }
    };

    struct int3
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xCC }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "int3" }; }
    };

    struct clc
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF8 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "clc" }; }
    };

    struct stc
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF9 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "stc" }; }
    };

    struct cmc
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF5 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "cmc" }; }
    };

    struct cld
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xFC }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "cld" }; }
    };

    struct pause
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rep = 0xF3, .bytes = { 0x90 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "pause" }; }
    };

    struct ud2
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0x0B },
                .len   = 2
            };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "ud2" }; }
    };

    struct syscall
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0x05 },
                .len   = 2
            };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "syscall" }; }
    };

    struct cpuid
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xA2 },
                .len   = 2
            };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "cpuid" }; }
    };

    struct rdtsc
    {
        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0x31 },
                .len   = 2
            };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "rdtsc" }; }
    };

    struct pushq_r
    {
        Reg src { };

        void visit( this auto& self, auto& v )
        { v.use( self.src ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x50 }, .len = 1 };
            emit_opreg( buf, op, src.as_preg( ), false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "push {}", fmt_gpr( src, 64 ) ); }
    };

    struct popq_r
    {
        Reg dst { };

        void visit( this auto& self, auto& v )
        { v.def( self.dst ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x58 }, .len = 1 };
            emit_opreg( buf, op, dst.as_preg( ), false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "pop {}", fmt_gpr( dst, 64 ) ); }
    };

    struct pushq_m
    {
        GprMem src { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.src, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xFF }, .len = 1 };
            emit_rm_digit( buf, op, 6, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "push {}", fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct popq_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Def ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x8F }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "pop {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct pushq_i8
    {
        int8_t imm { };

        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x6A }, .len = 1 };
            emit_op( buf, op );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "push {}", imm ); }
    };

    struct pushq_i32
    {
        int32_t imm { };

        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x68 }, .len = 1 };
            emit_op( buf, op );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "push {}", imm ); }
    };

    struct jmpq_m
    {
        GprMem target { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.target, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xFF }, .len = 1 };
            emit_rm_digit( buf, op, 4, target, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "jmp {}", fmt_gpr_mem( target, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( target.value ); }
    };

    struct callq_m
    {
        GprMem target { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.target, v, OperandKind::Use ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xFF }, .len = 1 };
            emit_rm_digit( buf, op, 2, target, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "call {}", fmt_gpr_mem( target, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( target.value ); }
    };

    struct movw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Def );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x89 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct movw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x8B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movw_mi16
    {
        GprMem  dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Def ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xC7 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xchgw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.usedef( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x87 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xchg {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xaddw_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.usedef( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xC1 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xadd {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpxchgw_mr
    {
        GprMem dst { };
        Reg    src { };
        Reg    acc { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
            v.usedef( self.acc, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xB1 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmpxchg {}, {}", fmt_gpr_mem( dst, 16 ), fmt_gpr( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmovw_rm
    {
        Reg    dst { };
        GprMem src { };
        CC     cond { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0x40 },
                .len   = 2
            };
            op.bytes[ 1 ] |= cc_nibble( cond );
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmov{} {}, {}", x86_64::to_string( cond ), fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movzxw_rm8
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xB6 },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movzx {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movsxw_rm8
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xBE },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movsx {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Def );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x89 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct movl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x8B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movl_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Def ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC7 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xchgl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.usedef( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x87 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xchg {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xaddl_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.usedef( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xC1 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xadd {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpxchgl_mr
    {
        GprMem dst { };
        Reg    src { };
        Reg    acc { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
            v.usedef( self.acc, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xB1 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmpxchg {}, {}", fmt_gpr_mem( dst, 32 ), fmt_gpr( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmovl_rm
    {
        Reg    dst { };
        GprMem src { };
        CC     cond { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            auto op = Opcode {
                .bytes = { 0x0F, 0x40 },
                .len   = 2
            };
            op.bytes[ 1 ] |= cc_nibble( cond );
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmov{} {}, {}", x86_64::to_string( cond ), fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movzxl_rm8
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xB6 },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movzx {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movsxl_rm8
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xBE },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movsx {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Def );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x89 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct movq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x8B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movq_mi32
    {
        GprMem  dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Def ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xC7 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xchgq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.usedef( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x87 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xchg {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xaddq_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.usedef( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xC1 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xadd {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpxchgq_mr
    {
        GprMem dst { };
        Reg    src { };
        Reg    acc { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
            v.usedef( self.acc, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xB1 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmpxchg {}, {}", fmt_gpr_mem( dst, 64 ), fmt_gpr( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmovq_rm
    {
        Reg    dst { };
        GprMem src { };
        CC     cond { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0x40 },
                .len   = 2
            };
            op.bytes[ 1 ] |= cc_nibble( cond );
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmov{} {}, {}", x86_64::to_string( cond ), fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movzxq_rm8
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xB6 },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movzx {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movsxq_rm8
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xBE },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movsx {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movzxl_rm16
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xB7 },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movzx {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movsxl_rm16
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xBF },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movsx {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct bswapl_r
    {
        Reg dst { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.dst ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xC8 },
                .len   = 2
            };
            emit_opreg( buf, op, dst.as_preg( ), false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bswap {}", fmt_gpr( dst, 32 ) ); }
    };

    struct movzxq_rm16
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xB7 },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movzx {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movsxq_rm16
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xBF },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movsx {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct bswapq_r
    {
        Reg dst { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.dst ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xC8 },
                .len   = 2
            };
            emit_opreg( buf, op, dst.as_preg( ), false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "bswap {}", fmt_gpr( dst, 64 ) ); }
    };

    struct movsxdq_rm32
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x63 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movsxd {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movw_ri16
    {
        Reg     dst { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        { v.def( self.dst ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xB8 }, .len = 1 };
            emit_opreg( buf, op, dst.as_preg( ), false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr( dst, 16 ), imm ); }
    };

    struct movl_ri32
    {
        Reg     dst { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        { v.def( self.dst ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xB8 }, .len = 1 };
            emit_opreg( buf, op, dst.as_preg( ), false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr( dst, 32 ), imm ); }
    };

    struct movq_ri64
    {
        Reg     dst { };
        int64_t imm { };

        void visit( this auto& self, auto& v )
        { v.def( self.dst ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xB8 }, .len = 1 };
            emit_opreg( buf, op, dst.as_preg( ), false );
            buf.emit_u64( static_cast< uint64_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "movabs {}, {}", fmt_gpr( dst, 64 ), imm ); }
    };

    struct movb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::Def );
            v.use( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x88 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct movb_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x8A }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr( dst, 8 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct movb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Def ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC6 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct movb_ri8
    {
        Reg    dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { v.def( self.dst ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xB0 }, .len = 1 };
            emit_opreg( buf, op, dst.as_preg( ), true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr( dst, 8 ), imm ); }
    };

    struct xchgb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.usedef( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x86 }, .len = 1 };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xchg {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct xaddb_mr
    {
        GprMem dst { };
        Reg    src { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.usedef( self.src );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xC0 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "xadd {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cmpxchgb_mr
    {
        GprMem dst { };
        Reg    src { };
        Reg    acc { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.src );
            v.usedef( self.acc, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xB0 },
                .len   = 2
            };
            emit_rm( buf, op, src.as_preg( ), dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "cmpxchg {}, {}", fmt_gpr_mem( dst, 8 ), fmt_gpr( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct setcc_m8
    {
        GprMem dst { };
        CC     cond { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::Def ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            auto op = Opcode {
                .bytes = { 0x0F, 0x90 },
                .len   = 2
            };
            op.bytes[ 1 ] |= cc_nibble( cond );
            emit_rm_digit( buf, op, 0, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "set{} {}", x86_64::to_string( cond ), fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct imulw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .bytes = { 0x0F, 0xAF },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct imulw_rmi8
    {
        Reg    dst { };
        GprMem src { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x6B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}, {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct imulw_rmi16
    {
        Reg     dst { };
        GprMem  src { };
        int16_t imm { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x69 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
            buf.emit_u16( static_cast< uint16_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}, {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct mulw_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 4, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mul {}", fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct imulw_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 5, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}", fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct divw_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 6, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "div {}", fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct idivw_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 7, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "idiv {}", fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct notw_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "not {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct negw_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "neg {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct incw_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xFF }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "inc {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct decw_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xFF }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "dec {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct imull_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .bytes = { 0x0F, 0xAF },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct imull_rmi8
    {
        Reg    dst { };
        GprMem src { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x6B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}, {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct imull_rmi32
    {
        Reg     dst { };
        GprMem  src { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x69 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}, {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct mull_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 4, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mul {}", fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct imull_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 5, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}", fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct divl_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 6, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "div {}", fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct idivl_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 7, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "idiv {}", fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct notl_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "not {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct negl_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "neg {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct incl_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xFF }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "inc {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct decl_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xFF }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "dec {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct imulq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rex_w = true,
                .bytes = { 0x0F, 0xAF },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct imulq_rmi8
    {
        Reg    dst { };
        GprMem src { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x6B }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}, {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct imulq_rmi32
    {
        Reg     dst { };
        GprMem  src { };
        int32_t imm { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x69 }, .len = 1 };
            emit_rm( buf, op, dst.as_preg( ), src, false );
            buf.emit_i32( imm );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}, {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct mulq_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 4, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mul {}", fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct imulq_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 5, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}", fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct divq_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 6, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "div {}", fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct idivq_m
    {
        GprMem src { };
        Reg    rax { };
        Reg    rdx { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
            v.usedef( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 7, src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "idiv {}", fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct notq_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "not {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct negq_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xF7 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "neg {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct incq_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xFF }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "inc {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct decq_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xFF }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "dec {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct mulb_m
    {
        GprMem src { };
        Reg    rax { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF6 }, .len = 1 };
            emit_rm_digit( buf, op, 4, src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mul {}", fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct imulb_m
    {
        GprMem src { };
        Reg    rax { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF6 }, .len = 1 };
            emit_rm_digit( buf, op, 5, src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "imul {}", fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct divb_m
    {
        GprMem src { };
        Reg    rax { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF6 }, .len = 1 };
            emit_rm_digit( buf, op, 6, src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "div {}", fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct idivb_m
    {
        GprMem src { };
        Reg    rax { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.src, v, OperandKind::Use );
            v.usedef( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF6 }, .len = 1 };
            emit_rm_digit( buf, op, 7, src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "idiv {}", fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct notb_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF6 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "not {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct negb_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xF6 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "neg {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct incb_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xFE }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "inc {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct decb_m
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xFE }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "dec {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct cbw
    {
        Reg rax { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.rax, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x98 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "cbw" }; }
    };

    struct cwde
    {
        Reg rax { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.rax, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x98 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "cwde" }; }
    };

    struct cdqe
    {
        Reg rax { };

        void visit( this auto& self, auto& v )
        { v.usedef( self.rax, RAX ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x98 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "cdqe" }; }
    };

    struct cwd
    {
        Reg rax { };
        Reg rdx { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.rax, RAX );
            v.def( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0x99 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "cwd" }; }
    };

    struct cdq
    {
        Reg rax { };
        Reg rdx { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.rax, RAX );
            v.def( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0x99 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "cdq" }; }
    };

    struct cqo
    {
        Reg rax { };
        Reg rdx { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.rax, RAX );
            v.def( self.rdx, RDX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0x99 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "cqo" }; }
    };

    struct popcntw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .rep   = 0xF3,
                .bytes = { 0x0F, 0xB8 },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "popcnt {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct tzcntw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .rep   = 0xF3,
                .bytes = { 0x0F, 0xBC },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "tzcnt {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct lzcntw_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .rep   = 0xF3,
                .bytes = { 0x0F, 0xBD },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "lzcnt {}, {}", fmt_gpr( dst, 16 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct popcntl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rep   = 0xF3,
                .bytes = { 0x0F, 0xB8 },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "popcnt {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct tzcntl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rep   = 0xF3,
                .bytes = { 0x0F, 0xBC },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "tzcnt {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct lzcntl_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rep   = 0xF3,
                .bytes = { 0x0F, 0xBD },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "lzcnt {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct popcntq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rep   = 0xF3,
                .rex_w = true,
                .bytes = { 0x0F, 0xB8 },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "popcnt {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct tzcntq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rep   = 0xF3,
                .rex_w = true,
                .bytes = { 0x0F, 0xBC },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "tzcnt {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct lzcntq_rm
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.def( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rep   = 0xF3,
                .rex_w = true,
                .bytes = { 0x0F, 0xBD },
                .len   = 2
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "lzcnt {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct crc32l_rm8
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rep   = 0xF2,
                .bytes = { 0x0F, 0x38, 0xF0 },
                .len   = 3
            };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "crc32 {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct crc32l_rm16
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .p66   = true,
                .rep   = 0xF2,
                .bytes = { 0x0F, 0x38, 0xF1 },
                .len   = 3
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "crc32 {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct crc32l_rm32
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rep   = 0xF2,
                .bytes = { 0x0F, 0x38, 0xF1 },
                .len   = 3
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "crc32 {}, {}", fmt_gpr( dst, 32 ), fmt_gpr_mem( src, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct crc32q_rm8
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rep   = 0xF2,
                .rex_w = true,
                .bytes = { 0x0F, 0x38, 0xF0 },
                .len   = 3
            };
            emit_rm( buf, op, dst.as_preg( ), src, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "crc32 {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct crc32q_rm64
    {
        Reg    dst { };
        GprMem src { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.dst );
            visit_gpr_mem( self.src, v, OperandKind::Use );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode {
                .rep   = 0xF2,
                .rex_w = true,
                .bytes = { 0x0F, 0x38, 0xF1 },
                .len   = 3
            };
            emit_rm( buf, op, dst.as_preg( ), src, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "crc32 {}, {}", fmt_gpr( dst, 64 ), fmt_gpr_mem( src, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( src.value ); }
    };

    struct rep_movsb
    {
        Reg rdi { };
        Reg rsi { };
        Reg rcx { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rsi, RSI );
            v.usedef( self.rcx, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rep = 0xF3, .bytes = { 0xA4 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "rep movsb" }; }
    };

    struct rep_movsw
    {
        Reg rdi { };
        Reg rsi { };
        Reg rcx { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rsi, RSI );
            v.usedef( self.rcx, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .rep = 0xF3, .bytes = { 0xA5 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "rep movsw" }; }
    };

    struct rep_movsd
    {
        Reg rdi { };
        Reg rsi { };
        Reg rcx { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rsi, RSI );
            v.usedef( self.rcx, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rep = 0xF3, .bytes = { 0xA5 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "rep movsd" }; }
    };

    struct rep_movsq
    {
        Reg rdi { };
        Reg rsi { };
        Reg rcx { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rsi, RSI );
            v.usedef( self.rcx, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rep = 0xF3, .rex_w = true, .bytes = { 0xA5 }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "rep movsq" }; }
    };

    struct rep_stosb
    {
        Reg rdi { };
        Reg rcx { };
        Reg rax { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rcx, RCX );
            v.use( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rep = 0xF3, .bytes = { 0xAA }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "rep stosb" }; }
    };

    struct rep_stosw
    {
        Reg rdi { };
        Reg rcx { };
        Reg rax { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rcx, RCX );
            v.use( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .rep = 0xF3, .bytes = { 0xAB }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "rep stosw" }; }
    };

    struct rep_stosd
    {
        Reg rdi { };
        Reg rcx { };
        Reg rax { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rcx, RCX );
            v.use( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rep = 0xF3, .bytes = { 0xAB }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "rep stosd" }; }
    };

    struct rep_stosq
    {
        Reg rdi { };
        Reg rcx { };
        Reg rax { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rcx, RCX );
            v.use( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rep = 0xF3, .rex_w = true, .bytes = { 0xAB }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "rep stosq" }; }
    };

    struct repne_scasb
    {
        Reg rdi { };
        Reg rcx { };
        Reg rax { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rcx, RCX );
            v.use( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rep = 0xF2, .bytes = { 0xAE }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "repne scasb" }; }
    };

    struct repne_scasw
    {
        Reg rdi { };
        Reg rcx { };
        Reg rax { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rcx, RCX );
            v.use( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .rep = 0xF2, .bytes = { 0xAF }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "repne scasw" }; }
    };

    struct repne_scasd
    {
        Reg rdi { };
        Reg rcx { };
        Reg rax { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rcx, RCX );
            v.use( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rep = 0xF2, .bytes = { 0xAF }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "repne scasd" }; }
    };

    struct repne_scasq
    {
        Reg rdi { };
        Reg rcx { };
        Reg rax { };

        void visit( this auto& self, auto& v )
        {
            v.usedef( self.rdi, RDI );
            v.usedef( self.rcx, RCX );
            v.use( self.rax, RAX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rep = 0xF2, .rex_w = true, .bytes = { 0xAF }, .len = 1 };
            emit_op( buf, op );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::string { "repne scasq" }; }
    };

    struct rolb_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD0 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rolb_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD2 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rolb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC0 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorb_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD0 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorb_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD2 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC0 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rclb_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD0 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rclb_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD2 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rclb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC0 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrb_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD0 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrb_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD2 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC0 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shlb_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD0 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shlb_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD2 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shlb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC0 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrb_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD0 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrb_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD2 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC0 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarb_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD0 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarb_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD2 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, true );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}", fmt_gpr_mem( dst, 8 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarb_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC0 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, true );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}, {}", fmt_gpr_mem( dst, 8 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rolw_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rolw_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rolw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorw_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorw_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rclw_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rclw_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rclw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrw_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrw_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shlw_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shlw_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shlw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrw_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrw_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarw_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarw_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}", fmt_gpr_mem( dst, 16 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarw_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .p66 = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}, {}", fmt_gpr_mem( dst, 16 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct roll_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct roll_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct roll_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorl_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorl_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcll_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcll_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcll_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrl_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrl_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shll_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shll_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shll_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrl_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrl_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarl_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarl_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}", fmt_gpr_mem( dst, 32 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarl_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}, {}", fmt_gpr_mem( dst, 32 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rolq_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rolq_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rolq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 0, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rol {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorq_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorq_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rorq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 1, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "ror {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rclq_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rclq_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rclq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 2, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcl {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrq_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrq_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct rcrq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 3, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "rcr {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shlq_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shlq_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shlq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 4, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shl {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrq_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrq_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct shrq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 5, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "shr {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarq_m1
    {
        GprMem dst { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD1 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarq_mc
    {
        GprMem dst { };
        Reg    count { };

        void visit( this auto& self, auto& v )
        {
            visit_gpr_mem( self.dst, v, OperandKind::UseDef );
            v.use( self.count, RCX );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xD3 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}", fmt_gpr_mem( dst, 64 ) ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    struct sarq_mi8
    {
        GprMem dst { };
        int8_t imm { };

        void visit( this auto& self, auto& v )
        { visit_gpr_mem( self.dst, v, OperandKind::UseDef ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            constexpr auto op = Opcode { .rex_w = true, .bytes = { 0xC1 }, .len = 1 };
            emit_rm_digit( buf, op, 7, dst, false );
            buf.emit_u8( static_cast< uint8_t >( imm ) );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "sar {}, {}", fmt_gpr_mem( dst, 64 ), imm ); }

        [[nodiscard]] bool mem_access( ) const noexcept
        { return std::holds_alternative< AddressMode >( dst.value ); }
    };

    enum class Kind : uint16_t
    {
        addw_mr,
        addw_rm,
        addw_mi8,
        addw_mi16,
        addw_i16,
        orw_mr,
        orw_rm,
        orw_mi8,
        orw_mi16,
        orw_i16,
        adcw_mr,
        adcw_rm,
        adcw_mi8,
        adcw_mi16,
        adcw_i16,
        sbbw_mr,
        sbbw_rm,
        sbbw_mi8,
        sbbw_mi16,
        sbbw_i16,
        andw_mr,
        andw_rm,
        andw_mi8,
        andw_mi16,
        andw_i16,
        subw_mr,
        subw_rm,
        subw_mi8,
        subw_mi16,
        subw_i16,
        xorw_mr,
        xorw_rm,
        xorw_mi8,
        xorw_mi16,
        xorw_i16,
        cmpw_mr,
        cmpw_rm,
        cmpw_mi8,
        cmpw_mi16,
        cmpw_i16,
        testw_mr,
        testw_mi16,
        testw_i16,
        addl_mr,
        addl_rm,
        addl_mi8,
        addl_mi32,
        addl_i32,
        orl_mr,
        orl_rm,
        orl_mi8,
        orl_mi32,
        orl_i32,
        adcl_mr,
        adcl_rm,
        adcl_mi8,
        adcl_mi32,
        adcl_i32,
        sbbl_mr,
        sbbl_rm,
        sbbl_mi8,
        sbbl_mi32,
        sbbl_i32,
        andl_mr,
        andl_rm,
        andl_mi8,
        andl_mi32,
        andl_i32,
        subl_mr,
        subl_rm,
        subl_mi8,
        subl_mi32,
        subl_i32,
        xorl_mr,
        xorl_rm,
        xorl_mi8,
        xorl_mi32,
        xorl_i32,
        cmpl_mr,
        cmpl_rm,
        cmpl_mi8,
        cmpl_mi32,
        cmpl_i32,
        testl_mr,
        testl_mi32,
        testl_i32,
        addq_mr,
        addq_rm,
        addq_mi8,
        addq_mi32,
        addq_i32,
        orq_mr,
        orq_rm,
        orq_mi8,
        orq_mi32,
        orq_i32,
        adcq_mr,
        adcq_rm,
        adcq_mi8,
        adcq_mi32,
        adcq_i32,
        sbbq_mr,
        sbbq_rm,
        sbbq_mi8,
        sbbq_mi32,
        sbbq_i32,
        andq_mr,
        andq_rm,
        andq_mi8,
        andq_mi32,
        andq_i32,
        subq_mr,
        subq_rm,
        subq_mi8,
        subq_mi32,
        subq_i32,
        xorq_mr,
        xorq_rm,
        xorq_mi8,
        xorq_mi32,
        xorq_i32,
        cmpq_mr,
        cmpq_rm,
        cmpq_mi8,
        cmpq_mi32,
        cmpq_i32,
        testq_mr,
        testq_mi32,
        testq_i32,
        addb_mr,
        addb_rm,
        addb_mi8,
        addb_i8,
        orb_mr,
        orb_rm,
        orb_mi8,
        orb_i8,
        adcb_mr,
        adcb_rm,
        adcb_mi8,
        adcb_i8,
        sbbb_mr,
        sbbb_rm,
        sbbb_mi8,
        sbbb_i8,
        andb_mr,
        andb_rm,
        andb_mi8,
        andb_i8,
        subb_mr,
        subb_rm,
        subb_mi8,
        subb_i8,
        xorb_mr,
        xorb_rm,
        xorb_mi8,
        xorb_i8,
        cmpb_mr,
        cmpb_rm,
        cmpb_mi8,
        cmpb_i8,
        testb_mr,
        testb_mi8,
        testb_i8,
        btw_mr,
        btw_mi8,
        btsw_mr,
        btsw_mi8,
        btrw_mr,
        btrw_mi8,
        btcw_mr,
        btcw_mi8,
        bsfw_rm,
        bsrw_rm,
        btl_mr,
        btl_mi8,
        btsl_mr,
        btsl_mi8,
        btrl_mr,
        btrl_mi8,
        btcl_mr,
        btcl_mi8,
        bsfl_rm,
        bsrl_rm,
        btq_mr,
        btq_mi8,
        btsq_mr,
        btsq_mi8,
        btrq_mr,
        btrq_mi8,
        btcq_mr,
        btcq_mi8,
        bsfq_rm,
        bsrq_rm,
        leaw_rm,
        leal_rm,
        leaq_rm,
        movntil_mr,
        movntiq_mr,
        prefetchnta_m8,
        prefetcht0_m8,
        prefetcht1_m8,
        prefetcht2_m8,
        clflush_m,
        ldmxcsr_m32,
        stmxcsr_m32,
        cmpxchg8b_m64,
        nop,
        ret,
        leave,
        hlt,
        int3,
        clc,
        stc,
        cmc,
        cld,
        pause,
        ud2,
        syscall,
        cpuid,
        rdtsc,
        pushq_r,
        popq_r,
        pushq_m,
        popq_m,
        pushq_i8,
        pushq_i32,
        jmpq_m,
        callq_m,
        movw_mr,
        movw_rm,
        movw_mi16,
        xchgw_mr,
        xaddw_mr,
        cmpxchgw_mr,
        cmovw_rm,
        movzxw_rm8,
        movsxw_rm8,
        movl_mr,
        movl_rm,
        movl_mi32,
        xchgl_mr,
        xaddl_mr,
        cmpxchgl_mr,
        cmovl_rm,
        movzxl_rm8,
        movsxl_rm8,
        movq_mr,
        movq_rm,
        movq_mi32,
        xchgq_mr,
        xaddq_mr,
        cmpxchgq_mr,
        cmovq_rm,
        movzxq_rm8,
        movsxq_rm8,
        movzxl_rm16,
        movsxl_rm16,
        bswapl_r,
        movzxq_rm16,
        movsxq_rm16,
        bswapq_r,
        movsxdq_rm32,
        movw_ri16,
        movl_ri32,
        movq_ri64,
        movb_mr,
        movb_rm,
        movb_mi8,
        movb_ri8,
        xchgb_mr,
        xaddb_mr,
        cmpxchgb_mr,
        setcc_m8,
        imulw_rm,
        imulw_rmi8,
        imulw_rmi16,
        mulw_m,
        imulw_m,
        divw_m,
        idivw_m,
        notw_m,
        negw_m,
        incw_m,
        decw_m,
        imull_rm,
        imull_rmi8,
        imull_rmi32,
        mull_m,
        imull_m,
        divl_m,
        idivl_m,
        notl_m,
        negl_m,
        incl_m,
        decl_m,
        imulq_rm,
        imulq_rmi8,
        imulq_rmi32,
        mulq_m,
        imulq_m,
        divq_m,
        idivq_m,
        notq_m,
        negq_m,
        incq_m,
        decq_m,
        mulb_m,
        imulb_m,
        divb_m,
        idivb_m,
        notb_m,
        negb_m,
        incb_m,
        decb_m,
        cbw,
        cwde,
        cdqe,
        cwd,
        cdq,
        cqo,
        popcntw_rm,
        tzcntw_rm,
        lzcntw_rm,
        popcntl_rm,
        tzcntl_rm,
        lzcntl_rm,
        popcntq_rm,
        tzcntq_rm,
        lzcntq_rm,
        crc32l_rm8,
        crc32l_rm16,
        crc32l_rm32,
        crc32q_rm8,
        crc32q_rm64,
        rep_movsb,
        rep_movsw,
        rep_movsd,
        rep_movsq,
        rep_stosb,
        rep_stosw,
        rep_stosd,
        rep_stosq,
        repne_scasb,
        repne_scasw,
        repne_scasd,
        repne_scasq,
        rolb_m1,
        rolb_mc,
        rolb_mi8,
        rorb_m1,
        rorb_mc,
        rorb_mi8,
        rclb_m1,
        rclb_mc,
        rclb_mi8,
        rcrb_m1,
        rcrb_mc,
        rcrb_mi8,
        shlb_m1,
        shlb_mc,
        shlb_mi8,
        shrb_m1,
        shrb_mc,
        shrb_mi8,
        sarb_m1,
        sarb_mc,
        sarb_mi8,
        rolw_m1,
        rolw_mc,
        rolw_mi8,
        rorw_m1,
        rorw_mc,
        rorw_mi8,
        rclw_m1,
        rclw_mc,
        rclw_mi8,
        rcrw_m1,
        rcrw_mc,
        rcrw_mi8,
        shlw_m1,
        shlw_mc,
        shlw_mi8,
        shrw_m1,
        shrw_mc,
        shrw_mi8,
        sarw_m1,
        sarw_mc,
        sarw_mi8,
        roll_m1,
        roll_mc,
        roll_mi8,
        rorl_m1,
        rorl_mc,
        rorl_mi8,
        rcll_m1,
        rcll_mc,
        rcll_mi8,
        rcrl_m1,
        rcrl_mc,
        rcrl_mi8,
        shll_m1,
        shll_mc,
        shll_mi8,
        shrl_m1,
        shrl_mc,
        shrl_mi8,
        sarl_m1,
        sarl_mc,
        sarl_mi8,
        rolq_m1,
        rolq_mc,
        rolq_mi8,
        rorq_m1,
        rorq_mc,
        rorq_mi8,
        rclq_m1,
        rclq_mc,
        rclq_mi8,
        rcrq_m1,
        rcrq_mc,
        rcrq_mi8,
        shlq_m1,
        shlq_mc,
        shlq_mi8,
        shrq_m1,
        shrq_mc,
        shrq_mi8,
        sarq_m1,
        sarq_mc,
        sarq_mi8,
        MovImm,
        Jmp,
        Jcc,
        Call,
        CallInd,
        LoadExtName,
        StackAddr,
        DummyUse,
    };

    inline constexpr std::size_t kind_count = 439;

    template < typename T >
    struct kind_of;

    template <>
    struct kind_of< addw_mr >
    {
        static constexpr Kind value = Kind::addw_mr;
    };

    template <>
    struct kind_of< addw_rm >
    {
        static constexpr Kind value = Kind::addw_rm;
    };

    template <>
    struct kind_of< addw_mi8 >
    {
        static constexpr Kind value = Kind::addw_mi8;
    };

    template <>
    struct kind_of< addw_mi16 >
    {
        static constexpr Kind value = Kind::addw_mi16;
    };

    template <>
    struct kind_of< addw_i16 >
    {
        static constexpr Kind value = Kind::addw_i16;
    };

    template <>
    struct kind_of< orw_mr >
    {
        static constexpr Kind value = Kind::orw_mr;
    };

    template <>
    struct kind_of< orw_rm >
    {
        static constexpr Kind value = Kind::orw_rm;
    };

    template <>
    struct kind_of< orw_mi8 >
    {
        static constexpr Kind value = Kind::orw_mi8;
    };

    template <>
    struct kind_of< orw_mi16 >
    {
        static constexpr Kind value = Kind::orw_mi16;
    };

    template <>
    struct kind_of< orw_i16 >
    {
        static constexpr Kind value = Kind::orw_i16;
    };

    template <>
    struct kind_of< adcw_mr >
    {
        static constexpr Kind value = Kind::adcw_mr;
    };

    template <>
    struct kind_of< adcw_rm >
    {
        static constexpr Kind value = Kind::adcw_rm;
    };

    template <>
    struct kind_of< adcw_mi8 >
    {
        static constexpr Kind value = Kind::adcw_mi8;
    };

    template <>
    struct kind_of< adcw_mi16 >
    {
        static constexpr Kind value = Kind::adcw_mi16;
    };

    template <>
    struct kind_of< adcw_i16 >
    {
        static constexpr Kind value = Kind::adcw_i16;
    };

    template <>
    struct kind_of< sbbw_mr >
    {
        static constexpr Kind value = Kind::sbbw_mr;
    };

    template <>
    struct kind_of< sbbw_rm >
    {
        static constexpr Kind value = Kind::sbbw_rm;
    };

    template <>
    struct kind_of< sbbw_mi8 >
    {
        static constexpr Kind value = Kind::sbbw_mi8;
    };

    template <>
    struct kind_of< sbbw_mi16 >
    {
        static constexpr Kind value = Kind::sbbw_mi16;
    };

    template <>
    struct kind_of< sbbw_i16 >
    {
        static constexpr Kind value = Kind::sbbw_i16;
    };

    template <>
    struct kind_of< andw_mr >
    {
        static constexpr Kind value = Kind::andw_mr;
    };

    template <>
    struct kind_of< andw_rm >
    {
        static constexpr Kind value = Kind::andw_rm;
    };

    template <>
    struct kind_of< andw_mi8 >
    {
        static constexpr Kind value = Kind::andw_mi8;
    };

    template <>
    struct kind_of< andw_mi16 >
    {
        static constexpr Kind value = Kind::andw_mi16;
    };

    template <>
    struct kind_of< andw_i16 >
    {
        static constexpr Kind value = Kind::andw_i16;
    };

    template <>
    struct kind_of< subw_mr >
    {
        static constexpr Kind value = Kind::subw_mr;
    };

    template <>
    struct kind_of< subw_rm >
    {
        static constexpr Kind value = Kind::subw_rm;
    };

    template <>
    struct kind_of< subw_mi8 >
    {
        static constexpr Kind value = Kind::subw_mi8;
    };

    template <>
    struct kind_of< subw_mi16 >
    {
        static constexpr Kind value = Kind::subw_mi16;
    };

    template <>
    struct kind_of< subw_i16 >
    {
        static constexpr Kind value = Kind::subw_i16;
    };

    template <>
    struct kind_of< xorw_mr >
    {
        static constexpr Kind value = Kind::xorw_mr;
    };

    template <>
    struct kind_of< xorw_rm >
    {
        static constexpr Kind value = Kind::xorw_rm;
    };

    template <>
    struct kind_of< xorw_mi8 >
    {
        static constexpr Kind value = Kind::xorw_mi8;
    };

    template <>
    struct kind_of< xorw_mi16 >
    {
        static constexpr Kind value = Kind::xorw_mi16;
    };

    template <>
    struct kind_of< xorw_i16 >
    {
        static constexpr Kind value = Kind::xorw_i16;
    };

    template <>
    struct kind_of< cmpw_mr >
    {
        static constexpr Kind value = Kind::cmpw_mr;
    };

    template <>
    struct kind_of< cmpw_rm >
    {
        static constexpr Kind value = Kind::cmpw_rm;
    };

    template <>
    struct kind_of< cmpw_mi8 >
    {
        static constexpr Kind value = Kind::cmpw_mi8;
    };

    template <>
    struct kind_of< cmpw_mi16 >
    {
        static constexpr Kind value = Kind::cmpw_mi16;
    };

    template <>
    struct kind_of< cmpw_i16 >
    {
        static constexpr Kind value = Kind::cmpw_i16;
    };

    template <>
    struct kind_of< testw_mr >
    {
        static constexpr Kind value = Kind::testw_mr;
    };

    template <>
    struct kind_of< testw_mi16 >
    {
        static constexpr Kind value = Kind::testw_mi16;
    };

    template <>
    struct kind_of< testw_i16 >
    {
        static constexpr Kind value = Kind::testw_i16;
    };

    template <>
    struct kind_of< addl_mr >
    {
        static constexpr Kind value = Kind::addl_mr;
    };

    template <>
    struct kind_of< addl_rm >
    {
        static constexpr Kind value = Kind::addl_rm;
    };

    template <>
    struct kind_of< addl_mi8 >
    {
        static constexpr Kind value = Kind::addl_mi8;
    };

    template <>
    struct kind_of< addl_mi32 >
    {
        static constexpr Kind value = Kind::addl_mi32;
    };

    template <>
    struct kind_of< addl_i32 >
    {
        static constexpr Kind value = Kind::addl_i32;
    };

    template <>
    struct kind_of< orl_mr >
    {
        static constexpr Kind value = Kind::orl_mr;
    };

    template <>
    struct kind_of< orl_rm >
    {
        static constexpr Kind value = Kind::orl_rm;
    };

    template <>
    struct kind_of< orl_mi8 >
    {
        static constexpr Kind value = Kind::orl_mi8;
    };

    template <>
    struct kind_of< orl_mi32 >
    {
        static constexpr Kind value = Kind::orl_mi32;
    };

    template <>
    struct kind_of< orl_i32 >
    {
        static constexpr Kind value = Kind::orl_i32;
    };

    template <>
    struct kind_of< adcl_mr >
    {
        static constexpr Kind value = Kind::adcl_mr;
    };

    template <>
    struct kind_of< adcl_rm >
    {
        static constexpr Kind value = Kind::adcl_rm;
    };

    template <>
    struct kind_of< adcl_mi8 >
    {
        static constexpr Kind value = Kind::adcl_mi8;
    };

    template <>
    struct kind_of< adcl_mi32 >
    {
        static constexpr Kind value = Kind::adcl_mi32;
    };

    template <>
    struct kind_of< adcl_i32 >
    {
        static constexpr Kind value = Kind::adcl_i32;
    };

    template <>
    struct kind_of< sbbl_mr >
    {
        static constexpr Kind value = Kind::sbbl_mr;
    };

    template <>
    struct kind_of< sbbl_rm >
    {
        static constexpr Kind value = Kind::sbbl_rm;
    };

    template <>
    struct kind_of< sbbl_mi8 >
    {
        static constexpr Kind value = Kind::sbbl_mi8;
    };

    template <>
    struct kind_of< sbbl_mi32 >
    {
        static constexpr Kind value = Kind::sbbl_mi32;
    };

    template <>
    struct kind_of< sbbl_i32 >
    {
        static constexpr Kind value = Kind::sbbl_i32;
    };

    template <>
    struct kind_of< andl_mr >
    {
        static constexpr Kind value = Kind::andl_mr;
    };

    template <>
    struct kind_of< andl_rm >
    {
        static constexpr Kind value = Kind::andl_rm;
    };

    template <>
    struct kind_of< andl_mi8 >
    {
        static constexpr Kind value = Kind::andl_mi8;
    };

    template <>
    struct kind_of< andl_mi32 >
    {
        static constexpr Kind value = Kind::andl_mi32;
    };

    template <>
    struct kind_of< andl_i32 >
    {
        static constexpr Kind value = Kind::andl_i32;
    };

    template <>
    struct kind_of< subl_mr >
    {
        static constexpr Kind value = Kind::subl_mr;
    };

    template <>
    struct kind_of< subl_rm >
    {
        static constexpr Kind value = Kind::subl_rm;
    };

    template <>
    struct kind_of< subl_mi8 >
    {
        static constexpr Kind value = Kind::subl_mi8;
    };

    template <>
    struct kind_of< subl_mi32 >
    {
        static constexpr Kind value = Kind::subl_mi32;
    };

    template <>
    struct kind_of< subl_i32 >
    {
        static constexpr Kind value = Kind::subl_i32;
    };

    template <>
    struct kind_of< xorl_mr >
    {
        static constexpr Kind value = Kind::xorl_mr;
    };

    template <>
    struct kind_of< xorl_rm >
    {
        static constexpr Kind value = Kind::xorl_rm;
    };

    template <>
    struct kind_of< xorl_mi8 >
    {
        static constexpr Kind value = Kind::xorl_mi8;
    };

    template <>
    struct kind_of< xorl_mi32 >
    {
        static constexpr Kind value = Kind::xorl_mi32;
    };

    template <>
    struct kind_of< xorl_i32 >
    {
        static constexpr Kind value = Kind::xorl_i32;
    };

    template <>
    struct kind_of< cmpl_mr >
    {
        static constexpr Kind value = Kind::cmpl_mr;
    };

    template <>
    struct kind_of< cmpl_rm >
    {
        static constexpr Kind value = Kind::cmpl_rm;
    };

    template <>
    struct kind_of< cmpl_mi8 >
    {
        static constexpr Kind value = Kind::cmpl_mi8;
    };

    template <>
    struct kind_of< cmpl_mi32 >
    {
        static constexpr Kind value = Kind::cmpl_mi32;
    };

    template <>
    struct kind_of< cmpl_i32 >
    {
        static constexpr Kind value = Kind::cmpl_i32;
    };

    template <>
    struct kind_of< testl_mr >
    {
        static constexpr Kind value = Kind::testl_mr;
    };

    template <>
    struct kind_of< testl_mi32 >
    {
        static constexpr Kind value = Kind::testl_mi32;
    };

    template <>
    struct kind_of< testl_i32 >
    {
        static constexpr Kind value = Kind::testl_i32;
    };

    template <>
    struct kind_of< addq_mr >
    {
        static constexpr Kind value = Kind::addq_mr;
    };

    template <>
    struct kind_of< addq_rm >
    {
        static constexpr Kind value = Kind::addq_rm;
    };

    template <>
    struct kind_of< addq_mi8 >
    {
        static constexpr Kind value = Kind::addq_mi8;
    };

    template <>
    struct kind_of< addq_mi32 >
    {
        static constexpr Kind value = Kind::addq_mi32;
    };

    template <>
    struct kind_of< addq_i32 >
    {
        static constexpr Kind value = Kind::addq_i32;
    };

    template <>
    struct kind_of< orq_mr >
    {
        static constexpr Kind value = Kind::orq_mr;
    };

    template <>
    struct kind_of< orq_rm >
    {
        static constexpr Kind value = Kind::orq_rm;
    };

    template <>
    struct kind_of< orq_mi8 >
    {
        static constexpr Kind value = Kind::orq_mi8;
    };

    template <>
    struct kind_of< orq_mi32 >
    {
        static constexpr Kind value = Kind::orq_mi32;
    };

    template <>
    struct kind_of< orq_i32 >
    {
        static constexpr Kind value = Kind::orq_i32;
    };

    template <>
    struct kind_of< adcq_mr >
    {
        static constexpr Kind value = Kind::adcq_mr;
    };

    template <>
    struct kind_of< adcq_rm >
    {
        static constexpr Kind value = Kind::adcq_rm;
    };

    template <>
    struct kind_of< adcq_mi8 >
    {
        static constexpr Kind value = Kind::adcq_mi8;
    };

    template <>
    struct kind_of< adcq_mi32 >
    {
        static constexpr Kind value = Kind::adcq_mi32;
    };

    template <>
    struct kind_of< adcq_i32 >
    {
        static constexpr Kind value = Kind::adcq_i32;
    };

    template <>
    struct kind_of< sbbq_mr >
    {
        static constexpr Kind value = Kind::sbbq_mr;
    };

    template <>
    struct kind_of< sbbq_rm >
    {
        static constexpr Kind value = Kind::sbbq_rm;
    };

    template <>
    struct kind_of< sbbq_mi8 >
    {
        static constexpr Kind value = Kind::sbbq_mi8;
    };

    template <>
    struct kind_of< sbbq_mi32 >
    {
        static constexpr Kind value = Kind::sbbq_mi32;
    };

    template <>
    struct kind_of< sbbq_i32 >
    {
        static constexpr Kind value = Kind::sbbq_i32;
    };

    template <>
    struct kind_of< andq_mr >
    {
        static constexpr Kind value = Kind::andq_mr;
    };

    template <>
    struct kind_of< andq_rm >
    {
        static constexpr Kind value = Kind::andq_rm;
    };

    template <>
    struct kind_of< andq_mi8 >
    {
        static constexpr Kind value = Kind::andq_mi8;
    };

    template <>
    struct kind_of< andq_mi32 >
    {
        static constexpr Kind value = Kind::andq_mi32;
    };

    template <>
    struct kind_of< andq_i32 >
    {
        static constexpr Kind value = Kind::andq_i32;
    };

    template <>
    struct kind_of< subq_mr >
    {
        static constexpr Kind value = Kind::subq_mr;
    };

    template <>
    struct kind_of< subq_rm >
    {
        static constexpr Kind value = Kind::subq_rm;
    };

    template <>
    struct kind_of< subq_mi8 >
    {
        static constexpr Kind value = Kind::subq_mi8;
    };

    template <>
    struct kind_of< subq_mi32 >
    {
        static constexpr Kind value = Kind::subq_mi32;
    };

    template <>
    struct kind_of< subq_i32 >
    {
        static constexpr Kind value = Kind::subq_i32;
    };

    template <>
    struct kind_of< xorq_mr >
    {
        static constexpr Kind value = Kind::xorq_mr;
    };

    template <>
    struct kind_of< xorq_rm >
    {
        static constexpr Kind value = Kind::xorq_rm;
    };

    template <>
    struct kind_of< xorq_mi8 >
    {
        static constexpr Kind value = Kind::xorq_mi8;
    };

    template <>
    struct kind_of< xorq_mi32 >
    {
        static constexpr Kind value = Kind::xorq_mi32;
    };

    template <>
    struct kind_of< xorq_i32 >
    {
        static constexpr Kind value = Kind::xorq_i32;
    };

    template <>
    struct kind_of< cmpq_mr >
    {
        static constexpr Kind value = Kind::cmpq_mr;
    };

    template <>
    struct kind_of< cmpq_rm >
    {
        static constexpr Kind value = Kind::cmpq_rm;
    };

    template <>
    struct kind_of< cmpq_mi8 >
    {
        static constexpr Kind value = Kind::cmpq_mi8;
    };

    template <>
    struct kind_of< cmpq_mi32 >
    {
        static constexpr Kind value = Kind::cmpq_mi32;
    };

    template <>
    struct kind_of< cmpq_i32 >
    {
        static constexpr Kind value = Kind::cmpq_i32;
    };

    template <>
    struct kind_of< testq_mr >
    {
        static constexpr Kind value = Kind::testq_mr;
    };

    template <>
    struct kind_of< testq_mi32 >
    {
        static constexpr Kind value = Kind::testq_mi32;
    };

    template <>
    struct kind_of< testq_i32 >
    {
        static constexpr Kind value = Kind::testq_i32;
    };

    template <>
    struct kind_of< addb_mr >
    {
        static constexpr Kind value = Kind::addb_mr;
    };

    template <>
    struct kind_of< addb_rm >
    {
        static constexpr Kind value = Kind::addb_rm;
    };

    template <>
    struct kind_of< addb_mi8 >
    {
        static constexpr Kind value = Kind::addb_mi8;
    };

    template <>
    struct kind_of< addb_i8 >
    {
        static constexpr Kind value = Kind::addb_i8;
    };

    template <>
    struct kind_of< orb_mr >
    {
        static constexpr Kind value = Kind::orb_mr;
    };

    template <>
    struct kind_of< orb_rm >
    {
        static constexpr Kind value = Kind::orb_rm;
    };

    template <>
    struct kind_of< orb_mi8 >
    {
        static constexpr Kind value = Kind::orb_mi8;
    };

    template <>
    struct kind_of< orb_i8 >
    {
        static constexpr Kind value = Kind::orb_i8;
    };

    template <>
    struct kind_of< adcb_mr >
    {
        static constexpr Kind value = Kind::adcb_mr;
    };

    template <>
    struct kind_of< adcb_rm >
    {
        static constexpr Kind value = Kind::adcb_rm;
    };

    template <>
    struct kind_of< adcb_mi8 >
    {
        static constexpr Kind value = Kind::adcb_mi8;
    };

    template <>
    struct kind_of< adcb_i8 >
    {
        static constexpr Kind value = Kind::adcb_i8;
    };

    template <>
    struct kind_of< sbbb_mr >
    {
        static constexpr Kind value = Kind::sbbb_mr;
    };

    template <>
    struct kind_of< sbbb_rm >
    {
        static constexpr Kind value = Kind::sbbb_rm;
    };

    template <>
    struct kind_of< sbbb_mi8 >
    {
        static constexpr Kind value = Kind::sbbb_mi8;
    };

    template <>
    struct kind_of< sbbb_i8 >
    {
        static constexpr Kind value = Kind::sbbb_i8;
    };

    template <>
    struct kind_of< andb_mr >
    {
        static constexpr Kind value = Kind::andb_mr;
    };

    template <>
    struct kind_of< andb_rm >
    {
        static constexpr Kind value = Kind::andb_rm;
    };

    template <>
    struct kind_of< andb_mi8 >
    {
        static constexpr Kind value = Kind::andb_mi8;
    };

    template <>
    struct kind_of< andb_i8 >
    {
        static constexpr Kind value = Kind::andb_i8;
    };

    template <>
    struct kind_of< subb_mr >
    {
        static constexpr Kind value = Kind::subb_mr;
    };

    template <>
    struct kind_of< subb_rm >
    {
        static constexpr Kind value = Kind::subb_rm;
    };

    template <>
    struct kind_of< subb_mi8 >
    {
        static constexpr Kind value = Kind::subb_mi8;
    };

    template <>
    struct kind_of< subb_i8 >
    {
        static constexpr Kind value = Kind::subb_i8;
    };

    template <>
    struct kind_of< xorb_mr >
    {
        static constexpr Kind value = Kind::xorb_mr;
    };

    template <>
    struct kind_of< xorb_rm >
    {
        static constexpr Kind value = Kind::xorb_rm;
    };

    template <>
    struct kind_of< xorb_mi8 >
    {
        static constexpr Kind value = Kind::xorb_mi8;
    };

    template <>
    struct kind_of< xorb_i8 >
    {
        static constexpr Kind value = Kind::xorb_i8;
    };

    template <>
    struct kind_of< cmpb_mr >
    {
        static constexpr Kind value = Kind::cmpb_mr;
    };

    template <>
    struct kind_of< cmpb_rm >
    {
        static constexpr Kind value = Kind::cmpb_rm;
    };

    template <>
    struct kind_of< cmpb_mi8 >
    {
        static constexpr Kind value = Kind::cmpb_mi8;
    };

    template <>
    struct kind_of< cmpb_i8 >
    {
        static constexpr Kind value = Kind::cmpb_i8;
    };

    template <>
    struct kind_of< testb_mr >
    {
        static constexpr Kind value = Kind::testb_mr;
    };

    template <>
    struct kind_of< testb_mi8 >
    {
        static constexpr Kind value = Kind::testb_mi8;
    };

    template <>
    struct kind_of< testb_i8 >
    {
        static constexpr Kind value = Kind::testb_i8;
    };

    template <>
    struct kind_of< btw_mr >
    {
        static constexpr Kind value = Kind::btw_mr;
    };

    template <>
    struct kind_of< btw_mi8 >
    {
        static constexpr Kind value = Kind::btw_mi8;
    };

    template <>
    struct kind_of< btsw_mr >
    {
        static constexpr Kind value = Kind::btsw_mr;
    };

    template <>
    struct kind_of< btsw_mi8 >
    {
        static constexpr Kind value = Kind::btsw_mi8;
    };

    template <>
    struct kind_of< btrw_mr >
    {
        static constexpr Kind value = Kind::btrw_mr;
    };

    template <>
    struct kind_of< btrw_mi8 >
    {
        static constexpr Kind value = Kind::btrw_mi8;
    };

    template <>
    struct kind_of< btcw_mr >
    {
        static constexpr Kind value = Kind::btcw_mr;
    };

    template <>
    struct kind_of< btcw_mi8 >
    {
        static constexpr Kind value = Kind::btcw_mi8;
    };

    template <>
    struct kind_of< bsfw_rm >
    {
        static constexpr Kind value = Kind::bsfw_rm;
    };

    template <>
    struct kind_of< bsrw_rm >
    {
        static constexpr Kind value = Kind::bsrw_rm;
    };

    template <>
    struct kind_of< btl_mr >
    {
        static constexpr Kind value = Kind::btl_mr;
    };

    template <>
    struct kind_of< btl_mi8 >
    {
        static constexpr Kind value = Kind::btl_mi8;
    };

    template <>
    struct kind_of< btsl_mr >
    {
        static constexpr Kind value = Kind::btsl_mr;
    };

    template <>
    struct kind_of< btsl_mi8 >
    {
        static constexpr Kind value = Kind::btsl_mi8;
    };

    template <>
    struct kind_of< btrl_mr >
    {
        static constexpr Kind value = Kind::btrl_mr;
    };

    template <>
    struct kind_of< btrl_mi8 >
    {
        static constexpr Kind value = Kind::btrl_mi8;
    };

    template <>
    struct kind_of< btcl_mr >
    {
        static constexpr Kind value = Kind::btcl_mr;
    };

    template <>
    struct kind_of< btcl_mi8 >
    {
        static constexpr Kind value = Kind::btcl_mi8;
    };

    template <>
    struct kind_of< bsfl_rm >
    {
        static constexpr Kind value = Kind::bsfl_rm;
    };

    template <>
    struct kind_of< bsrl_rm >
    {
        static constexpr Kind value = Kind::bsrl_rm;
    };

    template <>
    struct kind_of< btq_mr >
    {
        static constexpr Kind value = Kind::btq_mr;
    };

    template <>
    struct kind_of< btq_mi8 >
    {
        static constexpr Kind value = Kind::btq_mi8;
    };

    template <>
    struct kind_of< btsq_mr >
    {
        static constexpr Kind value = Kind::btsq_mr;
    };

    template <>
    struct kind_of< btsq_mi8 >
    {
        static constexpr Kind value = Kind::btsq_mi8;
    };

    template <>
    struct kind_of< btrq_mr >
    {
        static constexpr Kind value = Kind::btrq_mr;
    };

    template <>
    struct kind_of< btrq_mi8 >
    {
        static constexpr Kind value = Kind::btrq_mi8;
    };

    template <>
    struct kind_of< btcq_mr >
    {
        static constexpr Kind value = Kind::btcq_mr;
    };

    template <>
    struct kind_of< btcq_mi8 >
    {
        static constexpr Kind value = Kind::btcq_mi8;
    };

    template <>
    struct kind_of< bsfq_rm >
    {
        static constexpr Kind value = Kind::bsfq_rm;
    };

    template <>
    struct kind_of< bsrq_rm >
    {
        static constexpr Kind value = Kind::bsrq_rm;
    };

    template <>
    struct kind_of< leaw_rm >
    {
        static constexpr Kind value = Kind::leaw_rm;
    };

    template <>
    struct kind_of< leal_rm >
    {
        static constexpr Kind value = Kind::leal_rm;
    };

    template <>
    struct kind_of< leaq_rm >
    {
        static constexpr Kind value = Kind::leaq_rm;
    };

    template <>
    struct kind_of< movntil_mr >
    {
        static constexpr Kind value = Kind::movntil_mr;
    };

    template <>
    struct kind_of< movntiq_mr >
    {
        static constexpr Kind value = Kind::movntiq_mr;
    };

    template <>
    struct kind_of< prefetchnta_m8 >
    {
        static constexpr Kind value = Kind::prefetchnta_m8;
    };

    template <>
    struct kind_of< prefetcht0_m8 >
    {
        static constexpr Kind value = Kind::prefetcht0_m8;
    };

    template <>
    struct kind_of< prefetcht1_m8 >
    {
        static constexpr Kind value = Kind::prefetcht1_m8;
    };

    template <>
    struct kind_of< prefetcht2_m8 >
    {
        static constexpr Kind value = Kind::prefetcht2_m8;
    };

    template <>
    struct kind_of< clflush_m >
    {
        static constexpr Kind value = Kind::clflush_m;
    };

    template <>
    struct kind_of< ldmxcsr_m32 >
    {
        static constexpr Kind value = Kind::ldmxcsr_m32;
    };

    template <>
    struct kind_of< stmxcsr_m32 >
    {
        static constexpr Kind value = Kind::stmxcsr_m32;
    };

    template <>
    struct kind_of< cmpxchg8b_m64 >
    {
        static constexpr Kind value = Kind::cmpxchg8b_m64;
    };

    template <>
    struct kind_of< nop >
    {
        static constexpr Kind value = Kind::nop;
    };

    template <>
    struct kind_of< ret >
    {
        static constexpr Kind value = Kind::ret;
    };

    template <>
    struct kind_of< leave >
    {
        static constexpr Kind value = Kind::leave;
    };

    template <>
    struct kind_of< hlt >
    {
        static constexpr Kind value = Kind::hlt;
    };

    template <>
    struct kind_of< int3 >
    {
        static constexpr Kind value = Kind::int3;
    };

    template <>
    struct kind_of< clc >
    {
        static constexpr Kind value = Kind::clc;
    };

    template <>
    struct kind_of< stc >
    {
        static constexpr Kind value = Kind::stc;
    };

    template <>
    struct kind_of< cmc >
    {
        static constexpr Kind value = Kind::cmc;
    };

    template <>
    struct kind_of< cld >
    {
        static constexpr Kind value = Kind::cld;
    };

    template <>
    struct kind_of< pause >
    {
        static constexpr Kind value = Kind::pause;
    };

    template <>
    struct kind_of< ud2 >
    {
        static constexpr Kind value = Kind::ud2;
    };

    template <>
    struct kind_of< syscall >
    {
        static constexpr Kind value = Kind::syscall;
    };

    template <>
    struct kind_of< cpuid >
    {
        static constexpr Kind value = Kind::cpuid;
    };

    template <>
    struct kind_of< rdtsc >
    {
        static constexpr Kind value = Kind::rdtsc;
    };

    template <>
    struct kind_of< pushq_r >
    {
        static constexpr Kind value = Kind::pushq_r;
    };

    template <>
    struct kind_of< popq_r >
    {
        static constexpr Kind value = Kind::popq_r;
    };

    template <>
    struct kind_of< pushq_m >
    {
        static constexpr Kind value = Kind::pushq_m;
    };

    template <>
    struct kind_of< popq_m >
    {
        static constexpr Kind value = Kind::popq_m;
    };

    template <>
    struct kind_of< pushq_i8 >
    {
        static constexpr Kind value = Kind::pushq_i8;
    };

    template <>
    struct kind_of< pushq_i32 >
    {
        static constexpr Kind value = Kind::pushq_i32;
    };

    template <>
    struct kind_of< jmpq_m >
    {
        static constexpr Kind value = Kind::jmpq_m;
    };

    template <>
    struct kind_of< callq_m >
    {
        static constexpr Kind value = Kind::callq_m;
    };

    template <>
    struct kind_of< movw_mr >
    {
        static constexpr Kind value = Kind::movw_mr;
    };

    template <>
    struct kind_of< movw_rm >
    {
        static constexpr Kind value = Kind::movw_rm;
    };

    template <>
    struct kind_of< movw_mi16 >
    {
        static constexpr Kind value = Kind::movw_mi16;
    };

    template <>
    struct kind_of< xchgw_mr >
    {
        static constexpr Kind value = Kind::xchgw_mr;
    };

    template <>
    struct kind_of< xaddw_mr >
    {
        static constexpr Kind value = Kind::xaddw_mr;
    };

    template <>
    struct kind_of< cmpxchgw_mr >
    {
        static constexpr Kind value = Kind::cmpxchgw_mr;
    };

    template <>
    struct kind_of< cmovw_rm >
    {
        static constexpr Kind value = Kind::cmovw_rm;
    };

    template <>
    struct kind_of< movzxw_rm8 >
    {
        static constexpr Kind value = Kind::movzxw_rm8;
    };

    template <>
    struct kind_of< movsxw_rm8 >
    {
        static constexpr Kind value = Kind::movsxw_rm8;
    };

    template <>
    struct kind_of< movl_mr >
    {
        static constexpr Kind value = Kind::movl_mr;
    };

    template <>
    struct kind_of< movl_rm >
    {
        static constexpr Kind value = Kind::movl_rm;
    };

    template <>
    struct kind_of< movl_mi32 >
    {
        static constexpr Kind value = Kind::movl_mi32;
    };

    template <>
    struct kind_of< xchgl_mr >
    {
        static constexpr Kind value = Kind::xchgl_mr;
    };

    template <>
    struct kind_of< xaddl_mr >
    {
        static constexpr Kind value = Kind::xaddl_mr;
    };

    template <>
    struct kind_of< cmpxchgl_mr >
    {
        static constexpr Kind value = Kind::cmpxchgl_mr;
    };

    template <>
    struct kind_of< cmovl_rm >
    {
        static constexpr Kind value = Kind::cmovl_rm;
    };

    template <>
    struct kind_of< movzxl_rm8 >
    {
        static constexpr Kind value = Kind::movzxl_rm8;
    };

    template <>
    struct kind_of< movsxl_rm8 >
    {
        static constexpr Kind value = Kind::movsxl_rm8;
    };

    template <>
    struct kind_of< movq_mr >
    {
        static constexpr Kind value = Kind::movq_mr;
    };

    template <>
    struct kind_of< movq_rm >
    {
        static constexpr Kind value = Kind::movq_rm;
    };

    template <>
    struct kind_of< movq_mi32 >
    {
        static constexpr Kind value = Kind::movq_mi32;
    };

    template <>
    struct kind_of< xchgq_mr >
    {
        static constexpr Kind value = Kind::xchgq_mr;
    };

    template <>
    struct kind_of< xaddq_mr >
    {
        static constexpr Kind value = Kind::xaddq_mr;
    };

    template <>
    struct kind_of< cmpxchgq_mr >
    {
        static constexpr Kind value = Kind::cmpxchgq_mr;
    };

    template <>
    struct kind_of< cmovq_rm >
    {
        static constexpr Kind value = Kind::cmovq_rm;
    };

    template <>
    struct kind_of< movzxq_rm8 >
    {
        static constexpr Kind value = Kind::movzxq_rm8;
    };

    template <>
    struct kind_of< movsxq_rm8 >
    {
        static constexpr Kind value = Kind::movsxq_rm8;
    };

    template <>
    struct kind_of< movzxl_rm16 >
    {
        static constexpr Kind value = Kind::movzxl_rm16;
    };

    template <>
    struct kind_of< movsxl_rm16 >
    {
        static constexpr Kind value = Kind::movsxl_rm16;
    };

    template <>
    struct kind_of< bswapl_r >
    {
        static constexpr Kind value = Kind::bswapl_r;
    };

    template <>
    struct kind_of< movzxq_rm16 >
    {
        static constexpr Kind value = Kind::movzxq_rm16;
    };

    template <>
    struct kind_of< movsxq_rm16 >
    {
        static constexpr Kind value = Kind::movsxq_rm16;
    };

    template <>
    struct kind_of< bswapq_r >
    {
        static constexpr Kind value = Kind::bswapq_r;
    };

    template <>
    struct kind_of< movsxdq_rm32 >
    {
        static constexpr Kind value = Kind::movsxdq_rm32;
    };

    template <>
    struct kind_of< movw_ri16 >
    {
        static constexpr Kind value = Kind::movw_ri16;
    };

    template <>
    struct kind_of< movl_ri32 >
    {
        static constexpr Kind value = Kind::movl_ri32;
    };

    template <>
    struct kind_of< movq_ri64 >
    {
        static constexpr Kind value = Kind::movq_ri64;
    };

    template <>
    struct kind_of< movb_mr >
    {
        static constexpr Kind value = Kind::movb_mr;
    };

    template <>
    struct kind_of< movb_rm >
    {
        static constexpr Kind value = Kind::movb_rm;
    };

    template <>
    struct kind_of< movb_mi8 >
    {
        static constexpr Kind value = Kind::movb_mi8;
    };

    template <>
    struct kind_of< movb_ri8 >
    {
        static constexpr Kind value = Kind::movb_ri8;
    };

    template <>
    struct kind_of< xchgb_mr >
    {
        static constexpr Kind value = Kind::xchgb_mr;
    };

    template <>
    struct kind_of< xaddb_mr >
    {
        static constexpr Kind value = Kind::xaddb_mr;
    };

    template <>
    struct kind_of< cmpxchgb_mr >
    {
        static constexpr Kind value = Kind::cmpxchgb_mr;
    };

    template <>
    struct kind_of< setcc_m8 >
    {
        static constexpr Kind value = Kind::setcc_m8;
    };

    template <>
    struct kind_of< imulw_rm >
    {
        static constexpr Kind value = Kind::imulw_rm;
    };

    template <>
    struct kind_of< imulw_rmi8 >
    {
        static constexpr Kind value = Kind::imulw_rmi8;
    };

    template <>
    struct kind_of< imulw_rmi16 >
    {
        static constexpr Kind value = Kind::imulw_rmi16;
    };

    template <>
    struct kind_of< mulw_m >
    {
        static constexpr Kind value = Kind::mulw_m;
    };

    template <>
    struct kind_of< imulw_m >
    {
        static constexpr Kind value = Kind::imulw_m;
    };

    template <>
    struct kind_of< divw_m >
    {
        static constexpr Kind value = Kind::divw_m;
    };

    template <>
    struct kind_of< idivw_m >
    {
        static constexpr Kind value = Kind::idivw_m;
    };

    template <>
    struct kind_of< notw_m >
    {
        static constexpr Kind value = Kind::notw_m;
    };

    template <>
    struct kind_of< negw_m >
    {
        static constexpr Kind value = Kind::negw_m;
    };

    template <>
    struct kind_of< incw_m >
    {
        static constexpr Kind value = Kind::incw_m;
    };

    template <>
    struct kind_of< decw_m >
    {
        static constexpr Kind value = Kind::decw_m;
    };

    template <>
    struct kind_of< imull_rm >
    {
        static constexpr Kind value = Kind::imull_rm;
    };

    template <>
    struct kind_of< imull_rmi8 >
    {
        static constexpr Kind value = Kind::imull_rmi8;
    };

    template <>
    struct kind_of< imull_rmi32 >
    {
        static constexpr Kind value = Kind::imull_rmi32;
    };

    template <>
    struct kind_of< mull_m >
    {
        static constexpr Kind value = Kind::mull_m;
    };

    template <>
    struct kind_of< imull_m >
    {
        static constexpr Kind value = Kind::imull_m;
    };

    template <>
    struct kind_of< divl_m >
    {
        static constexpr Kind value = Kind::divl_m;
    };

    template <>
    struct kind_of< idivl_m >
    {
        static constexpr Kind value = Kind::idivl_m;
    };

    template <>
    struct kind_of< notl_m >
    {
        static constexpr Kind value = Kind::notl_m;
    };

    template <>
    struct kind_of< negl_m >
    {
        static constexpr Kind value = Kind::negl_m;
    };

    template <>
    struct kind_of< incl_m >
    {
        static constexpr Kind value = Kind::incl_m;
    };

    template <>
    struct kind_of< decl_m >
    {
        static constexpr Kind value = Kind::decl_m;
    };

    template <>
    struct kind_of< imulq_rm >
    {
        static constexpr Kind value = Kind::imulq_rm;
    };

    template <>
    struct kind_of< imulq_rmi8 >
    {
        static constexpr Kind value = Kind::imulq_rmi8;
    };

    template <>
    struct kind_of< imulq_rmi32 >
    {
        static constexpr Kind value = Kind::imulq_rmi32;
    };

    template <>
    struct kind_of< mulq_m >
    {
        static constexpr Kind value = Kind::mulq_m;
    };

    template <>
    struct kind_of< imulq_m >
    {
        static constexpr Kind value = Kind::imulq_m;
    };

    template <>
    struct kind_of< divq_m >
    {
        static constexpr Kind value = Kind::divq_m;
    };

    template <>
    struct kind_of< idivq_m >
    {
        static constexpr Kind value = Kind::idivq_m;
    };

    template <>
    struct kind_of< notq_m >
    {
        static constexpr Kind value = Kind::notq_m;
    };

    template <>
    struct kind_of< negq_m >
    {
        static constexpr Kind value = Kind::negq_m;
    };

    template <>
    struct kind_of< incq_m >
    {
        static constexpr Kind value = Kind::incq_m;
    };

    template <>
    struct kind_of< decq_m >
    {
        static constexpr Kind value = Kind::decq_m;
    };

    template <>
    struct kind_of< mulb_m >
    {
        static constexpr Kind value = Kind::mulb_m;
    };

    template <>
    struct kind_of< imulb_m >
    {
        static constexpr Kind value = Kind::imulb_m;
    };

    template <>
    struct kind_of< divb_m >
    {
        static constexpr Kind value = Kind::divb_m;
    };

    template <>
    struct kind_of< idivb_m >
    {
        static constexpr Kind value = Kind::idivb_m;
    };

    template <>
    struct kind_of< notb_m >
    {
        static constexpr Kind value = Kind::notb_m;
    };

    template <>
    struct kind_of< negb_m >
    {
        static constexpr Kind value = Kind::negb_m;
    };

    template <>
    struct kind_of< incb_m >
    {
        static constexpr Kind value = Kind::incb_m;
    };

    template <>
    struct kind_of< decb_m >
    {
        static constexpr Kind value = Kind::decb_m;
    };

    template <>
    struct kind_of< cbw >
    {
        static constexpr Kind value = Kind::cbw;
    };

    template <>
    struct kind_of< cwde >
    {
        static constexpr Kind value = Kind::cwde;
    };

    template <>
    struct kind_of< cdqe >
    {
        static constexpr Kind value = Kind::cdqe;
    };

    template <>
    struct kind_of< cwd >
    {
        static constexpr Kind value = Kind::cwd;
    };

    template <>
    struct kind_of< cdq >
    {
        static constexpr Kind value = Kind::cdq;
    };

    template <>
    struct kind_of< cqo >
    {
        static constexpr Kind value = Kind::cqo;
    };

    template <>
    struct kind_of< popcntw_rm >
    {
        static constexpr Kind value = Kind::popcntw_rm;
    };

    template <>
    struct kind_of< tzcntw_rm >
    {
        static constexpr Kind value = Kind::tzcntw_rm;
    };

    template <>
    struct kind_of< lzcntw_rm >
    {
        static constexpr Kind value = Kind::lzcntw_rm;
    };

    template <>
    struct kind_of< popcntl_rm >
    {
        static constexpr Kind value = Kind::popcntl_rm;
    };

    template <>
    struct kind_of< tzcntl_rm >
    {
        static constexpr Kind value = Kind::tzcntl_rm;
    };

    template <>
    struct kind_of< lzcntl_rm >
    {
        static constexpr Kind value = Kind::lzcntl_rm;
    };

    template <>
    struct kind_of< popcntq_rm >
    {
        static constexpr Kind value = Kind::popcntq_rm;
    };

    template <>
    struct kind_of< tzcntq_rm >
    {
        static constexpr Kind value = Kind::tzcntq_rm;
    };

    template <>
    struct kind_of< lzcntq_rm >
    {
        static constexpr Kind value = Kind::lzcntq_rm;
    };

    template <>
    struct kind_of< crc32l_rm8 >
    {
        static constexpr Kind value = Kind::crc32l_rm8;
    };

    template <>
    struct kind_of< crc32l_rm16 >
    {
        static constexpr Kind value = Kind::crc32l_rm16;
    };

    template <>
    struct kind_of< crc32l_rm32 >
    {
        static constexpr Kind value = Kind::crc32l_rm32;
    };

    template <>
    struct kind_of< crc32q_rm8 >
    {
        static constexpr Kind value = Kind::crc32q_rm8;
    };

    template <>
    struct kind_of< crc32q_rm64 >
    {
        static constexpr Kind value = Kind::crc32q_rm64;
    };

    template <>
    struct kind_of< rep_movsb >
    {
        static constexpr Kind value = Kind::rep_movsb;
    };

    template <>
    struct kind_of< rep_movsw >
    {
        static constexpr Kind value = Kind::rep_movsw;
    };

    template <>
    struct kind_of< rep_movsd >
    {
        static constexpr Kind value = Kind::rep_movsd;
    };

    template <>
    struct kind_of< rep_movsq >
    {
        static constexpr Kind value = Kind::rep_movsq;
    };

    template <>
    struct kind_of< rep_stosb >
    {
        static constexpr Kind value = Kind::rep_stosb;
    };

    template <>
    struct kind_of< rep_stosw >
    {
        static constexpr Kind value = Kind::rep_stosw;
    };

    template <>
    struct kind_of< rep_stosd >
    {
        static constexpr Kind value = Kind::rep_stosd;
    };

    template <>
    struct kind_of< rep_stosq >
    {
        static constexpr Kind value = Kind::rep_stosq;
    };

    template <>
    struct kind_of< repne_scasb >
    {
        static constexpr Kind value = Kind::repne_scasb;
    };

    template <>
    struct kind_of< repne_scasw >
    {
        static constexpr Kind value = Kind::repne_scasw;
    };

    template <>
    struct kind_of< repne_scasd >
    {
        static constexpr Kind value = Kind::repne_scasd;
    };

    template <>
    struct kind_of< repne_scasq >
    {
        static constexpr Kind value = Kind::repne_scasq;
    };

    template <>
    struct kind_of< rolb_m1 >
    {
        static constexpr Kind value = Kind::rolb_m1;
    };

    template <>
    struct kind_of< rolb_mc >
    {
        static constexpr Kind value = Kind::rolb_mc;
    };

    template <>
    struct kind_of< rolb_mi8 >
    {
        static constexpr Kind value = Kind::rolb_mi8;
    };

    template <>
    struct kind_of< rorb_m1 >
    {
        static constexpr Kind value = Kind::rorb_m1;
    };

    template <>
    struct kind_of< rorb_mc >
    {
        static constexpr Kind value = Kind::rorb_mc;
    };

    template <>
    struct kind_of< rorb_mi8 >
    {
        static constexpr Kind value = Kind::rorb_mi8;
    };

    template <>
    struct kind_of< rclb_m1 >
    {
        static constexpr Kind value = Kind::rclb_m1;
    };

    template <>
    struct kind_of< rclb_mc >
    {
        static constexpr Kind value = Kind::rclb_mc;
    };

    template <>
    struct kind_of< rclb_mi8 >
    {
        static constexpr Kind value = Kind::rclb_mi8;
    };

    template <>
    struct kind_of< rcrb_m1 >
    {
        static constexpr Kind value = Kind::rcrb_m1;
    };

    template <>
    struct kind_of< rcrb_mc >
    {
        static constexpr Kind value = Kind::rcrb_mc;
    };

    template <>
    struct kind_of< rcrb_mi8 >
    {
        static constexpr Kind value = Kind::rcrb_mi8;
    };

    template <>
    struct kind_of< shlb_m1 >
    {
        static constexpr Kind value = Kind::shlb_m1;
    };

    template <>
    struct kind_of< shlb_mc >
    {
        static constexpr Kind value = Kind::shlb_mc;
    };

    template <>
    struct kind_of< shlb_mi8 >
    {
        static constexpr Kind value = Kind::shlb_mi8;
    };

    template <>
    struct kind_of< shrb_m1 >
    {
        static constexpr Kind value = Kind::shrb_m1;
    };

    template <>
    struct kind_of< shrb_mc >
    {
        static constexpr Kind value = Kind::shrb_mc;
    };

    template <>
    struct kind_of< shrb_mi8 >
    {
        static constexpr Kind value = Kind::shrb_mi8;
    };

    template <>
    struct kind_of< sarb_m1 >
    {
        static constexpr Kind value = Kind::sarb_m1;
    };

    template <>
    struct kind_of< sarb_mc >
    {
        static constexpr Kind value = Kind::sarb_mc;
    };

    template <>
    struct kind_of< sarb_mi8 >
    {
        static constexpr Kind value = Kind::sarb_mi8;
    };

    template <>
    struct kind_of< rolw_m1 >
    {
        static constexpr Kind value = Kind::rolw_m1;
    };

    template <>
    struct kind_of< rolw_mc >
    {
        static constexpr Kind value = Kind::rolw_mc;
    };

    template <>
    struct kind_of< rolw_mi8 >
    {
        static constexpr Kind value = Kind::rolw_mi8;
    };

    template <>
    struct kind_of< rorw_m1 >
    {
        static constexpr Kind value = Kind::rorw_m1;
    };

    template <>
    struct kind_of< rorw_mc >
    {
        static constexpr Kind value = Kind::rorw_mc;
    };

    template <>
    struct kind_of< rorw_mi8 >
    {
        static constexpr Kind value = Kind::rorw_mi8;
    };

    template <>
    struct kind_of< rclw_m1 >
    {
        static constexpr Kind value = Kind::rclw_m1;
    };

    template <>
    struct kind_of< rclw_mc >
    {
        static constexpr Kind value = Kind::rclw_mc;
    };

    template <>
    struct kind_of< rclw_mi8 >
    {
        static constexpr Kind value = Kind::rclw_mi8;
    };

    template <>
    struct kind_of< rcrw_m1 >
    {
        static constexpr Kind value = Kind::rcrw_m1;
    };

    template <>
    struct kind_of< rcrw_mc >
    {
        static constexpr Kind value = Kind::rcrw_mc;
    };

    template <>
    struct kind_of< rcrw_mi8 >
    {
        static constexpr Kind value = Kind::rcrw_mi8;
    };

    template <>
    struct kind_of< shlw_m1 >
    {
        static constexpr Kind value = Kind::shlw_m1;
    };

    template <>
    struct kind_of< shlw_mc >
    {
        static constexpr Kind value = Kind::shlw_mc;
    };

    template <>
    struct kind_of< shlw_mi8 >
    {
        static constexpr Kind value = Kind::shlw_mi8;
    };

    template <>
    struct kind_of< shrw_m1 >
    {
        static constexpr Kind value = Kind::shrw_m1;
    };

    template <>
    struct kind_of< shrw_mc >
    {
        static constexpr Kind value = Kind::shrw_mc;
    };

    template <>
    struct kind_of< shrw_mi8 >
    {
        static constexpr Kind value = Kind::shrw_mi8;
    };

    template <>
    struct kind_of< sarw_m1 >
    {
        static constexpr Kind value = Kind::sarw_m1;
    };

    template <>
    struct kind_of< sarw_mc >
    {
        static constexpr Kind value = Kind::sarw_mc;
    };

    template <>
    struct kind_of< sarw_mi8 >
    {
        static constexpr Kind value = Kind::sarw_mi8;
    };

    template <>
    struct kind_of< roll_m1 >
    {
        static constexpr Kind value = Kind::roll_m1;
    };

    template <>
    struct kind_of< roll_mc >
    {
        static constexpr Kind value = Kind::roll_mc;
    };

    template <>
    struct kind_of< roll_mi8 >
    {
        static constexpr Kind value = Kind::roll_mi8;
    };

    template <>
    struct kind_of< rorl_m1 >
    {
        static constexpr Kind value = Kind::rorl_m1;
    };

    template <>
    struct kind_of< rorl_mc >
    {
        static constexpr Kind value = Kind::rorl_mc;
    };

    template <>
    struct kind_of< rorl_mi8 >
    {
        static constexpr Kind value = Kind::rorl_mi8;
    };

    template <>
    struct kind_of< rcll_m1 >
    {
        static constexpr Kind value = Kind::rcll_m1;
    };

    template <>
    struct kind_of< rcll_mc >
    {
        static constexpr Kind value = Kind::rcll_mc;
    };

    template <>
    struct kind_of< rcll_mi8 >
    {
        static constexpr Kind value = Kind::rcll_mi8;
    };

    template <>
    struct kind_of< rcrl_m1 >
    {
        static constexpr Kind value = Kind::rcrl_m1;
    };

    template <>
    struct kind_of< rcrl_mc >
    {
        static constexpr Kind value = Kind::rcrl_mc;
    };

    template <>
    struct kind_of< rcrl_mi8 >
    {
        static constexpr Kind value = Kind::rcrl_mi8;
    };

    template <>
    struct kind_of< shll_m1 >
    {
        static constexpr Kind value = Kind::shll_m1;
    };

    template <>
    struct kind_of< shll_mc >
    {
        static constexpr Kind value = Kind::shll_mc;
    };

    template <>
    struct kind_of< shll_mi8 >
    {
        static constexpr Kind value = Kind::shll_mi8;
    };

    template <>
    struct kind_of< shrl_m1 >
    {
        static constexpr Kind value = Kind::shrl_m1;
    };

    template <>
    struct kind_of< shrl_mc >
    {
        static constexpr Kind value = Kind::shrl_mc;
    };

    template <>
    struct kind_of< shrl_mi8 >
    {
        static constexpr Kind value = Kind::shrl_mi8;
    };

    template <>
    struct kind_of< sarl_m1 >
    {
        static constexpr Kind value = Kind::sarl_m1;
    };

    template <>
    struct kind_of< sarl_mc >
    {
        static constexpr Kind value = Kind::sarl_mc;
    };

    template <>
    struct kind_of< sarl_mi8 >
    {
        static constexpr Kind value = Kind::sarl_mi8;
    };

    template <>
    struct kind_of< rolq_m1 >
    {
        static constexpr Kind value = Kind::rolq_m1;
    };

    template <>
    struct kind_of< rolq_mc >
    {
        static constexpr Kind value = Kind::rolq_mc;
    };

    template <>
    struct kind_of< rolq_mi8 >
    {
        static constexpr Kind value = Kind::rolq_mi8;
    };

    template <>
    struct kind_of< rorq_m1 >
    {
        static constexpr Kind value = Kind::rorq_m1;
    };

    template <>
    struct kind_of< rorq_mc >
    {
        static constexpr Kind value = Kind::rorq_mc;
    };

    template <>
    struct kind_of< rorq_mi8 >
    {
        static constexpr Kind value = Kind::rorq_mi8;
    };

    template <>
    struct kind_of< rclq_m1 >
    {
        static constexpr Kind value = Kind::rclq_m1;
    };

    template <>
    struct kind_of< rclq_mc >
    {
        static constexpr Kind value = Kind::rclq_mc;
    };

    template <>
    struct kind_of< rclq_mi8 >
    {
        static constexpr Kind value = Kind::rclq_mi8;
    };

    template <>
    struct kind_of< rcrq_m1 >
    {
        static constexpr Kind value = Kind::rcrq_m1;
    };

    template <>
    struct kind_of< rcrq_mc >
    {
        static constexpr Kind value = Kind::rcrq_mc;
    };

    template <>
    struct kind_of< rcrq_mi8 >
    {
        static constexpr Kind value = Kind::rcrq_mi8;
    };

    template <>
    struct kind_of< shlq_m1 >
    {
        static constexpr Kind value = Kind::shlq_m1;
    };

    template <>
    struct kind_of< shlq_mc >
    {
        static constexpr Kind value = Kind::shlq_mc;
    };

    template <>
    struct kind_of< shlq_mi8 >
    {
        static constexpr Kind value = Kind::shlq_mi8;
    };

    template <>
    struct kind_of< shrq_m1 >
    {
        static constexpr Kind value = Kind::shrq_m1;
    };

    template <>
    struct kind_of< shrq_mc >
    {
        static constexpr Kind value = Kind::shrq_mc;
    };

    template <>
    struct kind_of< shrq_mi8 >
    {
        static constexpr Kind value = Kind::shrq_mi8;
    };

    template <>
    struct kind_of< sarq_m1 >
    {
        static constexpr Kind value = Kind::sarq_m1;
    };

    template <>
    struct kind_of< sarq_mc >
    {
        static constexpr Kind value = Kind::sarq_mc;
    };

    template <>
    struct kind_of< sarq_mi8 >
    {
        static constexpr Kind value = Kind::sarq_mi8;
    };

    template <>
    struct kind_of< MovImm >
    {
        static constexpr Kind value = Kind::MovImm;
    };

    template <>
    struct kind_of< Jmp >
    {
        static constexpr Kind value = Kind::Jmp;
    };

    template <>
    struct kind_of< Jcc >
    {
        static constexpr Kind value = Kind::Jcc;
    };

    template <>
    struct kind_of< Call >
    {
        static constexpr Kind value = Kind::Call;
    };

    template <>
    struct kind_of< CallInd >
    {
        static constexpr Kind value = Kind::CallInd;
    };

    template <>
    struct kind_of< LoadExtName >
    {
        static constexpr Kind value = Kind::LoadExtName;
    };

    template <>
    struct kind_of< StackAddr >
    {
        static constexpr Kind value = Kind::StackAddr;
    };

    template <>
    struct kind_of< DummyUse >
    {
        static constexpr Kind value = Kind::DummyUse;
    };

    template < typename T >
    concept member = requires { kind_of< std::remove_cvref_t< T > >::value; };

    template < typename T >
    inline constexpr Kind kind_v = kind_of< std::remove_cvref_t< T > >::value;

    template < template < typename... > class List >
    using all_kinds = List<
        addw_mr, addw_rm, addw_mi8, addw_mi16, addw_i16, orw_mr, orw_rm, orw_mi8, orw_mi16, orw_i16, adcw_mr, adcw_rm, adcw_mi8, adcw_mi16,
        adcw_i16, sbbw_mr, sbbw_rm, sbbw_mi8, sbbw_mi16, sbbw_i16, andw_mr, andw_rm, andw_mi8, andw_mi16, andw_i16, subw_mr, subw_rm,
        subw_mi8, subw_mi16, subw_i16, xorw_mr, xorw_rm, xorw_mi8, xorw_mi16, xorw_i16, cmpw_mr, cmpw_rm, cmpw_mi8, cmpw_mi16, cmpw_i16,
        testw_mr, testw_mi16, testw_i16, addl_mr, addl_rm, addl_mi8, addl_mi32, addl_i32, orl_mr, orl_rm, orl_mi8, orl_mi32, orl_i32,
        adcl_mr, adcl_rm, adcl_mi8, adcl_mi32, adcl_i32, sbbl_mr, sbbl_rm, sbbl_mi8, sbbl_mi32, sbbl_i32, andl_mr, andl_rm, andl_mi8,
        andl_mi32, andl_i32, subl_mr, subl_rm, subl_mi8, subl_mi32, subl_i32, xorl_mr, xorl_rm, xorl_mi8, xorl_mi32, xorl_i32, cmpl_mr,
        cmpl_rm, cmpl_mi8, cmpl_mi32, cmpl_i32, testl_mr, testl_mi32, testl_i32, addq_mr, addq_rm, addq_mi8, addq_mi32, addq_i32, orq_mr,
        orq_rm, orq_mi8, orq_mi32, orq_i32, adcq_mr, adcq_rm, adcq_mi8, adcq_mi32, adcq_i32, sbbq_mr, sbbq_rm, sbbq_mi8, sbbq_mi32,
        sbbq_i32, andq_mr, andq_rm, andq_mi8, andq_mi32, andq_i32, subq_mr, subq_rm, subq_mi8, subq_mi32, subq_i32, xorq_mr, xorq_rm,
        xorq_mi8, xorq_mi32, xorq_i32, cmpq_mr, cmpq_rm, cmpq_mi8, cmpq_mi32, cmpq_i32, testq_mr, testq_mi32, testq_i32, addb_mr, addb_rm,
        addb_mi8, addb_i8, orb_mr, orb_rm, orb_mi8, orb_i8, adcb_mr, adcb_rm, adcb_mi8, adcb_i8, sbbb_mr, sbbb_rm, sbbb_mi8, sbbb_i8,
        andb_mr, andb_rm, andb_mi8, andb_i8, subb_mr, subb_rm, subb_mi8, subb_i8, xorb_mr, xorb_rm, xorb_mi8, xorb_i8, cmpb_mr, cmpb_rm,
        cmpb_mi8, cmpb_i8, testb_mr, testb_mi8, testb_i8, btw_mr, btw_mi8, btsw_mr, btsw_mi8, btrw_mr, btrw_mi8, btcw_mr, btcw_mi8, bsfw_rm,
        bsrw_rm, btl_mr, btl_mi8, btsl_mr, btsl_mi8, btrl_mr, btrl_mi8, btcl_mr, btcl_mi8, bsfl_rm, bsrl_rm, btq_mr, btq_mi8, btsq_mr,
        btsq_mi8, btrq_mr, btrq_mi8, btcq_mr, btcq_mi8, bsfq_rm, bsrq_rm, leaw_rm, leal_rm, leaq_rm, movntil_mr, movntiq_mr, prefetchnta_m8,
        prefetcht0_m8, prefetcht1_m8, prefetcht2_m8, clflush_m, ldmxcsr_m32, stmxcsr_m32, cmpxchg8b_m64, nop, ret, leave, hlt, int3, clc,
        stc, cmc, cld, pause, ud2, syscall, cpuid, rdtsc, pushq_r, popq_r, pushq_m, popq_m, pushq_i8, pushq_i32, jmpq_m, callq_m, movw_mr,
        movw_rm, movw_mi16, xchgw_mr, xaddw_mr, cmpxchgw_mr, cmovw_rm, movzxw_rm8, movsxw_rm8, movl_mr, movl_rm, movl_mi32, xchgl_mr,
        xaddl_mr, cmpxchgl_mr, cmovl_rm, movzxl_rm8, movsxl_rm8, movq_mr, movq_rm, movq_mi32, xchgq_mr, xaddq_mr, cmpxchgq_mr, cmovq_rm,
        movzxq_rm8, movsxq_rm8, movzxl_rm16, movsxl_rm16, bswapl_r, movzxq_rm16, movsxq_rm16, bswapq_r, movsxdq_rm32, movw_ri16, movl_ri32,
        movq_ri64, movb_mr, movb_rm, movb_mi8, movb_ri8, xchgb_mr, xaddb_mr, cmpxchgb_mr, setcc_m8, imulw_rm, imulw_rmi8, imulw_rmi16,
        mulw_m, imulw_m, divw_m, idivw_m, notw_m, negw_m, incw_m, decw_m, imull_rm, imull_rmi8, imull_rmi32, mull_m, imull_m, divl_m,
        idivl_m, notl_m, negl_m, incl_m, decl_m, imulq_rm, imulq_rmi8, imulq_rmi32, mulq_m, imulq_m, divq_m, idivq_m, notq_m, negq_m,
        incq_m, decq_m, mulb_m, imulb_m, divb_m, idivb_m, notb_m, negb_m, incb_m, decb_m, cbw, cwde, cdqe, cwd, cdq, cqo, popcntw_rm,
        tzcntw_rm, lzcntw_rm, popcntl_rm, tzcntl_rm, lzcntl_rm, popcntq_rm, tzcntq_rm, lzcntq_rm, crc32l_rm8, crc32l_rm16, crc32l_rm32,
        crc32q_rm8, crc32q_rm64, rep_movsb, rep_movsw, rep_movsd, rep_movsq, rep_stosb, rep_stosw, rep_stosd, rep_stosq, repne_scasb,
        repne_scasw, repne_scasd, repne_scasq, rolb_m1, rolb_mc, rolb_mi8, rorb_m1, rorb_mc, rorb_mi8, rclb_m1, rclb_mc, rclb_mi8, rcrb_m1,
        rcrb_mc, rcrb_mi8, shlb_m1, shlb_mc, shlb_mi8, shrb_m1, shrb_mc, shrb_mi8, sarb_m1, sarb_mc, sarb_mi8, rolw_m1, rolw_mc, rolw_mi8,
        rorw_m1, rorw_mc, rorw_mi8, rclw_m1, rclw_mc, rclw_mi8, rcrw_m1, rcrw_mc, rcrw_mi8, shlw_m1, shlw_mc, shlw_mi8, shrw_m1, shrw_mc,
        shrw_mi8, sarw_m1, sarw_mc, sarw_mi8, roll_m1, roll_mc, roll_mi8, rorl_m1, rorl_mc, rorl_mi8, rcll_m1, rcll_mc, rcll_mi8, rcrl_m1,
        rcrl_mc, rcrl_mi8, shll_m1, shll_mc, shll_mi8, shrl_m1, shrl_mc, shrl_mi8, sarl_m1, sarl_mc, sarl_mi8, rolq_m1, rolq_mc, rolq_mi8,
        rorq_m1, rorq_mc, rorq_mi8, rclq_m1, rclq_mc, rclq_mi8, rcrq_m1, rcrq_mc, rcrq_mi8, shlq_m1, shlq_mc, shlq_mi8, shrq_m1, shrq_mc,
        shrq_mi8, sarq_m1, sarq_mc, sarq_mi8, MovImm, Jmp, Jcc, Call, CallInd, LoadExtName, StackAddr, DummyUse >;

    class MInst
    {
    public:
        template < typename F >
        decltype( auto ) dispatch( this auto& self, F&& f )
        {
            switch ( self.m_kind )
            {
                case Kind::addw_mr: return std::forward< F >( f )( self.template get< addw_mr >( ) );
                case Kind::addw_rm: return std::forward< F >( f )( self.template get< addw_rm >( ) );
                case Kind::addw_mi8: return std::forward< F >( f )( self.template get< addw_mi8 >( ) );
                case Kind::addw_mi16: return std::forward< F >( f )( self.template get< addw_mi16 >( ) );
                case Kind::addw_i16: return std::forward< F >( f )( self.template get< addw_i16 >( ) );
                case Kind::orw_mr: return std::forward< F >( f )( self.template get< orw_mr >( ) );
                case Kind::orw_rm: return std::forward< F >( f )( self.template get< orw_rm >( ) );
                case Kind::orw_mi8: return std::forward< F >( f )( self.template get< orw_mi8 >( ) );
                case Kind::orw_mi16: return std::forward< F >( f )( self.template get< orw_mi16 >( ) );
                case Kind::orw_i16: return std::forward< F >( f )( self.template get< orw_i16 >( ) );
                case Kind::adcw_mr: return std::forward< F >( f )( self.template get< adcw_mr >( ) );
                case Kind::adcw_rm: return std::forward< F >( f )( self.template get< adcw_rm >( ) );
                case Kind::adcw_mi8: return std::forward< F >( f )( self.template get< adcw_mi8 >( ) );
                case Kind::adcw_mi16: return std::forward< F >( f )( self.template get< adcw_mi16 >( ) );
                case Kind::adcw_i16: return std::forward< F >( f )( self.template get< adcw_i16 >( ) );
                case Kind::sbbw_mr: return std::forward< F >( f )( self.template get< sbbw_mr >( ) );
                case Kind::sbbw_rm: return std::forward< F >( f )( self.template get< sbbw_rm >( ) );
                case Kind::sbbw_mi8: return std::forward< F >( f )( self.template get< sbbw_mi8 >( ) );
                case Kind::sbbw_mi16: return std::forward< F >( f )( self.template get< sbbw_mi16 >( ) );
                case Kind::sbbw_i16: return std::forward< F >( f )( self.template get< sbbw_i16 >( ) );
                case Kind::andw_mr: return std::forward< F >( f )( self.template get< andw_mr >( ) );
                case Kind::andw_rm: return std::forward< F >( f )( self.template get< andw_rm >( ) );
                case Kind::andw_mi8: return std::forward< F >( f )( self.template get< andw_mi8 >( ) );
                case Kind::andw_mi16: return std::forward< F >( f )( self.template get< andw_mi16 >( ) );
                case Kind::andw_i16: return std::forward< F >( f )( self.template get< andw_i16 >( ) );
                case Kind::subw_mr: return std::forward< F >( f )( self.template get< subw_mr >( ) );
                case Kind::subw_rm: return std::forward< F >( f )( self.template get< subw_rm >( ) );
                case Kind::subw_mi8: return std::forward< F >( f )( self.template get< subw_mi8 >( ) );
                case Kind::subw_mi16: return std::forward< F >( f )( self.template get< subw_mi16 >( ) );
                case Kind::subw_i16: return std::forward< F >( f )( self.template get< subw_i16 >( ) );
                case Kind::xorw_mr: return std::forward< F >( f )( self.template get< xorw_mr >( ) );
                case Kind::xorw_rm: return std::forward< F >( f )( self.template get< xorw_rm >( ) );
                case Kind::xorw_mi8: return std::forward< F >( f )( self.template get< xorw_mi8 >( ) );
                case Kind::xorw_mi16: return std::forward< F >( f )( self.template get< xorw_mi16 >( ) );
                case Kind::xorw_i16: return std::forward< F >( f )( self.template get< xorw_i16 >( ) );
                case Kind::cmpw_mr: return std::forward< F >( f )( self.template get< cmpw_mr >( ) );
                case Kind::cmpw_rm: return std::forward< F >( f )( self.template get< cmpw_rm >( ) );
                case Kind::cmpw_mi8: return std::forward< F >( f )( self.template get< cmpw_mi8 >( ) );
                case Kind::cmpw_mi16: return std::forward< F >( f )( self.template get< cmpw_mi16 >( ) );
                case Kind::cmpw_i16: return std::forward< F >( f )( self.template get< cmpw_i16 >( ) );
                case Kind::testw_mr: return std::forward< F >( f )( self.template get< testw_mr >( ) );
                case Kind::testw_mi16: return std::forward< F >( f )( self.template get< testw_mi16 >( ) );
                case Kind::testw_i16: return std::forward< F >( f )( self.template get< testw_i16 >( ) );
                case Kind::addl_mr: return std::forward< F >( f )( self.template get< addl_mr >( ) );
                case Kind::addl_rm: return std::forward< F >( f )( self.template get< addl_rm >( ) );
                case Kind::addl_mi8: return std::forward< F >( f )( self.template get< addl_mi8 >( ) );
                case Kind::addl_mi32: return std::forward< F >( f )( self.template get< addl_mi32 >( ) );
                case Kind::addl_i32: return std::forward< F >( f )( self.template get< addl_i32 >( ) );
                case Kind::orl_mr: return std::forward< F >( f )( self.template get< orl_mr >( ) );
                case Kind::orl_rm: return std::forward< F >( f )( self.template get< orl_rm >( ) );
                case Kind::orl_mi8: return std::forward< F >( f )( self.template get< orl_mi8 >( ) );
                case Kind::orl_mi32: return std::forward< F >( f )( self.template get< orl_mi32 >( ) );
                case Kind::orl_i32: return std::forward< F >( f )( self.template get< orl_i32 >( ) );
                case Kind::adcl_mr: return std::forward< F >( f )( self.template get< adcl_mr >( ) );
                case Kind::adcl_rm: return std::forward< F >( f )( self.template get< adcl_rm >( ) );
                case Kind::adcl_mi8: return std::forward< F >( f )( self.template get< adcl_mi8 >( ) );
                case Kind::adcl_mi32: return std::forward< F >( f )( self.template get< adcl_mi32 >( ) );
                case Kind::adcl_i32: return std::forward< F >( f )( self.template get< adcl_i32 >( ) );
                case Kind::sbbl_mr: return std::forward< F >( f )( self.template get< sbbl_mr >( ) );
                case Kind::sbbl_rm: return std::forward< F >( f )( self.template get< sbbl_rm >( ) );
                case Kind::sbbl_mi8: return std::forward< F >( f )( self.template get< sbbl_mi8 >( ) );
                case Kind::sbbl_mi32: return std::forward< F >( f )( self.template get< sbbl_mi32 >( ) );
                case Kind::sbbl_i32: return std::forward< F >( f )( self.template get< sbbl_i32 >( ) );
                case Kind::andl_mr: return std::forward< F >( f )( self.template get< andl_mr >( ) );
                case Kind::andl_rm: return std::forward< F >( f )( self.template get< andl_rm >( ) );
                case Kind::andl_mi8: return std::forward< F >( f )( self.template get< andl_mi8 >( ) );
                case Kind::andl_mi32: return std::forward< F >( f )( self.template get< andl_mi32 >( ) );
                case Kind::andl_i32: return std::forward< F >( f )( self.template get< andl_i32 >( ) );
                case Kind::subl_mr: return std::forward< F >( f )( self.template get< subl_mr >( ) );
                case Kind::subl_rm: return std::forward< F >( f )( self.template get< subl_rm >( ) );
                case Kind::subl_mi8: return std::forward< F >( f )( self.template get< subl_mi8 >( ) );
                case Kind::subl_mi32: return std::forward< F >( f )( self.template get< subl_mi32 >( ) );
                case Kind::subl_i32: return std::forward< F >( f )( self.template get< subl_i32 >( ) );
                case Kind::xorl_mr: return std::forward< F >( f )( self.template get< xorl_mr >( ) );
                case Kind::xorl_rm: return std::forward< F >( f )( self.template get< xorl_rm >( ) );
                case Kind::xorl_mi8: return std::forward< F >( f )( self.template get< xorl_mi8 >( ) );
                case Kind::xorl_mi32: return std::forward< F >( f )( self.template get< xorl_mi32 >( ) );
                case Kind::xorl_i32: return std::forward< F >( f )( self.template get< xorl_i32 >( ) );
                case Kind::cmpl_mr: return std::forward< F >( f )( self.template get< cmpl_mr >( ) );
                case Kind::cmpl_rm: return std::forward< F >( f )( self.template get< cmpl_rm >( ) );
                case Kind::cmpl_mi8: return std::forward< F >( f )( self.template get< cmpl_mi8 >( ) );
                case Kind::cmpl_mi32: return std::forward< F >( f )( self.template get< cmpl_mi32 >( ) );
                case Kind::cmpl_i32: return std::forward< F >( f )( self.template get< cmpl_i32 >( ) );
                case Kind::testl_mr: return std::forward< F >( f )( self.template get< testl_mr >( ) );
                case Kind::testl_mi32: return std::forward< F >( f )( self.template get< testl_mi32 >( ) );
                case Kind::testl_i32: return std::forward< F >( f )( self.template get< testl_i32 >( ) );
                case Kind::addq_mr: return std::forward< F >( f )( self.template get< addq_mr >( ) );
                case Kind::addq_rm: return std::forward< F >( f )( self.template get< addq_rm >( ) );
                case Kind::addq_mi8: return std::forward< F >( f )( self.template get< addq_mi8 >( ) );
                case Kind::addq_mi32: return std::forward< F >( f )( self.template get< addq_mi32 >( ) );
                case Kind::addq_i32: return std::forward< F >( f )( self.template get< addq_i32 >( ) );
                case Kind::orq_mr: return std::forward< F >( f )( self.template get< orq_mr >( ) );
                case Kind::orq_rm: return std::forward< F >( f )( self.template get< orq_rm >( ) );
                case Kind::orq_mi8: return std::forward< F >( f )( self.template get< orq_mi8 >( ) );
                case Kind::orq_mi32: return std::forward< F >( f )( self.template get< orq_mi32 >( ) );
                case Kind::orq_i32: return std::forward< F >( f )( self.template get< orq_i32 >( ) );
                case Kind::adcq_mr: return std::forward< F >( f )( self.template get< adcq_mr >( ) );
                case Kind::adcq_rm: return std::forward< F >( f )( self.template get< adcq_rm >( ) );
                case Kind::adcq_mi8: return std::forward< F >( f )( self.template get< adcq_mi8 >( ) );
                case Kind::adcq_mi32: return std::forward< F >( f )( self.template get< adcq_mi32 >( ) );
                case Kind::adcq_i32: return std::forward< F >( f )( self.template get< adcq_i32 >( ) );
                case Kind::sbbq_mr: return std::forward< F >( f )( self.template get< sbbq_mr >( ) );
                case Kind::sbbq_rm: return std::forward< F >( f )( self.template get< sbbq_rm >( ) );
                case Kind::sbbq_mi8: return std::forward< F >( f )( self.template get< sbbq_mi8 >( ) );
                case Kind::sbbq_mi32: return std::forward< F >( f )( self.template get< sbbq_mi32 >( ) );
                case Kind::sbbq_i32: return std::forward< F >( f )( self.template get< sbbq_i32 >( ) );
                case Kind::andq_mr: return std::forward< F >( f )( self.template get< andq_mr >( ) );
                case Kind::andq_rm: return std::forward< F >( f )( self.template get< andq_rm >( ) );
                case Kind::andq_mi8: return std::forward< F >( f )( self.template get< andq_mi8 >( ) );
                case Kind::andq_mi32: return std::forward< F >( f )( self.template get< andq_mi32 >( ) );
                case Kind::andq_i32: return std::forward< F >( f )( self.template get< andq_i32 >( ) );
                case Kind::subq_mr: return std::forward< F >( f )( self.template get< subq_mr >( ) );
                case Kind::subq_rm: return std::forward< F >( f )( self.template get< subq_rm >( ) );
                case Kind::subq_mi8: return std::forward< F >( f )( self.template get< subq_mi8 >( ) );
                case Kind::subq_mi32: return std::forward< F >( f )( self.template get< subq_mi32 >( ) );
                case Kind::subq_i32: return std::forward< F >( f )( self.template get< subq_i32 >( ) );
                case Kind::xorq_mr: return std::forward< F >( f )( self.template get< xorq_mr >( ) );
                case Kind::xorq_rm: return std::forward< F >( f )( self.template get< xorq_rm >( ) );
                case Kind::xorq_mi8: return std::forward< F >( f )( self.template get< xorq_mi8 >( ) );
                case Kind::xorq_mi32: return std::forward< F >( f )( self.template get< xorq_mi32 >( ) );
                case Kind::xorq_i32: return std::forward< F >( f )( self.template get< xorq_i32 >( ) );
                case Kind::cmpq_mr: return std::forward< F >( f )( self.template get< cmpq_mr >( ) );
                case Kind::cmpq_rm: return std::forward< F >( f )( self.template get< cmpq_rm >( ) );
                case Kind::cmpq_mi8: return std::forward< F >( f )( self.template get< cmpq_mi8 >( ) );
                case Kind::cmpq_mi32: return std::forward< F >( f )( self.template get< cmpq_mi32 >( ) );
                case Kind::cmpq_i32: return std::forward< F >( f )( self.template get< cmpq_i32 >( ) );
                case Kind::testq_mr: return std::forward< F >( f )( self.template get< testq_mr >( ) );
                case Kind::testq_mi32: return std::forward< F >( f )( self.template get< testq_mi32 >( ) );
                case Kind::testq_i32: return std::forward< F >( f )( self.template get< testq_i32 >( ) );
                case Kind::addb_mr: return std::forward< F >( f )( self.template get< addb_mr >( ) );
                case Kind::addb_rm: return std::forward< F >( f )( self.template get< addb_rm >( ) );
                case Kind::addb_mi8: return std::forward< F >( f )( self.template get< addb_mi8 >( ) );
                case Kind::addb_i8: return std::forward< F >( f )( self.template get< addb_i8 >( ) );
                case Kind::orb_mr: return std::forward< F >( f )( self.template get< orb_mr >( ) );
                case Kind::orb_rm: return std::forward< F >( f )( self.template get< orb_rm >( ) );
                case Kind::orb_mi8: return std::forward< F >( f )( self.template get< orb_mi8 >( ) );
                case Kind::orb_i8: return std::forward< F >( f )( self.template get< orb_i8 >( ) );
                case Kind::adcb_mr: return std::forward< F >( f )( self.template get< adcb_mr >( ) );
                case Kind::adcb_rm: return std::forward< F >( f )( self.template get< adcb_rm >( ) );
                case Kind::adcb_mi8: return std::forward< F >( f )( self.template get< adcb_mi8 >( ) );
                case Kind::adcb_i8: return std::forward< F >( f )( self.template get< adcb_i8 >( ) );
                case Kind::sbbb_mr: return std::forward< F >( f )( self.template get< sbbb_mr >( ) );
                case Kind::sbbb_rm: return std::forward< F >( f )( self.template get< sbbb_rm >( ) );
                case Kind::sbbb_mi8: return std::forward< F >( f )( self.template get< sbbb_mi8 >( ) );
                case Kind::sbbb_i8: return std::forward< F >( f )( self.template get< sbbb_i8 >( ) );
                case Kind::andb_mr: return std::forward< F >( f )( self.template get< andb_mr >( ) );
                case Kind::andb_rm: return std::forward< F >( f )( self.template get< andb_rm >( ) );
                case Kind::andb_mi8: return std::forward< F >( f )( self.template get< andb_mi8 >( ) );
                case Kind::andb_i8: return std::forward< F >( f )( self.template get< andb_i8 >( ) );
                case Kind::subb_mr: return std::forward< F >( f )( self.template get< subb_mr >( ) );
                case Kind::subb_rm: return std::forward< F >( f )( self.template get< subb_rm >( ) );
                case Kind::subb_mi8: return std::forward< F >( f )( self.template get< subb_mi8 >( ) );
                case Kind::subb_i8: return std::forward< F >( f )( self.template get< subb_i8 >( ) );
                case Kind::xorb_mr: return std::forward< F >( f )( self.template get< xorb_mr >( ) );
                case Kind::xorb_rm: return std::forward< F >( f )( self.template get< xorb_rm >( ) );
                case Kind::xorb_mi8: return std::forward< F >( f )( self.template get< xorb_mi8 >( ) );
                case Kind::xorb_i8: return std::forward< F >( f )( self.template get< xorb_i8 >( ) );
                case Kind::cmpb_mr: return std::forward< F >( f )( self.template get< cmpb_mr >( ) );
                case Kind::cmpb_rm: return std::forward< F >( f )( self.template get< cmpb_rm >( ) );
                case Kind::cmpb_mi8: return std::forward< F >( f )( self.template get< cmpb_mi8 >( ) );
                case Kind::cmpb_i8: return std::forward< F >( f )( self.template get< cmpb_i8 >( ) );
                case Kind::testb_mr: return std::forward< F >( f )( self.template get< testb_mr >( ) );
                case Kind::testb_mi8: return std::forward< F >( f )( self.template get< testb_mi8 >( ) );
                case Kind::testb_i8: return std::forward< F >( f )( self.template get< testb_i8 >( ) );
                case Kind::btw_mr: return std::forward< F >( f )( self.template get< btw_mr >( ) );
                case Kind::btw_mi8: return std::forward< F >( f )( self.template get< btw_mi8 >( ) );
                case Kind::btsw_mr: return std::forward< F >( f )( self.template get< btsw_mr >( ) );
                case Kind::btsw_mi8: return std::forward< F >( f )( self.template get< btsw_mi8 >( ) );
                case Kind::btrw_mr: return std::forward< F >( f )( self.template get< btrw_mr >( ) );
                case Kind::btrw_mi8: return std::forward< F >( f )( self.template get< btrw_mi8 >( ) );
                case Kind::btcw_mr: return std::forward< F >( f )( self.template get< btcw_mr >( ) );
                case Kind::btcw_mi8: return std::forward< F >( f )( self.template get< btcw_mi8 >( ) );
                case Kind::bsfw_rm: return std::forward< F >( f )( self.template get< bsfw_rm >( ) );
                case Kind::bsrw_rm: return std::forward< F >( f )( self.template get< bsrw_rm >( ) );
                case Kind::btl_mr: return std::forward< F >( f )( self.template get< btl_mr >( ) );
                case Kind::btl_mi8: return std::forward< F >( f )( self.template get< btl_mi8 >( ) );
                case Kind::btsl_mr: return std::forward< F >( f )( self.template get< btsl_mr >( ) );
                case Kind::btsl_mi8: return std::forward< F >( f )( self.template get< btsl_mi8 >( ) );
                case Kind::btrl_mr: return std::forward< F >( f )( self.template get< btrl_mr >( ) );
                case Kind::btrl_mi8: return std::forward< F >( f )( self.template get< btrl_mi8 >( ) );
                case Kind::btcl_mr: return std::forward< F >( f )( self.template get< btcl_mr >( ) );
                case Kind::btcl_mi8: return std::forward< F >( f )( self.template get< btcl_mi8 >( ) );
                case Kind::bsfl_rm: return std::forward< F >( f )( self.template get< bsfl_rm >( ) );
                case Kind::bsrl_rm: return std::forward< F >( f )( self.template get< bsrl_rm >( ) );
                case Kind::btq_mr: return std::forward< F >( f )( self.template get< btq_mr >( ) );
                case Kind::btq_mi8: return std::forward< F >( f )( self.template get< btq_mi8 >( ) );
                case Kind::btsq_mr: return std::forward< F >( f )( self.template get< btsq_mr >( ) );
                case Kind::btsq_mi8: return std::forward< F >( f )( self.template get< btsq_mi8 >( ) );
                case Kind::btrq_mr: return std::forward< F >( f )( self.template get< btrq_mr >( ) );
                case Kind::btrq_mi8: return std::forward< F >( f )( self.template get< btrq_mi8 >( ) );
                case Kind::btcq_mr: return std::forward< F >( f )( self.template get< btcq_mr >( ) );
                case Kind::btcq_mi8: return std::forward< F >( f )( self.template get< btcq_mi8 >( ) );
                case Kind::bsfq_rm: return std::forward< F >( f )( self.template get< bsfq_rm >( ) );
                case Kind::bsrq_rm: return std::forward< F >( f )( self.template get< bsrq_rm >( ) );
                case Kind::leaw_rm: return std::forward< F >( f )( self.template get< leaw_rm >( ) );
                case Kind::leal_rm: return std::forward< F >( f )( self.template get< leal_rm >( ) );
                case Kind::leaq_rm: return std::forward< F >( f )( self.template get< leaq_rm >( ) );
                case Kind::movntil_mr: return std::forward< F >( f )( self.template get< movntil_mr >( ) );
                case Kind::movntiq_mr: return std::forward< F >( f )( self.template get< movntiq_mr >( ) );
                case Kind::prefetchnta_m8: return std::forward< F >( f )( self.template get< prefetchnta_m8 >( ) );
                case Kind::prefetcht0_m8: return std::forward< F >( f )( self.template get< prefetcht0_m8 >( ) );
                case Kind::prefetcht1_m8: return std::forward< F >( f )( self.template get< prefetcht1_m8 >( ) );
                case Kind::prefetcht2_m8: return std::forward< F >( f )( self.template get< prefetcht2_m8 >( ) );
                case Kind::clflush_m: return std::forward< F >( f )( self.template get< clflush_m >( ) );
                case Kind::ldmxcsr_m32: return std::forward< F >( f )( self.template get< ldmxcsr_m32 >( ) );
                case Kind::stmxcsr_m32: return std::forward< F >( f )( self.template get< stmxcsr_m32 >( ) );
                case Kind::cmpxchg8b_m64: return std::forward< F >( f )( self.template get< cmpxchg8b_m64 >( ) );
                case Kind::nop: return std::forward< F >( f )( self.template get< nop >( ) );
                case Kind::ret: return std::forward< F >( f )( self.template get< ret >( ) );
                case Kind::leave: return std::forward< F >( f )( self.template get< leave >( ) );
                case Kind::hlt: return std::forward< F >( f )( self.template get< hlt >( ) );
                case Kind::int3: return std::forward< F >( f )( self.template get< int3 >( ) );
                case Kind::clc: return std::forward< F >( f )( self.template get< clc >( ) );
                case Kind::stc: return std::forward< F >( f )( self.template get< stc >( ) );
                case Kind::cmc: return std::forward< F >( f )( self.template get< cmc >( ) );
                case Kind::cld: return std::forward< F >( f )( self.template get< cld >( ) );
                case Kind::pause: return std::forward< F >( f )( self.template get< pause >( ) );
                case Kind::ud2: return std::forward< F >( f )( self.template get< ud2 >( ) );
                case Kind::syscall: return std::forward< F >( f )( self.template get< syscall >( ) );
                case Kind::cpuid: return std::forward< F >( f )( self.template get< cpuid >( ) );
                case Kind::rdtsc: return std::forward< F >( f )( self.template get< rdtsc >( ) );
                case Kind::pushq_r: return std::forward< F >( f )( self.template get< pushq_r >( ) );
                case Kind::popq_r: return std::forward< F >( f )( self.template get< popq_r >( ) );
                case Kind::pushq_m: return std::forward< F >( f )( self.template get< pushq_m >( ) );
                case Kind::popq_m: return std::forward< F >( f )( self.template get< popq_m >( ) );
                case Kind::pushq_i8: return std::forward< F >( f )( self.template get< pushq_i8 >( ) );
                case Kind::pushq_i32: return std::forward< F >( f )( self.template get< pushq_i32 >( ) );
                case Kind::jmpq_m: return std::forward< F >( f )( self.template get< jmpq_m >( ) );
                case Kind::callq_m: return std::forward< F >( f )( self.template get< callq_m >( ) );
                case Kind::movw_mr: return std::forward< F >( f )( self.template get< movw_mr >( ) );
                case Kind::movw_rm: return std::forward< F >( f )( self.template get< movw_rm >( ) );
                case Kind::movw_mi16: return std::forward< F >( f )( self.template get< movw_mi16 >( ) );
                case Kind::xchgw_mr: return std::forward< F >( f )( self.template get< xchgw_mr >( ) );
                case Kind::xaddw_mr: return std::forward< F >( f )( self.template get< xaddw_mr >( ) );
                case Kind::cmpxchgw_mr: return std::forward< F >( f )( self.template get< cmpxchgw_mr >( ) );
                case Kind::cmovw_rm: return std::forward< F >( f )( self.template get< cmovw_rm >( ) );
                case Kind::movzxw_rm8: return std::forward< F >( f )( self.template get< movzxw_rm8 >( ) );
                case Kind::movsxw_rm8: return std::forward< F >( f )( self.template get< movsxw_rm8 >( ) );
                case Kind::movl_mr: return std::forward< F >( f )( self.template get< movl_mr >( ) );
                case Kind::movl_rm: return std::forward< F >( f )( self.template get< movl_rm >( ) );
                case Kind::movl_mi32: return std::forward< F >( f )( self.template get< movl_mi32 >( ) );
                case Kind::xchgl_mr: return std::forward< F >( f )( self.template get< xchgl_mr >( ) );
                case Kind::xaddl_mr: return std::forward< F >( f )( self.template get< xaddl_mr >( ) );
                case Kind::cmpxchgl_mr: return std::forward< F >( f )( self.template get< cmpxchgl_mr >( ) );
                case Kind::cmovl_rm: return std::forward< F >( f )( self.template get< cmovl_rm >( ) );
                case Kind::movzxl_rm8: return std::forward< F >( f )( self.template get< movzxl_rm8 >( ) );
                case Kind::movsxl_rm8: return std::forward< F >( f )( self.template get< movsxl_rm8 >( ) );
                case Kind::movq_mr: return std::forward< F >( f )( self.template get< movq_mr >( ) );
                case Kind::movq_rm: return std::forward< F >( f )( self.template get< movq_rm >( ) );
                case Kind::movq_mi32: return std::forward< F >( f )( self.template get< movq_mi32 >( ) );
                case Kind::xchgq_mr: return std::forward< F >( f )( self.template get< xchgq_mr >( ) );
                case Kind::xaddq_mr: return std::forward< F >( f )( self.template get< xaddq_mr >( ) );
                case Kind::cmpxchgq_mr: return std::forward< F >( f )( self.template get< cmpxchgq_mr >( ) );
                case Kind::cmovq_rm: return std::forward< F >( f )( self.template get< cmovq_rm >( ) );
                case Kind::movzxq_rm8: return std::forward< F >( f )( self.template get< movzxq_rm8 >( ) );
                case Kind::movsxq_rm8: return std::forward< F >( f )( self.template get< movsxq_rm8 >( ) );
                case Kind::movzxl_rm16: return std::forward< F >( f )( self.template get< movzxl_rm16 >( ) );
                case Kind::movsxl_rm16: return std::forward< F >( f )( self.template get< movsxl_rm16 >( ) );
                case Kind::bswapl_r: return std::forward< F >( f )( self.template get< bswapl_r >( ) );
                case Kind::movzxq_rm16: return std::forward< F >( f )( self.template get< movzxq_rm16 >( ) );
                case Kind::movsxq_rm16: return std::forward< F >( f )( self.template get< movsxq_rm16 >( ) );
                case Kind::bswapq_r: return std::forward< F >( f )( self.template get< bswapq_r >( ) );
                case Kind::movsxdq_rm32: return std::forward< F >( f )( self.template get< movsxdq_rm32 >( ) );
                case Kind::movw_ri16: return std::forward< F >( f )( self.template get< movw_ri16 >( ) );
                case Kind::movl_ri32: return std::forward< F >( f )( self.template get< movl_ri32 >( ) );
                case Kind::movq_ri64: return std::forward< F >( f )( self.template get< movq_ri64 >( ) );
                case Kind::movb_mr: return std::forward< F >( f )( self.template get< movb_mr >( ) );
                case Kind::movb_rm: return std::forward< F >( f )( self.template get< movb_rm >( ) );
                case Kind::movb_mi8: return std::forward< F >( f )( self.template get< movb_mi8 >( ) );
                case Kind::movb_ri8: return std::forward< F >( f )( self.template get< movb_ri8 >( ) );
                case Kind::xchgb_mr: return std::forward< F >( f )( self.template get< xchgb_mr >( ) );
                case Kind::xaddb_mr: return std::forward< F >( f )( self.template get< xaddb_mr >( ) );
                case Kind::cmpxchgb_mr: return std::forward< F >( f )( self.template get< cmpxchgb_mr >( ) );
                case Kind::setcc_m8: return std::forward< F >( f )( self.template get< setcc_m8 >( ) );
                case Kind::imulw_rm: return std::forward< F >( f )( self.template get< imulw_rm >( ) );
                case Kind::imulw_rmi8: return std::forward< F >( f )( self.template get< imulw_rmi8 >( ) );
                case Kind::imulw_rmi16: return std::forward< F >( f )( self.template get< imulw_rmi16 >( ) );
                case Kind::mulw_m: return std::forward< F >( f )( self.template get< mulw_m >( ) );
                case Kind::imulw_m: return std::forward< F >( f )( self.template get< imulw_m >( ) );
                case Kind::divw_m: return std::forward< F >( f )( self.template get< divw_m >( ) );
                case Kind::idivw_m: return std::forward< F >( f )( self.template get< idivw_m >( ) );
                case Kind::notw_m: return std::forward< F >( f )( self.template get< notw_m >( ) );
                case Kind::negw_m: return std::forward< F >( f )( self.template get< negw_m >( ) );
                case Kind::incw_m: return std::forward< F >( f )( self.template get< incw_m >( ) );
                case Kind::decw_m: return std::forward< F >( f )( self.template get< decw_m >( ) );
                case Kind::imull_rm: return std::forward< F >( f )( self.template get< imull_rm >( ) );
                case Kind::imull_rmi8: return std::forward< F >( f )( self.template get< imull_rmi8 >( ) );
                case Kind::imull_rmi32: return std::forward< F >( f )( self.template get< imull_rmi32 >( ) );
                case Kind::mull_m: return std::forward< F >( f )( self.template get< mull_m >( ) );
                case Kind::imull_m: return std::forward< F >( f )( self.template get< imull_m >( ) );
                case Kind::divl_m: return std::forward< F >( f )( self.template get< divl_m >( ) );
                case Kind::idivl_m: return std::forward< F >( f )( self.template get< idivl_m >( ) );
                case Kind::notl_m: return std::forward< F >( f )( self.template get< notl_m >( ) );
                case Kind::negl_m: return std::forward< F >( f )( self.template get< negl_m >( ) );
                case Kind::incl_m: return std::forward< F >( f )( self.template get< incl_m >( ) );
                case Kind::decl_m: return std::forward< F >( f )( self.template get< decl_m >( ) );
                case Kind::imulq_rm: return std::forward< F >( f )( self.template get< imulq_rm >( ) );
                case Kind::imulq_rmi8: return std::forward< F >( f )( self.template get< imulq_rmi8 >( ) );
                case Kind::imulq_rmi32: return std::forward< F >( f )( self.template get< imulq_rmi32 >( ) );
                case Kind::mulq_m: return std::forward< F >( f )( self.template get< mulq_m >( ) );
                case Kind::imulq_m: return std::forward< F >( f )( self.template get< imulq_m >( ) );
                case Kind::divq_m: return std::forward< F >( f )( self.template get< divq_m >( ) );
                case Kind::idivq_m: return std::forward< F >( f )( self.template get< idivq_m >( ) );
                case Kind::notq_m: return std::forward< F >( f )( self.template get< notq_m >( ) );
                case Kind::negq_m: return std::forward< F >( f )( self.template get< negq_m >( ) );
                case Kind::incq_m: return std::forward< F >( f )( self.template get< incq_m >( ) );
                case Kind::decq_m: return std::forward< F >( f )( self.template get< decq_m >( ) );
                case Kind::mulb_m: return std::forward< F >( f )( self.template get< mulb_m >( ) );
                case Kind::imulb_m: return std::forward< F >( f )( self.template get< imulb_m >( ) );
                case Kind::divb_m: return std::forward< F >( f )( self.template get< divb_m >( ) );
                case Kind::idivb_m: return std::forward< F >( f )( self.template get< idivb_m >( ) );
                case Kind::notb_m: return std::forward< F >( f )( self.template get< notb_m >( ) );
                case Kind::negb_m: return std::forward< F >( f )( self.template get< negb_m >( ) );
                case Kind::incb_m: return std::forward< F >( f )( self.template get< incb_m >( ) );
                case Kind::decb_m: return std::forward< F >( f )( self.template get< decb_m >( ) );
                case Kind::cbw: return std::forward< F >( f )( self.template get< cbw >( ) );
                case Kind::cwde: return std::forward< F >( f )( self.template get< cwde >( ) );
                case Kind::cdqe: return std::forward< F >( f )( self.template get< cdqe >( ) );
                case Kind::cwd: return std::forward< F >( f )( self.template get< cwd >( ) );
                case Kind::cdq: return std::forward< F >( f )( self.template get< cdq >( ) );
                case Kind::cqo: return std::forward< F >( f )( self.template get< cqo >( ) );
                case Kind::popcntw_rm: return std::forward< F >( f )( self.template get< popcntw_rm >( ) );
                case Kind::tzcntw_rm: return std::forward< F >( f )( self.template get< tzcntw_rm >( ) );
                case Kind::lzcntw_rm: return std::forward< F >( f )( self.template get< lzcntw_rm >( ) );
                case Kind::popcntl_rm: return std::forward< F >( f )( self.template get< popcntl_rm >( ) );
                case Kind::tzcntl_rm: return std::forward< F >( f )( self.template get< tzcntl_rm >( ) );
                case Kind::lzcntl_rm: return std::forward< F >( f )( self.template get< lzcntl_rm >( ) );
                case Kind::popcntq_rm: return std::forward< F >( f )( self.template get< popcntq_rm >( ) );
                case Kind::tzcntq_rm: return std::forward< F >( f )( self.template get< tzcntq_rm >( ) );
                case Kind::lzcntq_rm: return std::forward< F >( f )( self.template get< lzcntq_rm >( ) );
                case Kind::crc32l_rm8: return std::forward< F >( f )( self.template get< crc32l_rm8 >( ) );
                case Kind::crc32l_rm16: return std::forward< F >( f )( self.template get< crc32l_rm16 >( ) );
                case Kind::crc32l_rm32: return std::forward< F >( f )( self.template get< crc32l_rm32 >( ) );
                case Kind::crc32q_rm8: return std::forward< F >( f )( self.template get< crc32q_rm8 >( ) );
                case Kind::crc32q_rm64: return std::forward< F >( f )( self.template get< crc32q_rm64 >( ) );
                case Kind::rep_movsb: return std::forward< F >( f )( self.template get< rep_movsb >( ) );
                case Kind::rep_movsw: return std::forward< F >( f )( self.template get< rep_movsw >( ) );
                case Kind::rep_movsd: return std::forward< F >( f )( self.template get< rep_movsd >( ) );
                case Kind::rep_movsq: return std::forward< F >( f )( self.template get< rep_movsq >( ) );
                case Kind::rep_stosb: return std::forward< F >( f )( self.template get< rep_stosb >( ) );
                case Kind::rep_stosw: return std::forward< F >( f )( self.template get< rep_stosw >( ) );
                case Kind::rep_stosd: return std::forward< F >( f )( self.template get< rep_stosd >( ) );
                case Kind::rep_stosq: return std::forward< F >( f )( self.template get< rep_stosq >( ) );
                case Kind::repne_scasb: return std::forward< F >( f )( self.template get< repne_scasb >( ) );
                case Kind::repne_scasw: return std::forward< F >( f )( self.template get< repne_scasw >( ) );
                case Kind::repne_scasd: return std::forward< F >( f )( self.template get< repne_scasd >( ) );
                case Kind::repne_scasq: return std::forward< F >( f )( self.template get< repne_scasq >( ) );
                case Kind::rolb_m1: return std::forward< F >( f )( self.template get< rolb_m1 >( ) );
                case Kind::rolb_mc: return std::forward< F >( f )( self.template get< rolb_mc >( ) );
                case Kind::rolb_mi8: return std::forward< F >( f )( self.template get< rolb_mi8 >( ) );
                case Kind::rorb_m1: return std::forward< F >( f )( self.template get< rorb_m1 >( ) );
                case Kind::rorb_mc: return std::forward< F >( f )( self.template get< rorb_mc >( ) );
                case Kind::rorb_mi8: return std::forward< F >( f )( self.template get< rorb_mi8 >( ) );
                case Kind::rclb_m1: return std::forward< F >( f )( self.template get< rclb_m1 >( ) );
                case Kind::rclb_mc: return std::forward< F >( f )( self.template get< rclb_mc >( ) );
                case Kind::rclb_mi8: return std::forward< F >( f )( self.template get< rclb_mi8 >( ) );
                case Kind::rcrb_m1: return std::forward< F >( f )( self.template get< rcrb_m1 >( ) );
                case Kind::rcrb_mc: return std::forward< F >( f )( self.template get< rcrb_mc >( ) );
                case Kind::rcrb_mi8: return std::forward< F >( f )( self.template get< rcrb_mi8 >( ) );
                case Kind::shlb_m1: return std::forward< F >( f )( self.template get< shlb_m1 >( ) );
                case Kind::shlb_mc: return std::forward< F >( f )( self.template get< shlb_mc >( ) );
                case Kind::shlb_mi8: return std::forward< F >( f )( self.template get< shlb_mi8 >( ) );
                case Kind::shrb_m1: return std::forward< F >( f )( self.template get< shrb_m1 >( ) );
                case Kind::shrb_mc: return std::forward< F >( f )( self.template get< shrb_mc >( ) );
                case Kind::shrb_mi8: return std::forward< F >( f )( self.template get< shrb_mi8 >( ) );
                case Kind::sarb_m1: return std::forward< F >( f )( self.template get< sarb_m1 >( ) );
                case Kind::sarb_mc: return std::forward< F >( f )( self.template get< sarb_mc >( ) );
                case Kind::sarb_mi8: return std::forward< F >( f )( self.template get< sarb_mi8 >( ) );
                case Kind::rolw_m1: return std::forward< F >( f )( self.template get< rolw_m1 >( ) );
                case Kind::rolw_mc: return std::forward< F >( f )( self.template get< rolw_mc >( ) );
                case Kind::rolw_mi8: return std::forward< F >( f )( self.template get< rolw_mi8 >( ) );
                case Kind::rorw_m1: return std::forward< F >( f )( self.template get< rorw_m1 >( ) );
                case Kind::rorw_mc: return std::forward< F >( f )( self.template get< rorw_mc >( ) );
                case Kind::rorw_mi8: return std::forward< F >( f )( self.template get< rorw_mi8 >( ) );
                case Kind::rclw_m1: return std::forward< F >( f )( self.template get< rclw_m1 >( ) );
                case Kind::rclw_mc: return std::forward< F >( f )( self.template get< rclw_mc >( ) );
                case Kind::rclw_mi8: return std::forward< F >( f )( self.template get< rclw_mi8 >( ) );
                case Kind::rcrw_m1: return std::forward< F >( f )( self.template get< rcrw_m1 >( ) );
                case Kind::rcrw_mc: return std::forward< F >( f )( self.template get< rcrw_mc >( ) );
                case Kind::rcrw_mi8: return std::forward< F >( f )( self.template get< rcrw_mi8 >( ) );
                case Kind::shlw_m1: return std::forward< F >( f )( self.template get< shlw_m1 >( ) );
                case Kind::shlw_mc: return std::forward< F >( f )( self.template get< shlw_mc >( ) );
                case Kind::shlw_mi8: return std::forward< F >( f )( self.template get< shlw_mi8 >( ) );
                case Kind::shrw_m1: return std::forward< F >( f )( self.template get< shrw_m1 >( ) );
                case Kind::shrw_mc: return std::forward< F >( f )( self.template get< shrw_mc >( ) );
                case Kind::shrw_mi8: return std::forward< F >( f )( self.template get< shrw_mi8 >( ) );
                case Kind::sarw_m1: return std::forward< F >( f )( self.template get< sarw_m1 >( ) );
                case Kind::sarw_mc: return std::forward< F >( f )( self.template get< sarw_mc >( ) );
                case Kind::sarw_mi8: return std::forward< F >( f )( self.template get< sarw_mi8 >( ) );
                case Kind::roll_m1: return std::forward< F >( f )( self.template get< roll_m1 >( ) );
                case Kind::roll_mc: return std::forward< F >( f )( self.template get< roll_mc >( ) );
                case Kind::roll_mi8: return std::forward< F >( f )( self.template get< roll_mi8 >( ) );
                case Kind::rorl_m1: return std::forward< F >( f )( self.template get< rorl_m1 >( ) );
                case Kind::rorl_mc: return std::forward< F >( f )( self.template get< rorl_mc >( ) );
                case Kind::rorl_mi8: return std::forward< F >( f )( self.template get< rorl_mi8 >( ) );
                case Kind::rcll_m1: return std::forward< F >( f )( self.template get< rcll_m1 >( ) );
                case Kind::rcll_mc: return std::forward< F >( f )( self.template get< rcll_mc >( ) );
                case Kind::rcll_mi8: return std::forward< F >( f )( self.template get< rcll_mi8 >( ) );
                case Kind::rcrl_m1: return std::forward< F >( f )( self.template get< rcrl_m1 >( ) );
                case Kind::rcrl_mc: return std::forward< F >( f )( self.template get< rcrl_mc >( ) );
                case Kind::rcrl_mi8: return std::forward< F >( f )( self.template get< rcrl_mi8 >( ) );
                case Kind::shll_m1: return std::forward< F >( f )( self.template get< shll_m1 >( ) );
                case Kind::shll_mc: return std::forward< F >( f )( self.template get< shll_mc >( ) );
                case Kind::shll_mi8: return std::forward< F >( f )( self.template get< shll_mi8 >( ) );
                case Kind::shrl_m1: return std::forward< F >( f )( self.template get< shrl_m1 >( ) );
                case Kind::shrl_mc: return std::forward< F >( f )( self.template get< shrl_mc >( ) );
                case Kind::shrl_mi8: return std::forward< F >( f )( self.template get< shrl_mi8 >( ) );
                case Kind::sarl_m1: return std::forward< F >( f )( self.template get< sarl_m1 >( ) );
                case Kind::sarl_mc: return std::forward< F >( f )( self.template get< sarl_mc >( ) );
                case Kind::sarl_mi8: return std::forward< F >( f )( self.template get< sarl_mi8 >( ) );
                case Kind::rolq_m1: return std::forward< F >( f )( self.template get< rolq_m1 >( ) );
                case Kind::rolq_mc: return std::forward< F >( f )( self.template get< rolq_mc >( ) );
                case Kind::rolq_mi8: return std::forward< F >( f )( self.template get< rolq_mi8 >( ) );
                case Kind::rorq_m1: return std::forward< F >( f )( self.template get< rorq_m1 >( ) );
                case Kind::rorq_mc: return std::forward< F >( f )( self.template get< rorq_mc >( ) );
                case Kind::rorq_mi8: return std::forward< F >( f )( self.template get< rorq_mi8 >( ) );
                case Kind::rclq_m1: return std::forward< F >( f )( self.template get< rclq_m1 >( ) );
                case Kind::rclq_mc: return std::forward< F >( f )( self.template get< rclq_mc >( ) );
                case Kind::rclq_mi8: return std::forward< F >( f )( self.template get< rclq_mi8 >( ) );
                case Kind::rcrq_m1: return std::forward< F >( f )( self.template get< rcrq_m1 >( ) );
                case Kind::rcrq_mc: return std::forward< F >( f )( self.template get< rcrq_mc >( ) );
                case Kind::rcrq_mi8: return std::forward< F >( f )( self.template get< rcrq_mi8 >( ) );
                case Kind::shlq_m1: return std::forward< F >( f )( self.template get< shlq_m1 >( ) );
                case Kind::shlq_mc: return std::forward< F >( f )( self.template get< shlq_mc >( ) );
                case Kind::shlq_mi8: return std::forward< F >( f )( self.template get< shlq_mi8 >( ) );
                case Kind::shrq_m1: return std::forward< F >( f )( self.template get< shrq_m1 >( ) );
                case Kind::shrq_mc: return std::forward< F >( f )( self.template get< shrq_mc >( ) );
                case Kind::shrq_mi8: return std::forward< F >( f )( self.template get< shrq_mi8 >( ) );
                case Kind::sarq_m1: return std::forward< F >( f )( self.template get< sarq_m1 >( ) );
                case Kind::sarq_mc: return std::forward< F >( f )( self.template get< sarq_mc >( ) );
                case Kind::sarq_mi8: return std::forward< F >( f )( self.template get< sarq_mi8 >( ) );
                case Kind::MovImm: return std::forward< F >( f )( self.template get< MovImm >( ) );
                case Kind::Jmp: return std::forward< F >( f )( self.template get< Jmp >( ) );
                case Kind::Jcc: return std::forward< F >( f )( self.template get< Jcc >( ) );
                case Kind::Call: return std::forward< F >( f )( self.template get< Call >( ) );
                case Kind::CallInd: return std::forward< F >( f )( self.template get< CallInd >( ) );
                case Kind::LoadExtName: return std::forward< F >( f )( self.template get< LoadExtName >( ) );
                case Kind::StackAddr: return std::forward< F >( f )( self.template get< StackAddr >( ) );
                case Kind::DummyUse: return std::forward< F >( f )( self.template get< DummyUse >( ) );
            }
            std::unreachable( );
        }

        template < member T >
        MInst( T&& inst ) : m_kind { kind_v< T > }
        { std::construct_at( slot< std::remove_cvref_t< T > >( ), std::forward< T >( inst ) ); }

        MInst( const MInst& other ) : m_kind { other.m_kind }
        {
            other.dispatch(
                [ this ]< typename T >( const T& inst )
                {
                    std::construct_at( slot< T >( ), inst );
                } );
        }

        MInst( MInst&& other ) noexcept : m_kind { other.m_kind }
        {
            other.dispatch(
                [ this ]< typename T >( T& inst )
                {
                    std::construct_at( slot< T >( ), std::move( inst ) );
                } );
        }

        MInst& operator= ( const MInst& other )
        {
            if ( this == &other ) return *this;
            destroy( );
            m_kind = other.m_kind;
            other.dispatch(
                [ this ]< typename T >( const T& inst )
                {
                    std::construct_at( slot< T >( ), inst );
                } );
            return *this;
        }

        MInst& operator= ( MInst&& other ) noexcept
        {
            if ( this == &other ) return *this;
            destroy( );
            m_kind = other.m_kind;
            other.dispatch(
                [ this ]< typename T >( T& inst )
                {
                    std::construct_at( slot< T >( ), std::move( inst ) );
                } );
            return *this;
        }

        ~MInst( )
        { destroy( ); }

        [[nodiscard]] Kind kind( ) const noexcept
        { return m_kind; }

        template < member T >
        [[nodiscard]] bool is( ) const noexcept
        { return m_kind == kind_v< T >; }

        template < member T >
        [[nodiscard]] T* get_if( ) noexcept
        { return is< T >( ) ? std::launder( slot< T >( ) ) : nullptr; }

        template < member T >
        [[nodiscard]] const T* get_if( ) const noexcept
        { return is< T >( ) ? std::launder( slot< T >( ) ) : nullptr; }

        template < member T >
        [[nodiscard]] T& get( ) noexcept
        {
            assert( is< T >( ) && "[xiao::x86_64::inst::MInst] get with the wrong kind" );
            return *std::launder( slot< T >( ) );
        }

        template < member T >
        [[nodiscard]] const T& get( ) const noexcept
        {
            assert( is< T >( ) && "[xiao::x86_64::inst::MInst] get with the wrong kind" );
            return *std::launder( slot< T >( ) );
        }

    private:
        static constexpr std::size_t storage_align
            = std::max( { alignof( addw_mr ),       alignof( addw_rm ),       alignof( addw_mi8 ),      alignof( addw_mi16 ),
                          alignof( addw_i16 ),      alignof( orw_mr ),        alignof( orw_rm ),        alignof( orw_mi8 ),
                          alignof( orw_mi16 ),      alignof( orw_i16 ),       alignof( adcw_mr ),       alignof( adcw_rm ),
                          alignof( adcw_mi8 ),      alignof( adcw_mi16 ),     alignof( adcw_i16 ),      alignof( sbbw_mr ),
                          alignof( sbbw_rm ),       alignof( sbbw_mi8 ),      alignof( sbbw_mi16 ),     alignof( sbbw_i16 ),
                          alignof( andw_mr ),       alignof( andw_rm ),       alignof( andw_mi8 ),      alignof( andw_mi16 ),
                          alignof( andw_i16 ),      alignof( subw_mr ),       alignof( subw_rm ),       alignof( subw_mi8 ),
                          alignof( subw_mi16 ),     alignof( subw_i16 ),      alignof( xorw_mr ),       alignof( xorw_rm ),
                          alignof( xorw_mi8 ),      alignof( xorw_mi16 ),     alignof( xorw_i16 ),      alignof( cmpw_mr ),
                          alignof( cmpw_rm ),       alignof( cmpw_mi8 ),      alignof( cmpw_mi16 ),     alignof( cmpw_i16 ),
                          alignof( testw_mr ),      alignof( testw_mi16 ),    alignof( testw_i16 ),     alignof( addl_mr ),
                          alignof( addl_rm ),       alignof( addl_mi8 ),      alignof( addl_mi32 ),     alignof( addl_i32 ),
                          alignof( orl_mr ),        alignof( orl_rm ),        alignof( orl_mi8 ),       alignof( orl_mi32 ),
                          alignof( orl_i32 ),       alignof( adcl_mr ),       alignof( adcl_rm ),       alignof( adcl_mi8 ),
                          alignof( adcl_mi32 ),     alignof( adcl_i32 ),      alignof( sbbl_mr ),       alignof( sbbl_rm ),
                          alignof( sbbl_mi8 ),      alignof( sbbl_mi32 ),     alignof( sbbl_i32 ),      alignof( andl_mr ),
                          alignof( andl_rm ),       alignof( andl_mi8 ),      alignof( andl_mi32 ),     alignof( andl_i32 ),
                          alignof( subl_mr ),       alignof( subl_rm ),       alignof( subl_mi8 ),      alignof( subl_mi32 ),
                          alignof( subl_i32 ),      alignof( xorl_mr ),       alignof( xorl_rm ),       alignof( xorl_mi8 ),
                          alignof( xorl_mi32 ),     alignof( xorl_i32 ),      alignof( cmpl_mr ),       alignof( cmpl_rm ),
                          alignof( cmpl_mi8 ),      alignof( cmpl_mi32 ),     alignof( cmpl_i32 ),      alignof( testl_mr ),
                          alignof( testl_mi32 ),    alignof( testl_i32 ),     alignof( addq_mr ),       alignof( addq_rm ),
                          alignof( addq_mi8 ),      alignof( addq_mi32 ),     alignof( addq_i32 ),      alignof( orq_mr ),
                          alignof( orq_rm ),        alignof( orq_mi8 ),       alignof( orq_mi32 ),      alignof( orq_i32 ),
                          alignof( adcq_mr ),       alignof( adcq_rm ),       alignof( adcq_mi8 ),      alignof( adcq_mi32 ),
                          alignof( adcq_i32 ),      alignof( sbbq_mr ),       alignof( sbbq_rm ),       alignof( sbbq_mi8 ),
                          alignof( sbbq_mi32 ),     alignof( sbbq_i32 ),      alignof( andq_mr ),       alignof( andq_rm ),
                          alignof( andq_mi8 ),      alignof( andq_mi32 ),     alignof( andq_i32 ),      alignof( subq_mr ),
                          alignof( subq_rm ),       alignof( subq_mi8 ),      alignof( subq_mi32 ),     alignof( subq_i32 ),
                          alignof( xorq_mr ),       alignof( xorq_rm ),       alignof( xorq_mi8 ),      alignof( xorq_mi32 ),
                          alignof( xorq_i32 ),      alignof( cmpq_mr ),       alignof( cmpq_rm ),       alignof( cmpq_mi8 ),
                          alignof( cmpq_mi32 ),     alignof( cmpq_i32 ),      alignof( testq_mr ),      alignof( testq_mi32 ),
                          alignof( testq_i32 ),     alignof( addb_mr ),       alignof( addb_rm ),       alignof( addb_mi8 ),
                          alignof( addb_i8 ),       alignof( orb_mr ),        alignof( orb_rm ),        alignof( orb_mi8 ),
                          alignof( orb_i8 ),        alignof( adcb_mr ),       alignof( adcb_rm ),       alignof( adcb_mi8 ),
                          alignof( adcb_i8 ),       alignof( sbbb_mr ),       alignof( sbbb_rm ),       alignof( sbbb_mi8 ),
                          alignof( sbbb_i8 ),       alignof( andb_mr ),       alignof( andb_rm ),       alignof( andb_mi8 ),
                          alignof( andb_i8 ),       alignof( subb_mr ),       alignof( subb_rm ),       alignof( subb_mi8 ),
                          alignof( subb_i8 ),       alignof( xorb_mr ),       alignof( xorb_rm ),       alignof( xorb_mi8 ),
                          alignof( xorb_i8 ),       alignof( cmpb_mr ),       alignof( cmpb_rm ),       alignof( cmpb_mi8 ),
                          alignof( cmpb_i8 ),       alignof( testb_mr ),      alignof( testb_mi8 ),     alignof( testb_i8 ),
                          alignof( btw_mr ),        alignof( btw_mi8 ),       alignof( btsw_mr ),       alignof( btsw_mi8 ),
                          alignof( btrw_mr ),       alignof( btrw_mi8 ),      alignof( btcw_mr ),       alignof( btcw_mi8 ),
                          alignof( bsfw_rm ),       alignof( bsrw_rm ),       alignof( btl_mr ),        alignof( btl_mi8 ),
                          alignof( btsl_mr ),       alignof( btsl_mi8 ),      alignof( btrl_mr ),       alignof( btrl_mi8 ),
                          alignof( btcl_mr ),       alignof( btcl_mi8 ),      alignof( bsfl_rm ),       alignof( bsrl_rm ),
                          alignof( btq_mr ),        alignof( btq_mi8 ),       alignof( btsq_mr ),       alignof( btsq_mi8 ),
                          alignof( btrq_mr ),       alignof( btrq_mi8 ),      alignof( btcq_mr ),       alignof( btcq_mi8 ),
                          alignof( bsfq_rm ),       alignof( bsrq_rm ),       alignof( leaw_rm ),       alignof( leal_rm ),
                          alignof( leaq_rm ),       alignof( movntil_mr ),    alignof( movntiq_mr ),    alignof( prefetchnta_m8 ),
                          alignof( prefetcht0_m8 ), alignof( prefetcht1_m8 ), alignof( prefetcht2_m8 ), alignof( clflush_m ),
                          alignof( ldmxcsr_m32 ),   alignof( stmxcsr_m32 ),   alignof( cmpxchg8b_m64 ), alignof( nop ),
                          alignof( ret ),           alignof( leave ),         alignof( hlt ),           alignof( int3 ),
                          alignof( clc ),           alignof( stc ),           alignof( cmc ),           alignof( cld ),
                          alignof( pause ),         alignof( ud2 ),           alignof( syscall ),       alignof( cpuid ),
                          alignof( rdtsc ),         alignof( pushq_r ),       alignof( popq_r ),        alignof( pushq_m ),
                          alignof( popq_m ),        alignof( pushq_i8 ),      alignof( pushq_i32 ),     alignof( jmpq_m ),
                          alignof( callq_m ),       alignof( movw_mr ),       alignof( movw_rm ),       alignof( movw_mi16 ),
                          alignof( xchgw_mr ),      alignof( xaddw_mr ),      alignof( cmpxchgw_mr ),   alignof( cmovw_rm ),
                          alignof( movzxw_rm8 ),    alignof( movsxw_rm8 ),    alignof( movl_mr ),       alignof( movl_rm ),
                          alignof( movl_mi32 ),     alignof( xchgl_mr ),      alignof( xaddl_mr ),      alignof( cmpxchgl_mr ),
                          alignof( cmovl_rm ),      alignof( movzxl_rm8 ),    alignof( movsxl_rm8 ),    alignof( movq_mr ),
                          alignof( movq_rm ),       alignof( movq_mi32 ),     alignof( xchgq_mr ),      alignof( xaddq_mr ),
                          alignof( cmpxchgq_mr ),   alignof( cmovq_rm ),      alignof( movzxq_rm8 ),    alignof( movsxq_rm8 ),
                          alignof( movzxl_rm16 ),   alignof( movsxl_rm16 ),   alignof( bswapl_r ),      alignof( movzxq_rm16 ),
                          alignof( movsxq_rm16 ),   alignof( bswapq_r ),      alignof( movsxdq_rm32 ),  alignof( movw_ri16 ),
                          alignof( movl_ri32 ),     alignof( movq_ri64 ),     alignof( movb_mr ),       alignof( movb_rm ),
                          alignof( movb_mi8 ),      alignof( movb_ri8 ),      alignof( xchgb_mr ),      alignof( xaddb_mr ),
                          alignof( cmpxchgb_mr ),   alignof( setcc_m8 ),      alignof( imulw_rm ),      alignof( imulw_rmi8 ),
                          alignof( imulw_rmi16 ),   alignof( mulw_m ),        alignof( imulw_m ),       alignof( divw_m ),
                          alignof( idivw_m ),       alignof( notw_m ),        alignof( negw_m ),        alignof( incw_m ),
                          alignof( decw_m ),        alignof( imull_rm ),      alignof( imull_rmi8 ),    alignof( imull_rmi32 ),
                          alignof( mull_m ),        alignof( imull_m ),       alignof( divl_m ),        alignof( idivl_m ),
                          alignof( notl_m ),        alignof( negl_m ),        alignof( incl_m ),        alignof( decl_m ),
                          alignof( imulq_rm ),      alignof( imulq_rmi8 ),    alignof( imulq_rmi32 ),   alignof( mulq_m ),
                          alignof( imulq_m ),       alignof( divq_m ),        alignof( idivq_m ),       alignof( notq_m ),
                          alignof( negq_m ),        alignof( incq_m ),        alignof( decq_m ),        alignof( mulb_m ),
                          alignof( imulb_m ),       alignof( divb_m ),        alignof( idivb_m ),       alignof( notb_m ),
                          alignof( negb_m ),        alignof( incb_m ),        alignof( decb_m ),        alignof( cbw ),
                          alignof( cwde ),          alignof( cdqe ),          alignof( cwd ),           alignof( cdq ),
                          alignof( cqo ),           alignof( popcntw_rm ),    alignof( tzcntw_rm ),     alignof( lzcntw_rm ),
                          alignof( popcntl_rm ),    alignof( tzcntl_rm ),     alignof( lzcntl_rm ),     alignof( popcntq_rm ),
                          alignof( tzcntq_rm ),     alignof( lzcntq_rm ),     alignof( crc32l_rm8 ),    alignof( crc32l_rm16 ),
                          alignof( crc32l_rm32 ),   alignof( crc32q_rm8 ),    alignof( crc32q_rm64 ),   alignof( rep_movsb ),
                          alignof( rep_movsw ),     alignof( rep_movsd ),     alignof( rep_movsq ),     alignof( rep_stosb ),
                          alignof( rep_stosw ),     alignof( rep_stosd ),     alignof( rep_stosq ),     alignof( repne_scasb ),
                          alignof( repne_scasw ),   alignof( repne_scasd ),   alignof( repne_scasq ),   alignof( rolb_m1 ),
                          alignof( rolb_mc ),       alignof( rolb_mi8 ),      alignof( rorb_m1 ),       alignof( rorb_mc ),
                          alignof( rorb_mi8 ),      alignof( rclb_m1 ),       alignof( rclb_mc ),       alignof( rclb_mi8 ),
                          alignof( rcrb_m1 ),       alignof( rcrb_mc ),       alignof( rcrb_mi8 ),      alignof( shlb_m1 ),
                          alignof( shlb_mc ),       alignof( shlb_mi8 ),      alignof( shrb_m1 ),       alignof( shrb_mc ),
                          alignof( shrb_mi8 ),      alignof( sarb_m1 ),       alignof( sarb_mc ),       alignof( sarb_mi8 ),
                          alignof( rolw_m1 ),       alignof( rolw_mc ),       alignof( rolw_mi8 ),      alignof( rorw_m1 ),
                          alignof( rorw_mc ),       alignof( rorw_mi8 ),      alignof( rclw_m1 ),       alignof( rclw_mc ),
                          alignof( rclw_mi8 ),      alignof( rcrw_m1 ),       alignof( rcrw_mc ),       alignof( rcrw_mi8 ),
                          alignof( shlw_m1 ),       alignof( shlw_mc ),       alignof( shlw_mi8 ),      alignof( shrw_m1 ),
                          alignof( shrw_mc ),       alignof( shrw_mi8 ),      alignof( sarw_m1 ),       alignof( sarw_mc ),
                          alignof( sarw_mi8 ),      alignof( roll_m1 ),       alignof( roll_mc ),       alignof( roll_mi8 ),
                          alignof( rorl_m1 ),       alignof( rorl_mc ),       alignof( rorl_mi8 ),      alignof( rcll_m1 ),
                          alignof( rcll_mc ),       alignof( rcll_mi8 ),      alignof( rcrl_m1 ),       alignof( rcrl_mc ),
                          alignof( rcrl_mi8 ),      alignof( shll_m1 ),       alignof( shll_mc ),       alignof( shll_mi8 ),
                          alignof( shrl_m1 ),       alignof( shrl_mc ),       alignof( shrl_mi8 ),      alignof( sarl_m1 ),
                          alignof( sarl_mc ),       alignof( sarl_mi8 ),      alignof( rolq_m1 ),       alignof( rolq_mc ),
                          alignof( rolq_mi8 ),      alignof( rorq_m1 ),       alignof( rorq_mc ),       alignof( rorq_mi8 ),
                          alignof( rclq_m1 ),       alignof( rclq_mc ),       alignof( rclq_mi8 ),      alignof( rcrq_m1 ),
                          alignof( rcrq_mc ),       alignof( rcrq_mi8 ),      alignof( shlq_m1 ),       alignof( shlq_mc ),
                          alignof( shlq_mi8 ),      alignof( shrq_m1 ),       alignof( shrq_mc ),       alignof( shrq_mi8 ),
                          alignof( sarq_m1 ),       alignof( sarq_mc ),       alignof( sarq_mi8 ),      alignof( MovImm ),
                          alignof( Jmp ),           alignof( Jcc ),           alignof( Call ),          alignof( CallInd ),
                          alignof( LoadExtName ),   alignof( StackAddr ),     alignof( DummyUse ) } );

        static constexpr std::size_t storage_size = std::max(
            { sizeof( addw_mr ),       sizeof( addw_rm ),       sizeof( addw_mi8 ),      sizeof( addw_mi16 ),   sizeof( addw_i16 ),
              sizeof( orw_mr ),        sizeof( orw_rm ),        sizeof( orw_mi8 ),       sizeof( orw_mi16 ),    sizeof( orw_i16 ),
              sizeof( adcw_mr ),       sizeof( adcw_rm ),       sizeof( adcw_mi8 ),      sizeof( adcw_mi16 ),   sizeof( adcw_i16 ),
              sizeof( sbbw_mr ),       sizeof( sbbw_rm ),       sizeof( sbbw_mi8 ),      sizeof( sbbw_mi16 ),   sizeof( sbbw_i16 ),
              sizeof( andw_mr ),       sizeof( andw_rm ),       sizeof( andw_mi8 ),      sizeof( andw_mi16 ),   sizeof( andw_i16 ),
              sizeof( subw_mr ),       sizeof( subw_rm ),       sizeof( subw_mi8 ),      sizeof( subw_mi16 ),   sizeof( subw_i16 ),
              sizeof( xorw_mr ),       sizeof( xorw_rm ),       sizeof( xorw_mi8 ),      sizeof( xorw_mi16 ),   sizeof( xorw_i16 ),
              sizeof( cmpw_mr ),       sizeof( cmpw_rm ),       sizeof( cmpw_mi8 ),      sizeof( cmpw_mi16 ),   sizeof( cmpw_i16 ),
              sizeof( testw_mr ),      sizeof( testw_mi16 ),    sizeof( testw_i16 ),     sizeof( addl_mr ),     sizeof( addl_rm ),
              sizeof( addl_mi8 ),      sizeof( addl_mi32 ),     sizeof( addl_i32 ),      sizeof( orl_mr ),      sizeof( orl_rm ),
              sizeof( orl_mi8 ),       sizeof( orl_mi32 ),      sizeof( orl_i32 ),       sizeof( adcl_mr ),     sizeof( adcl_rm ),
              sizeof( adcl_mi8 ),      sizeof( adcl_mi32 ),     sizeof( adcl_i32 ),      sizeof( sbbl_mr ),     sizeof( sbbl_rm ),
              sizeof( sbbl_mi8 ),      sizeof( sbbl_mi32 ),     sizeof( sbbl_i32 ),      sizeof( andl_mr ),     sizeof( andl_rm ),
              sizeof( andl_mi8 ),      sizeof( andl_mi32 ),     sizeof( andl_i32 ),      sizeof( subl_mr ),     sizeof( subl_rm ),
              sizeof( subl_mi8 ),      sizeof( subl_mi32 ),     sizeof( subl_i32 ),      sizeof( xorl_mr ),     sizeof( xorl_rm ),
              sizeof( xorl_mi8 ),      sizeof( xorl_mi32 ),     sizeof( xorl_i32 ),      sizeof( cmpl_mr ),     sizeof( cmpl_rm ),
              sizeof( cmpl_mi8 ),      sizeof( cmpl_mi32 ),     sizeof( cmpl_i32 ),      sizeof( testl_mr ),    sizeof( testl_mi32 ),
              sizeof( testl_i32 ),     sizeof( addq_mr ),       sizeof( addq_rm ),       sizeof( addq_mi8 ),    sizeof( addq_mi32 ),
              sizeof( addq_i32 ),      sizeof( orq_mr ),        sizeof( orq_rm ),        sizeof( orq_mi8 ),     sizeof( orq_mi32 ),
              sizeof( orq_i32 ),       sizeof( adcq_mr ),       sizeof( adcq_rm ),       sizeof( adcq_mi8 ),    sizeof( adcq_mi32 ),
              sizeof( adcq_i32 ),      sizeof( sbbq_mr ),       sizeof( sbbq_rm ),       sizeof( sbbq_mi8 ),    sizeof( sbbq_mi32 ),
              sizeof( sbbq_i32 ),      sizeof( andq_mr ),       sizeof( andq_rm ),       sizeof( andq_mi8 ),    sizeof( andq_mi32 ),
              sizeof( andq_i32 ),      sizeof( subq_mr ),       sizeof( subq_rm ),       sizeof( subq_mi8 ),    sizeof( subq_mi32 ),
              sizeof( subq_i32 ),      sizeof( xorq_mr ),       sizeof( xorq_rm ),       sizeof( xorq_mi8 ),    sizeof( xorq_mi32 ),
              sizeof( xorq_i32 ),      sizeof( cmpq_mr ),       sizeof( cmpq_rm ),       sizeof( cmpq_mi8 ),    sizeof( cmpq_mi32 ),
              sizeof( cmpq_i32 ),      sizeof( testq_mr ),      sizeof( testq_mi32 ),    sizeof( testq_i32 ),   sizeof( addb_mr ),
              sizeof( addb_rm ),       sizeof( addb_mi8 ),      sizeof( addb_i8 ),       sizeof( orb_mr ),      sizeof( orb_rm ),
              sizeof( orb_mi8 ),       sizeof( orb_i8 ),        sizeof( adcb_mr ),       sizeof( adcb_rm ),     sizeof( adcb_mi8 ),
              sizeof( adcb_i8 ),       sizeof( sbbb_mr ),       sizeof( sbbb_rm ),       sizeof( sbbb_mi8 ),    sizeof( sbbb_i8 ),
              sizeof( andb_mr ),       sizeof( andb_rm ),       sizeof( andb_mi8 ),      sizeof( andb_i8 ),     sizeof( subb_mr ),
              sizeof( subb_rm ),       sizeof( subb_mi8 ),      sizeof( subb_i8 ),       sizeof( xorb_mr ),     sizeof( xorb_rm ),
              sizeof( xorb_mi8 ),      sizeof( xorb_i8 ),       sizeof( cmpb_mr ),       sizeof( cmpb_rm ),     sizeof( cmpb_mi8 ),
              sizeof( cmpb_i8 ),       sizeof( testb_mr ),      sizeof( testb_mi8 ),     sizeof( testb_i8 ),    sizeof( btw_mr ),
              sizeof( btw_mi8 ),       sizeof( btsw_mr ),       sizeof( btsw_mi8 ),      sizeof( btrw_mr ),     sizeof( btrw_mi8 ),
              sizeof( btcw_mr ),       sizeof( btcw_mi8 ),      sizeof( bsfw_rm ),       sizeof( bsrw_rm ),     sizeof( btl_mr ),
              sizeof( btl_mi8 ),       sizeof( btsl_mr ),       sizeof( btsl_mi8 ),      sizeof( btrl_mr ),     sizeof( btrl_mi8 ),
              sizeof( btcl_mr ),       sizeof( btcl_mi8 ),      sizeof( bsfl_rm ),       sizeof( bsrl_rm ),     sizeof( btq_mr ),
              sizeof( btq_mi8 ),       sizeof( btsq_mr ),       sizeof( btsq_mi8 ),      sizeof( btrq_mr ),     sizeof( btrq_mi8 ),
              sizeof( btcq_mr ),       sizeof( btcq_mi8 ),      sizeof( bsfq_rm ),       sizeof( bsrq_rm ),     sizeof( leaw_rm ),
              sizeof( leal_rm ),       sizeof( leaq_rm ),       sizeof( movntil_mr ),    sizeof( movntiq_mr ),  sizeof( prefetchnta_m8 ),
              sizeof( prefetcht0_m8 ), sizeof( prefetcht1_m8 ), sizeof( prefetcht2_m8 ), sizeof( clflush_m ),   sizeof( ldmxcsr_m32 ),
              sizeof( stmxcsr_m32 ),   sizeof( cmpxchg8b_m64 ), sizeof( nop ),           sizeof( ret ),         sizeof( leave ),
              sizeof( hlt ),           sizeof( int3 ),          sizeof( clc ),           sizeof( stc ),         sizeof( cmc ),
              sizeof( cld ),           sizeof( pause ),         sizeof( ud2 ),           sizeof( syscall ),     sizeof( cpuid ),
              sizeof( rdtsc ),         sizeof( pushq_r ),       sizeof( popq_r ),        sizeof( pushq_m ),     sizeof( popq_m ),
              sizeof( pushq_i8 ),      sizeof( pushq_i32 ),     sizeof( jmpq_m ),        sizeof( callq_m ),     sizeof( movw_mr ),
              sizeof( movw_rm ),       sizeof( movw_mi16 ),     sizeof( xchgw_mr ),      sizeof( xaddw_mr ),    sizeof( cmpxchgw_mr ),
              sizeof( cmovw_rm ),      sizeof( movzxw_rm8 ),    sizeof( movsxw_rm8 ),    sizeof( movl_mr ),     sizeof( movl_rm ),
              sizeof( movl_mi32 ),     sizeof( xchgl_mr ),      sizeof( xaddl_mr ),      sizeof( cmpxchgl_mr ), sizeof( cmovl_rm ),
              sizeof( movzxl_rm8 ),    sizeof( movsxl_rm8 ),    sizeof( movq_mr ),       sizeof( movq_rm ),     sizeof( movq_mi32 ),
              sizeof( xchgq_mr ),      sizeof( xaddq_mr ),      sizeof( cmpxchgq_mr ),   sizeof( cmovq_rm ),    sizeof( movzxq_rm8 ),
              sizeof( movsxq_rm8 ),    sizeof( movzxl_rm16 ),   sizeof( movsxl_rm16 ),   sizeof( bswapl_r ),    sizeof( movzxq_rm16 ),
              sizeof( movsxq_rm16 ),   sizeof( bswapq_r ),      sizeof( movsxdq_rm32 ),  sizeof( movw_ri16 ),   sizeof( movl_ri32 ),
              sizeof( movq_ri64 ),     sizeof( movb_mr ),       sizeof( movb_rm ),       sizeof( movb_mi8 ),    sizeof( movb_ri8 ),
              sizeof( xchgb_mr ),      sizeof( xaddb_mr ),      sizeof( cmpxchgb_mr ),   sizeof( setcc_m8 ),    sizeof( imulw_rm ),
              sizeof( imulw_rmi8 ),    sizeof( imulw_rmi16 ),   sizeof( mulw_m ),        sizeof( imulw_m ),     sizeof( divw_m ),
              sizeof( idivw_m ),       sizeof( notw_m ),        sizeof( negw_m ),        sizeof( incw_m ),      sizeof( decw_m ),
              sizeof( imull_rm ),      sizeof( imull_rmi8 ),    sizeof( imull_rmi32 ),   sizeof( mull_m ),      sizeof( imull_m ),
              sizeof( divl_m ),        sizeof( idivl_m ),       sizeof( notl_m ),        sizeof( negl_m ),      sizeof( incl_m ),
              sizeof( decl_m ),        sizeof( imulq_rm ),      sizeof( imulq_rmi8 ),    sizeof( imulq_rmi32 ), sizeof( mulq_m ),
              sizeof( imulq_m ),       sizeof( divq_m ),        sizeof( idivq_m ),       sizeof( notq_m ),      sizeof( negq_m ),
              sizeof( incq_m ),        sizeof( decq_m ),        sizeof( mulb_m ),        sizeof( imulb_m ),     sizeof( divb_m ),
              sizeof( idivb_m ),       sizeof( notb_m ),        sizeof( negb_m ),        sizeof( incb_m ),      sizeof( decb_m ),
              sizeof( cbw ),           sizeof( cwde ),          sizeof( cdqe ),          sizeof( cwd ),         sizeof( cdq ),
              sizeof( cqo ),           sizeof( popcntw_rm ),    sizeof( tzcntw_rm ),     sizeof( lzcntw_rm ),   sizeof( popcntl_rm ),
              sizeof( tzcntl_rm ),     sizeof( lzcntl_rm ),     sizeof( popcntq_rm ),    sizeof( tzcntq_rm ),   sizeof( lzcntq_rm ),
              sizeof( crc32l_rm8 ),    sizeof( crc32l_rm16 ),   sizeof( crc32l_rm32 ),   sizeof( crc32q_rm8 ),  sizeof( crc32q_rm64 ),
              sizeof( rep_movsb ),     sizeof( rep_movsw ),     sizeof( rep_movsd ),     sizeof( rep_movsq ),   sizeof( rep_stosb ),
              sizeof( rep_stosw ),     sizeof( rep_stosd ),     sizeof( rep_stosq ),     sizeof( repne_scasb ), sizeof( repne_scasw ),
              sizeof( repne_scasd ),   sizeof( repne_scasq ),   sizeof( rolb_m1 ),       sizeof( rolb_mc ),     sizeof( rolb_mi8 ),
              sizeof( rorb_m1 ),       sizeof( rorb_mc ),       sizeof( rorb_mi8 ),      sizeof( rclb_m1 ),     sizeof( rclb_mc ),
              sizeof( rclb_mi8 ),      sizeof( rcrb_m1 ),       sizeof( rcrb_mc ),       sizeof( rcrb_mi8 ),    sizeof( shlb_m1 ),
              sizeof( shlb_mc ),       sizeof( shlb_mi8 ),      sizeof( shrb_m1 ),       sizeof( shrb_mc ),     sizeof( shrb_mi8 ),
              sizeof( sarb_m1 ),       sizeof( sarb_mc ),       sizeof( sarb_mi8 ),      sizeof( rolw_m1 ),     sizeof( rolw_mc ),
              sizeof( rolw_mi8 ),      sizeof( rorw_m1 ),       sizeof( rorw_mc ),       sizeof( rorw_mi8 ),    sizeof( rclw_m1 ),
              sizeof( rclw_mc ),       sizeof( rclw_mi8 ),      sizeof( rcrw_m1 ),       sizeof( rcrw_mc ),     sizeof( rcrw_mi8 ),
              sizeof( shlw_m1 ),       sizeof( shlw_mc ),       sizeof( shlw_mi8 ),      sizeof( shrw_m1 ),     sizeof( shrw_mc ),
              sizeof( shrw_mi8 ),      sizeof( sarw_m1 ),       sizeof( sarw_mc ),       sizeof( sarw_mi8 ),    sizeof( roll_m1 ),
              sizeof( roll_mc ),       sizeof( roll_mi8 ),      sizeof( rorl_m1 ),       sizeof( rorl_mc ),     sizeof( rorl_mi8 ),
              sizeof( rcll_m1 ),       sizeof( rcll_mc ),       sizeof( rcll_mi8 ),      sizeof( rcrl_m1 ),     sizeof( rcrl_mc ),
              sizeof( rcrl_mi8 ),      sizeof( shll_m1 ),       sizeof( shll_mc ),       sizeof( shll_mi8 ),    sizeof( shrl_m1 ),
              sizeof( shrl_mc ),       sizeof( shrl_mi8 ),      sizeof( sarl_m1 ),       sizeof( sarl_mc ),     sizeof( sarl_mi8 ),
              sizeof( rolq_m1 ),       sizeof( rolq_mc ),       sizeof( rolq_mi8 ),      sizeof( rorq_m1 ),     sizeof( rorq_mc ),
              sizeof( rorq_mi8 ),      sizeof( rclq_m1 ),       sizeof( rclq_mc ),       sizeof( rclq_mi8 ),    sizeof( rcrq_m1 ),
              sizeof( rcrq_mc ),       sizeof( rcrq_mi8 ),      sizeof( shlq_m1 ),       sizeof( shlq_mc ),     sizeof( shlq_mi8 ),
              sizeof( shrq_m1 ),       sizeof( shrq_mc ),       sizeof( shrq_mi8 ),      sizeof( sarq_m1 ),     sizeof( sarq_mc ),
              sizeof( sarq_mi8 ),      sizeof( MovImm ),        sizeof( Jmp ),           sizeof( Jcc ),         sizeof( Call ),
              sizeof( CallInd ),       sizeof( LoadExtName ),   sizeof( StackAddr ),     sizeof( DummyUse ) } );

        template < typename T >
        [[nodiscard]] T* slot( ) noexcept
        { return reinterpret_cast< T* >( m_storage ); }

        template < typename T >
        [[nodiscard]] const T* slot( ) const noexcept
        { return reinterpret_cast< const T* >( m_storage ); }

        void destroy( ) noexcept
        {
            dispatch(
                []< typename T >( T& inst )
                {
                    std::destroy_at( &inst );
                } );
        }

        alignas( storage_align ) std::byte m_storage[ storage_size ];
        Kind m_kind;
    };
} // namespace xiao::x86_64::inst
