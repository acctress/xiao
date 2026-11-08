use std::fmt;
use rasi_x86_64_asm::{asm::X86Assembler, operand::{RegImm, Mem}, regs::{Gpr, Size}};
use rasi_codegen::{operand::{Constraint, OpKind, RegVisitor}, Block, Buffer, MachInst, PReg, Reg};
use rasi_x86_64_asm::operand::Cc;

pub const RAX: PReg = PReg::int(0);
pub const RCX: PReg = PReg::int(1);
pub const RDX: PReg = PReg::int(2);
pub const RBX: PReg = PReg::int(3);
pub const RSP: PReg = PReg::int(4);
pub const RBP: PReg = PReg::int(5);
pub const RSI: PReg = PReg::int(6);
pub const RDI: PReg = PReg::int(7);
pub const R8:  PReg = PReg::int(8);
pub const R9:  PReg = PReg::int(9);
pub const R10: PReg = PReg::int(10);
pub const R11: PReg = PReg::int(11);
pub const R12: PReg = PReg::int(12);
pub const R13: PReg = PReg::int(13);
pub const R14: PReg = PReg::int(14);
pub const R15: PReg = PReg::int(15);

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum AluOp { Add, Or, Adc, Sbb, And, Sub, Xor, Cmp }

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum DivOp { Div, Idiv }

#[derive(Clone, Copy, Debug)]
pub enum RegOImm { Reg(Reg), Imm(i32) }

#[derive(Clone, Debug)]
pub enum X86Inst {
    MovImm { size: Size, dst: Reg, imm: i32 },
    MovToMem { size: Size, dsp: i32, src: Reg },
    MovFromMem { size: Size, dst: Reg, dsp: i32 },
    Mov { size: Size, dst: Reg, src: Reg },
    Alu { op: AluOp, size: Size, dst: Reg, src: RegOImm },
    Imul { size: Size, dst: Reg, src: Reg },
    Cdq { size: Size, rax: Reg, rdx: Reg },
    Div { op: DivOp, size: Size, rax: Reg, rdx: Reg, src: Reg },
    Movzx { dsz: Size, ssz: Size, dst: Reg, src: Reg },
    Movsx { dsz: Size, ssz: Size, dst: Reg, src: Reg },
    LeaFrame { size: Size, dst: Reg, dsp: i32 },
    SetCC { cc: Cc, dst: Reg },
    Jmp { target: Block },
    Jcc { cc: Cc, target: Block },
    Ret,
}

fn gpr(r: Reg) -> Gpr { Gpr::name(r.preg().hw) }

macro_rules! reg {
    ($v:expr, $r:expr, Def)    => { $v.reg($r, OpKind::Def, Constraint::Any) };
    ($v:expr, $r:expr, Use)    => { $v.reg($r, OpKind::Use, Constraint::Any) };
    ($v:expr, $r:expr, UseDef) => { $v.reg($r, OpKind::UseDef, Constraint::Any) };
}

impl MachInst for X86Inst {
    fn visit_regs(&mut self, v: &mut impl RegVisitor) {
        match self {
            X86Inst::MovImm { dst, .. }     =>   reg!(v, dst, Def),
            X86Inst::Mov { dst, src, .. }   => { reg!(v, dst, Def); reg!(v, src, Use); },
            X86Inst::Alu { op, dst, src, .. } => {
                if *op == AluOp::Cmp { reg!(v, dst, Use); } else { reg!(v, dst, UseDef); }
                if let RegOImm::Reg(r) = src { reg!(v, r, Use); }
            }
            X86Inst::Imul { dst, src, .. } => { reg!(v, dst, UseDef); reg!(v, src, Use); }
            X86Inst::Cdq { rax, rdx, .. } => { reg!(v, rax, Use); reg!(v, rdx, Def); }
            X86Inst::Div { rax, rdx, src, .. } => { reg!(v, rax, UseDef); reg!(v, rdx, UseDef); reg!(v, src, Use); }
            X86Inst::Movzx { dst, src, .. } | X86Inst::Movsx { dst, src, .. } => { reg!(v, dst, Def); reg!(v, src, Use); },
            X86Inst::LeaFrame { dst, .. } => reg!(v, dst, Def),
            X86Inst::MovToMem { src, .. } => reg!(v, src, Use),
            X86Inst::MovFromMem { dst, .. } => reg!(v, dst, Def),
            X86Inst::SetCC { dst, .. } => reg!(v, dst, Def),
            X86Inst::Jmp { .. } | X86Inst::Jcc { .. } => {}
            X86Inst::Ret => {}
        }
    }

