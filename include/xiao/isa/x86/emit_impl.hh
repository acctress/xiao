//!
//! Created by griffith on 7/11/2026.
//!

#pragma once

#include <xiao/isa/x86/inst.hh>
#include <xiao/isa/x86/encode.hh>
#include <xiao/isa/x86/reg.hh>

namespace xiao::x86_64
{
    inline void X86Inst::emit( MachBuffer< LabelUse >& buf, const std::span< const PReg > pregs ) const
    {
        uint32_t i { };
        auto     next = [ & ]( ) -> PReg
        {
            return pregs[ i++ ];
        };

        std::visit(
            [ & ]< typename T >( const T& inst )
            {
                if constexpr ( std::is_same_v< T, inst::Mov > )
                {
                    const auto dst = next( );
                    const auto src = next( );
                    emit_rex( buf, true, src, dst );
                    buf.emit_u8( 0x89 );
                    buf.emit_u8( modrm( 0b11, src.hw( ), dst.hw( ) ) );
                }
                else if constexpr ( std::is_same_v< T, inst::MovImm > )
                {
                    const auto dst = next( );
                    const auto imm = inst.imm;

                    if ( imm == 0 )
                    {
                        emit_rex( buf, false, dst, dst );
                        buf.emit_u8( 0x31 );
                        buf.emit_u8( modrm( 0b11, dst.hw( ), dst.hw( ) ) );
                    }
                    else if ( imm > 0 && imm <= INT64_C( 0xFFFFFFFF ) )
                    {
                        emit_rex( buf, false, PReg::invalid( ), dst );
                        buf.emit_u8( static_cast< uint8_t >( 0xB8 + ( dst.hw( ) & 7 ) ) );
                        buf.emit_u32( static_cast< uint32_t >( imm ) );
                    }
                    else if ( imm >= INT32_MIN && imm < 0 )
                    {
                        emit_rex( buf, true, PReg::invalid( ), dst );
                        buf.emit_u8( 0xC7 );
                        buf.emit_u8( modrm( 0b11, 0, dst.hw( ) ) );
                        buf.emit_i32( static_cast< int32_t >( imm ) );
                    }
                    else
                    {
                        emit_rex( buf, true, PReg::invalid( ), dst );
                        buf.emit_u8( static_cast< uint8_t >( 0xB8 + ( dst.hw( ) & 7 ) ) );
                        buf.emit_u64( static_cast< uint64_t >( imm ) );
                    }
                }
                else if constexpr ( std::is_same_v< T, inst::Alu > )
                {
                    const auto dst = next( );
                    const auto src = next( );

                    if ( inst.op == AluOp::Mul )
                    {
                        //! imul r64, r/m64 — 0F AF /r, operand order: dst is reg, src is r/m
                        emit_rex( buf, true, dst, src );
                        buf.emit_u8( 0x0F );
                        buf.emit_u8( 0xAF );
                        buf.emit_u8( modrm( 0b11, dst.hw( ), src.hw( ) ) );
                    }
                    else
                    {
                        uint8_t opcode { };
                        switch ( inst.op )
                        {
                            case AluOp::Add: opcode = 0x01; break;  //! add r/m64, r64
                            case AluOp::Sub: opcode = 0x29; break;  //! sub r/m64, r64
                            case AluOp::And: opcode = 0x21; break;  //! and r/m64, r64
                            case AluOp::Or: opcode = 0x09; break;   //! or  r/m64, r64
                            case AluOp::Xor: opcode = 0x31; break;  //! xor r/m64, r64
                            case AluOp::Cmp: opcode = 0x39; break;  //! cmp r/m64, r64
                            case AluOp::Test: opcode = 0x85; break; //! test r/m64, r64
                            default: std::unreachable( );
                        }

                        emit_rex( buf, true, src, dst );
                        buf.emit_u8( opcode );
                        buf.emit_u8( modrm( 0b11, src.hw( ), dst.hw( ) ) );
                    }
                }
                else if constexpr ( std::is_same_v< T, inst::AluImm > )
                {
                    const auto dst = next( );
                    uint8_t    ext { };
                    switch ( inst.op )
                    {
                        case AluOp::Add: ext = 0; break;
                        case AluOp::Or: ext = 1; break;
                        case AluOp::And: ext = 4; break;
                        case AluOp::Sub: ext = 5; break;
                        case AluOp::Xor: ext = 6; break;
                        case AluOp::Cmp: ext = 7; break;
                        default: std::unreachable( );
                    }
                    emit_rex( buf, true, PReg::invalid( ), dst );
                    buf.emit_u8( 0x81 );
                    buf.emit_u8( modrm( 0b11, ext, dst.hw( ) ) );
                    buf.emit_i32( inst.imm );
                }
                else if constexpr ( std::is_same_v< T, inst::MovLoad > )
                {
                    //! mov dst, [base + index*scale + disp]
                    const auto dst   = next( );
                    const auto base  = next( );
                    const auto index = inst.mem.index.valid( ) ? next( ) : PReg::invalid( );

                    emit_rex_mem( buf, true, dst, base, index );
                    buf.emit_u8( 0x8B );
                    emit_mem_operand( buf, dst.hw( ), base, index, inst.mem.scale, inst.mem.disp );
                }
                else if constexpr ( std::is_same_v< T, inst::MovStore > )
                {
                    const auto base  = next( );
                    const auto index = inst.mem.index.valid( ) ? next( ) : PReg::invalid( );
                    const auto src   = next( );

                    emit_rex_mem( buf, true, src, base, index );
                    buf.emit_u8( 0x89 );
                    emit_mem_operand( buf, src.hw( ), base, index, inst.mem.scale, inst.mem.disp );
                }
                else if constexpr ( std::is_same_v< T, inst::Movzx > )
                {
                    const auto dst = next( );
                    const auto src = next( );
                    emit_rex( buf, true, dst, src );
                    buf.emit_u8( 0x0F );
                    switch ( inst.size )
                    {
                        case OperandSize::S8: buf.emit_u8( 0xB6 ); break;  //! movzx r64, r/m8
                        case OperandSize::S16: buf.emit_u8( 0xB7 ); break; //! movzx r64, r/m16
                        default: std::unreachable( );
                    }
                    buf.emit_u8( modrm( 0b11, dst.hw( ), src.hw( ) ) );
                }
                else if constexpr ( std::is_same_v< T, inst::Movsx > )
                {
                    const auto dst = next( );
                    const auto src = next( );
                    emit_rex( buf, true, dst, src );
                    buf.emit_u8( 0x0F );
                    switch ( inst.size )
                    {
                        case OperandSize::S8: buf.emit_u8( 0xBE ); break;  //! movsx r64, r/m8
                        case OperandSize::S16: buf.emit_u8( 0xBF ); break; //! movsx r64, r/m16
                        default: std::unreachable( );
                    }
                    buf.emit_u8( modrm( 0b11, dst.hw( ), src.hw( ) ) );
                }
                else if constexpr ( std::is_same_v< T, inst::Cdq > )
                {
                    ++i;                 //! rax
                    ++i;                 //! rdx
                    buf.emit_u8( 0x48 ); //! rex.w
                    buf.emit_u8( 0x99 ); //! cqo
                }
                else if constexpr ( std::is_same_v< T, inst::Div > )
                {
                    const auto src = next( );
                    emit_rex( buf, true, PReg::invalid( ), src );
                    buf.emit_u8( 0xF7 );
                    buf.emit_u8( modrm( 0b11, 6, src.hw( ) ) );
                }
                else if constexpr ( std::is_same_v< T, inst::Idiv > )
                {
                    const auto src = next( );
                    ++i; //! rax
                    ++i; //! rdx
                    emit_rex( buf, true, PReg::invalid( ), src );
                    buf.emit_u8( 0xF7 );
                    buf.emit_u8( modrm( 0b11, 7, src.hw( ) ) );
                }
                else if constexpr ( std::is_same_v< T, inst::Push > )
                {
                    const auto src = next( );
                    if ( src.hw( ) >= 8 ) buf.emit_u8( 0x41 ); //! rex.b
                    buf.emit_u8( static_cast< uint8_t >( 0x50 + ( src.hw( ) & 7 ) ) );
                }
                else if constexpr ( std::is_same_v< T, inst::Pop > )
                {
                    const auto dst = next( );
                    if ( dst.hw( ) >= 8 ) buf.emit_u8( 0x41 ); //! rex.b
                    buf.emit_u8( static_cast< uint8_t >( 0x58 + ( dst.hw( ) & 7 ) ) );
                }
                else if constexpr ( std::is_same_v< T, inst::LeaFrame > )
                {
                    //! lea dst, [rbp + offset]
                    const auto dst = next( );
                    emit_rex( buf, true, dst, RBP );
                    buf.emit_u8( 0x8D );
                    if ( inst.offset >= -128 && inst.offset <= 127 )
                    {
                        buf.emit_u8( modrm( 0b01, dst.hw( ), 5 ) );
                        buf.emit_u8( static_cast< uint8_t >( inst.offset ) );
                    }
                    else
                    {
                        buf.emit_u8( modrm( 0b10, dst.hw( ), 5 ) );
                        buf.emit_i32( inst.offset );
                    }
                }
                else if constexpr ( std::is_same_v< T, inst::SetCC > )
                {
                    const auto dst = next( );
                    uint8_t    cc_code { };
                    switch ( inst.cc )
                    {
                        case CC::E: cc_code = 0x94; break;
                        case CC::NE: cc_code = 0x95; break;
                        case CC::B: cc_code = 0x92; break;
                        case CC::BE: cc_code = 0x96; break;
                        case CC::A: cc_code = 0x97; break;
                        case CC::AE: cc_code = 0x93; break;
                        case CC::L: cc_code = 0x9C; break;
                        case CC::LE: cc_code = 0x9E; break;
                        case CC::G: cc_code = 0x9F; break;
                        case CC::GE: cc_code = 0x9D; break;
                    }
                    if ( dst.hw( ) >= 8 ) buf.emit_u8( 0x41 );
                    else if ( dst.hw( ) >= 4 )
                        buf.emit_u8( 0x40 );
                    buf.emit_u8( 0x0F );
                    buf.emit_u8( cc_code );
                    buf.emit_u8( modrm( 0b11, 0, dst.hw( ) ) );
                }
                else if constexpr ( std::is_same_v< T, inst::Jmp > )
                {
                    buf.emit_u8( 0xE9 );
                    buf.emit_label_ref( inst.target.id, LabelUse { LabelUse::Kind::JmpRel32 } );
                }
                else if constexpr ( std::is_same_v< T, inst::Jcc > )
                {
                    uint8_t cc_code { };
                    switch ( inst.cc )
                    {
                        case CC::E: cc_code = 0x84; break;
                        case CC::NE: cc_code = 0x85; break;
                        case CC::B: cc_code = 0x82; break;
                        case CC::BE: cc_code = 0x86; break;
                        case CC::A: cc_code = 0x87; break;
                        case CC::AE: cc_code = 0x83; break;
                        case CC::L: cc_code = 0x8C; break;
                        case CC::LE: cc_code = 0x8E; break;
                        case CC::G: cc_code = 0x8F; break;
                        case CC::GE: cc_code = 0x8D; break;
                    }
                    buf.emit_u8( 0x0F );
                    buf.emit_u8( cc_code );
                    buf.emit_label_ref( inst.target.id, LabelUse { LabelUse::Kind::JmpRel32 } );
                }
                else if constexpr ( std::is_same_v< T, inst::Call > )
                {
                    for ( std::size_t a = 0; a < inst.args.size( ); ++a ) ( void ) next( );
                    if ( inst.ret.valid( ) ) ( void ) next( );

                    buf.emit_u8( 0xE8 );
                    buf.add_reloc( codegen::RelocKind::X86CallPCRel4, inst.name, -4 );
                    buf.emit_u32( 0 );
                }
                else if constexpr ( std::is_same_v< T, inst::LoadExtName > )
                {
                    const auto dst = next( );
                    emit_rex( buf, true, PReg::invalid( ), dst );
                    buf.emit_u8( static_cast< uint8_t >( 0xB8 + ( dst.hw( ) & 7 ) ) );
                    buf.add_reloc( codegen::RelocKind::Abs8, inst.name, 0 );
                    buf.emit_u64( 0 );
                }
                else if constexpr ( std::is_same_v< T, inst::CallInd > )
                {
                    const auto target = next( );
                    for ( std::size_t a = 0; a < inst.args.size( ); ++a ) ( void ) next( );
                    if ( inst.ret.valid( ) ) ( void ) next( );

                    if ( target.hw( ) >= 8 ) buf.emit_u8( 0x41 );
                    buf.emit_u8( 0xFF );
                    buf.emit_u8( modrm( 0b11, 2, target.hw( ) ) );
                }
                else if constexpr ( std::is_same_v< T, inst::Ret > ) { buf.emit_u8( 0xC3 ); }
                else if constexpr ( std::is_same_v< T, inst::DummyUse > ) { }
            },
            inner );

        assert( i == pregs.size( ) && "operand count mismatch between get_operands and emit" );
    }
} // namespace xiao::x86_64
