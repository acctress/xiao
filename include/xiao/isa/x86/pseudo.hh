#pragma once

#include <xiao/codegen/machbuffer.hh>
#include <xiao/codegen/call_conv.hh>
#include <xiao/codegen/vcode.hh>
#include <xiao/codegen/reg.hh>
#include <xiao/isa/x86/asm.hh>
#include <xiao/ir/extname.hh>
#include <xiao/support/box.hh>

#include <cassert>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <vector>

namespace xiao::x86_64::inst
{
    struct MovImm
    {
        Reg     dst { };
        int64_t imm { };

        void visit( this auto& self, auto& v )
        { v.def( self.dst ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            const auto d = dst.as_preg( );

            if ( imm == 0 ) emit_rm( buf, Opcode { .bytes = { 0x31 }, .len = 1 }, d, GprMem { dst }, false );
            else if ( imm > 0 && imm <= INT64_C( 0xFFFFFFFF ) )
            {
                emit_opreg( buf, Opcode { .bytes = { 0xB8 }, .len = 1 }, d, false );
                buf.emit_u32( static_cast< uint32_t >( imm ) );
            }
            else if ( imm >= INT32_MIN && imm < 0 )
            {
                emit_rm_digit( buf, Opcode { .rex_w = true, .bytes = { 0xC7 }, .len = 1 }, 0, GprMem { dst }, false );
                buf.emit_i32( static_cast< int32_t >( imm ) );
            }
            else
            {
                emit_opreg( buf, Opcode { .rex_w = true, .bytes = { 0xB8 }, .len = 1 }, d, false );
                buf.emit_u64( static_cast< uint64_t >( imm ) );
            }
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "mov {}, {}", fmt_gpr( dst, 64 ), imm ); }
    };

    struct Jmp
    {
        BlockId target { };

        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        { emit_jmp( buf, target.id ); }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "jmp .L{}", target.id ); }
    };

    struct Jcc
    {
        CC      cc { };
        BlockId taken { };
        BlockId not_taken { };

        void visit( this auto&, auto& ) { }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            emit_jcc( buf, cc_nibble( cc ), taken.id );
            emit_jmp( buf, not_taken.id );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "j{} .L{}; jmp .L{}", x86_64::to_string( cc ), taken.id, not_taken.id ); }
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

    struct CallInfo
    {
        std::vector< PReg > args { };
        PReg                ret { };
        std::vector< PReg > clobbers { };
        CallConv            cc { };
    };

    struct Call
    {
        ir::ExternalName         name { };
        support::Box< CallInfo > info { };

        void visit( this auto& self, auto& v )
        { visit_call( self.info->args, self.info->ret, v ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            buf.emit_u8( 0xE8 );
            buf.add_reloc( codegen::RelocKind::X86CallPCRel4, name, -4 );
            buf.emit_u32( 0 );
        }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "call u{}:{}", name.ns, name.index ); }
    };

    struct CallInd
    {
        Reg                      target { };
        support::Box< CallInfo > info { };

        void visit( this auto& self, auto& v )
        {
            v.use( self.target );
            visit_call( self.info->args, self.info->ret, v );
        }

        void emit( MachBuffer< LabelUse >& buf ) const
        { emit_rm_digit( buf, Opcode { .bytes = { 0xFF }, .len = 1 }, 2, GprMem { target }, false ); }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "call *{}", fmt_gpr( target, 64 ) ); }
    };

    struct LoadExtName
    {
        Reg              dst { };
        ir::ExternalName name { };
        int64_t          offset { };
        RelocDistance    distance { RelocDistance::Far };

        void visit( this auto& self, auto& v )
        { v.def( self.dst ); }

        void emit( MachBuffer< LabelUse >& buf ) const
        {
            const auto d = dst.as_preg( );
            if ( distance == RelocDistance::Near )
            {
                emit_rex( buf, true, d, PReg::invalid( ) );
                buf.emit_u8( 0x8D );
                buf.emit_u8( modrm( 0b00, d.hw( ), 5 ) );
                buf.add_reloc( codegen::RelocKind::X86PCRel4, name, offset - 4 );
                buf.emit_u32( 0 );
                return;
            }

            emit_rex( buf, true, PReg::invalid( ), d );
            buf.emit_u8( static_cast< uint8_t >( 0xB8 + ( d.hw( ) & 7 ) ) );
            buf.add_reloc( codegen::RelocKind::Abs8, name, offset );
            buf.emit_u64( 0 );
        }

        [[nodiscard]] std::string to_string( ) const
        {
            const auto off = offset ? std::format( "{:+}", offset ) : std::string { };
            if ( distance == RelocDistance::Near )
                return std::format( "lea {}, [rip+u{}:{}{}]", fmt_gpr( dst, 64 ), name.ns, name.index, off );
            return std::format( "mov {}, u{}:{}{}", fmt_gpr( dst, 64 ), name.ns, name.index, off );
        }
    };

    struct StackAddr
    {
        Reg      dst { };
        uint32_t slot { };
        int32_t  offset { };

        void visit( this auto& self, auto& v )
        { v.def( self.dst ); }

        void emit( MachBuffer< LabelUse >& buf, const std::span< const int32_t > slot_offsets ) const
        {
            assert( slot < slot_offsets.size( ) && "[xiao::x86_64::emit] stack slot without a frame offset" );
            const auto d = dst.as_preg( );
            emit_rex_mem( buf, true, d, RSP, PReg::invalid( ) );
            buf.emit_u8( 0x8D );
            emit_mem_operand( buf, d.hw( ), RSP, PReg::invalid( ), 0, slot_offsets[ slot ] + offset );
        }

        [[nodiscard]] std::string to_string( ) const
        {
            return offset ? std::format( "lea {}, [ss{}{:+}]", fmt_gpr( dst, 64 ), slot, offset )
                          : std::format( "lea {}, [ss{}]", fmt_gpr( dst, 64 ), slot );
        }
    };

    struct DummyUse
    {
        Reg reg { };

        void visit( this auto& self, auto& v )
        { v.use( self.reg ); }

        void emit( MachBuffer< LabelUse >& ) const { }

        [[nodiscard]] std::string to_string( ) const
        { return std::format( "# keep-alive {}", fmt_gpr( reg, 64 ) ); }
    };
} // namespace xiao::x86_64::inst