    fn emit(&self, buf: &mut Buffer) {
        let mut a = X86Assembler::new(buf);

        match *self {
            X86Inst::MovImm { size, dst, imm } => a.mov(size, gpr(dst), imm),
            X86Inst::Mov { size, dst, src } => a.mov(size, gpr(dst), gpr(src)),
            X86Inst::Alu { op, size, dst, src } => {
                let d = gpr(dst);
                let s = match src {
                    RegOImm::Reg(r) => RegImm::Reg(gpr(r)),
                    RegOImm::Imm(i) => RegImm::Imm(i),
                };

                match op {
                    AluOp::Add => a.add(size, d, s),
                    AluOp::Or  => a.or(size, d, s),
                    AluOp::Adc => a.adc(size, d, s),
                    AluOp::Sbb => a.sbb(size, d, s),
                    AluOp::And => a.and(size, d, s),
                    AluOp::Sub => a.sub(size, d, s),
                    AluOp::Xor => a.xor(size, d, s),
                    AluOp::Cmp => a.cmp(size, d, s),
                }
            }
            X86Inst::Imul { size, dst, src } => a.imul(size, gpr(dst), gpr(src)),
            X86Inst::Cdq { size, .. } => match size {
                Size::S64 => a.cqo(),
                Size::S32 => a.cdq(),
                s => panic!("cdq/cqo undefined for size {s:?}"),
            }
            X86Inst::Div { op, size, src, .. } => match op {
                DivOp::Div  => a.div(size, gpr(src)),
                DivOp::Idiv => a.idiv(size, gpr(src)),
            }
            X86Inst::Movzx { dsz, ssz, dst, src } => a.movzx(dsz, gpr(dst), ssz, gpr(src)),
            X86Inst::Movsx { dsz, ssz, dst, src } => a.movsx(dsz, gpr(dst), ssz, gpr(src)),
            X86Inst::LeaFrame { size, dst, dsp } => a.lea(size, gpr(dst), Mem::base(rasi_x86_64_asm::regs::RBP).disp(dsp)),
            X86Inst::MovToMem { size, dsp, src } => a.mov(size, Mem::base(rasi_x86_64_asm::regs::RBP).disp(dsp), gpr(src)),
            X86Inst::MovFromMem { size, dst, dsp } => a.mov_load(size, gpr(dst), Mem::base(rasi_x86_64_asm::regs::RBP).disp(dsp)),
            X86Inst::SetCC { cc, dst } => a.setcc(cc, gpr(dst)),
            X86Inst::Jmp { .. } => todo!(),
            X86Inst::Jcc { .. } => todo!(),
            X86Inst::Ret => { a.leave(); a.ret(); },
        }
    }
}

impl fmt::Display for RegOImm {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            RegOImm::Reg(r) => write!(f, "{r}"),
            RegOImm::Imm(i) => write!(f, "{i}"),
        }
    }
}

impl fmt::Display for X86Inst {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            X86Inst::MovImm { size, dst, imm } =>
                write!(f, "MovImm {{ size: {size:?}, dst: {dst}, imm: {imm} }}"),
            X86Inst::Mov { size, dst, src } =>
                write!(f, "Mov {{ size: {size:?}, dst: {dst}, src: {src} }}"),
            X86Inst::Alu { op, size, dst, src } =>
                write!(f, "Alu {{ op: {op:?}, size: {size:?}, dst: {dst}, src: {src} }}"),
            X86Inst::Imul { size, dst, src } =>
                write!(f, "Imul {{ size: {size:?}, dst: {dst}, src: {src} }}"),
            X86Inst::Ret => write!(f, "Ret"),
            X86Inst::Movzx { dsz, ssz, dst, src } => write!(f, "Movzx {{ dsz: {dsz:?}, ssz: {ssz:?}, dst: {dst}, src: {src} }}"),
            X86Inst::Movsx { dsz, ssz, dst, src } => write!(f, "Movsx {{ dsz: {dsz:?}, ssz: {ssz:?}, dst: {dst}, src: {src} }}"),
            X86Inst::Cdq { size, rax, rdx } => write!(f, "Cdq {{ size: {size:?}, rax: {rax}, rdx: {rdx} }}"),
            X86Inst::Div { op, size, rax, rdx, src } =>
                write!(f, "Div {{ op: {op:?}, size: {size:?}, rax: {rax}, rdx: {rdx}, src: {src} }}"),
            X86Inst::LeaFrame { size, dst, dsp } => write!(f, "LeaFrame {{ size: {size:?}, dst: {dst}, dsp: {dsp} }}"),
            X86Inst::MovToMem { size, dsp, src } => write!(f, "MovToMem {{ size: {size:?}, dsp: {dsp}, src: {src} }}"),
            X86Inst::MovFromMem { size, dst, dsp } => write!(f, "MovFromMem {{ size: {size:?}, dst: {dst}, dsp: {dsp} }}"),
            X86Inst::SetCC { cc, dst } => write!(f, "SetCC {{ cc: {cc:?}, dst: {dst} }}"),
            X86Inst::Jmp { target } => write!(f, "Jmp {{ target: {target:?} }}"),
            X86Inst::Jcc { cc, target } => write!(f, "Jcc {{ cc: {cc:?}, target: {target:?} }}"),
        }
    }
}