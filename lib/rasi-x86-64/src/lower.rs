use rasi_ir::{function::Function, insts::{Opcode, Value}, types::Type};
use rasi_codegen::{Block, PReg, Reg, RegClass, VCode, VCodeBuilder};
use rasi_x86_64_asm::regs::Size;
use std::collections::HashMap;
use rasi_ir::insts::{ConstRef, Inst, StackSlot};
use crate::{AluOp, DivOp, RegOImm, X86Inst, RAX, RDX};
use crate::frame::FrameLayout;

pub struct LowerCtx {
    builder: VCodeBuilder<X86Inst>,
    vregs: HashMap<u32, Reg>,
}

impl LowerCtx {
    fn new() -> Self { Self { builder: VCodeBuilder::new(), vregs: HashMap::new() } }

    fn class_of(ty: Type) -> RegClass {
        if matches!(ty, Type::F32 | Type::F64) { RegClass::Float } else { RegClass::Int }
    }

    fn value(&mut self, v: Value, ty: Type) -> Reg {
        let b = &mut self.builder;
        *self.vregs.entry(v.0).or_insert_with(|| Reg::V(b.new_vreg(Self::class_of(ty))))
    }

    fn bind(&mut self, v: Value, reg: Reg) {
        self.vregs.insert(v.0, reg);
    }

    fn tmp(&mut self, ty: Type) -> Reg {
        Reg::V(self.builder.new_vreg(Self::class_of(ty)))
    }

    fn push(&mut self, inst: X86Inst) { self.builder.push(inst); }
    fn start_block(&mut self, params: &[rasi_codegen::VReg]) -> Block { self.builder.start_block(params) }
    fn end_block(&mut self, succs: &[Block], branch_args: Vec<Vec<rasi_codegen::VReg>>) { self.builder.end_block(succs, branch_args) }
    fn finish(self) -> VCode<X86Inst> { self.builder.finish() }
}

fn size_of(ty: Type) -> Size {
    match ty {
        Type::I8 => Size::S8, Type::I16 => Size::S16,
        Type::I32 => Size::S32, Type::I64 => Size::S64,
        t => panic!("unsupported type for int lowering: {t:?}"),
    }
}

fn lower_iconst(ctx: &mut LowerCtx, func: &Function, inst: &Inst, cref: ConstRef) {
    let dst = ctx.value(inst.result.unwrap(), inst.ty);
    let imm = func.constant(cref) as i32;
    ctx.push(X86Inst::MovImm { size: size_of(inst.ty), dst, imm })
}

fn lower_binop(ctx: &mut LowerCtx, inst: &Inst, op: AluOp) {
    let a = ctx.value(inst.args[0], inst.ty);
    let s = ctx.value(inst.args[1], inst.ty);
    let size = size_of(inst.ty);
    let dst = ctx.tmp(inst.ty);

    ctx.push(X86Inst::Mov { size, dst, src: a });
    ctx.push(X86Inst::Alu { op, size, dst, src: RegOImm::Reg(s) });
    ctx.bind(inst.result.unwrap(), dst);
}

fn lower_imul(ctx: &mut LowerCtx, inst: &Inst) {
    let a = ctx.value(inst.args[0], inst.ty);
    let s = ctx.value(inst.args[1], inst.ty);
    let size = size_of(inst.ty);
    let dst = ctx.tmp(inst.ty);

    ctx.push(X86Inst::Mov { size, dst, src: a });
    ctx.push(X86Inst::Imul { size, dst, src: s });
    ctx.bind(inst.result.unwrap(), dst);
}

fn lower_ret(ctx: &mut LowerCtx, func: &Function, inst: &Inst) {
    if let Some(&val) = inst.args.first() {
        let ty = func.value_type(val);
        let src = ctx.value(val, ty);
        let size = size_of(ty);

        ctx.push(X86Inst::Mov { size, dst: Reg::P(RAX), src });
    }

    ctx.push(X86Inst::Ret);
}

fn lower_divmod(ctx: &mut LowerCtx, inst: &Inst, signed: bool, result: PReg) {
    let divid = ctx.value(inst.args[0], inst.ty);
    let divis = ctx.value(inst.args[1], inst.ty);
    let size  = size_of(inst.ty);

    ctx.push(X86Inst::Mov { size, dst: Reg::P(RAX), src: divid });

    if signed {
        ctx.push(X86Inst::Cdq { size, rax: Reg::P(RAX), rdx: Reg::P(RDX) });
    } else {
        ctx.push(X86Inst::Alu { op: AluOp::Xor, size, dst: Reg::P(RDX), src: RegOImm::Reg(Reg::P(RDX)) });
    }

    let op = if signed { DivOp::Idiv } else { DivOp::Div };
    ctx.push(X86Inst::Div { op, size, rax: Reg::P(RAX), rdx: Reg::P(RDX), src: divis });

    let dst = ctx.tmp(inst.ty);
    ctx.push(X86Inst::Mov { size, dst, src: Reg::P(RAX) });
    ctx.bind(inst.result.unwrap(), dst);
}

fn lower_ext(ctx: &mut LowerCtx, func: &Function, inst: &Inst, signed: bool) {
    let src_val = inst.args[0];
    let src = ctx.value(src_val, func.value_type(src_val));
    let dsz = size_of(inst.ty);
    let ssz = size_of(func.value_type(src_val));
    let dst = ctx.tmp(inst.ty);

    let i = if signed { X86Inst::Movsx { dsz, ssz, dst, src } } else { X86Inst::Movzx { dsz, ssz, dst, src } };
    ctx.push(i);
    ctx.bind(inst.result.unwrap(), dst);
}

fn lower_trunc(ctx: &mut LowerCtx, func: &Function, inst: &Inst) {
    let src_val = inst.args[0];
    let src = ctx.value(src_val, func.value_type(src_val));
    let dst = ctx.tmp(inst.ty);

    ctx.push(X86Inst::Mov { size: size_of(inst.ty), dst, src });
    ctx.bind(inst.result.unwrap(), dst);
}

fn lower_stack_addr(ctx: &mut LowerCtx, inst: &Inst, slot: StackSlot, frame: &FrameLayout) {
    let dst = ctx.tmp(Type::I64);
    let dsp = frame.slot_offsets[slot.0 as usize];

    ctx.push(X86Inst::LeaFrame { size: Size::S64, dst, dsp });
    ctx.bind(inst.result.unwrap(), dst);
}

pub fn lower_function(func: &Function) -> (VCode<X86Inst>, FrameLayout) {
    assert_eq!(func.blocks.len(), 1, "lower_function: multiple blocks not implemented");

    let frame = FrameLayout::layout(&func.stack_slots);
    let mut ctx = LowerCtx::new();
    ctx.start_block(&[]);

    for inst in &func.block(func.entry).insts {
        match &inst.opcode {
            Opcode::Iconst(cref) => lower_iconst    (&mut ctx, func, inst, *cref),
            Opcode::Iadd         => lower_binop     (&mut ctx, inst, AluOp::Add),
            Opcode::Isub         => lower_binop     (&mut ctx, inst, AluOp::Sub),
            Opcode::And          => lower_binop     (&mut ctx, inst, AluOp::And),
            Opcode::Or           => lower_binop     (&mut ctx, inst, AluOp::Or),
            Opcode::Xor          => lower_binop     (&mut ctx, inst, AluOp::Xor),
            Opcode::Imul         => lower_imul      (&mut ctx, inst),
            Opcode::Ret          => lower_ret       (&mut ctx, func, inst),
            Opcode::Sdiv         => lower_divmod    (&mut ctx, inst, true,  RAX),
            Opcode::Udiv         => lower_divmod    (&mut ctx, inst, false, RAX),
            Opcode::Srem         => lower_divmod    (&mut ctx, inst, true,  RDX),
            Opcode::Urem         => lower_divmod    (&mut ctx, inst, false, RDX),
            Opcode::Sext         => lower_ext       (&mut ctx, func, inst, true),
            Opcode::Zext         => lower_ext       (&mut ctx, func, inst, false),
            Opcode::Trunc        => lower_trunc     (&mut ctx, func, inst),
            Opcode::StackAddr(s) => lower_stack_addr(&mut ctx, inst, *s, &frame),
            op                   => panic!("lower_function: unhandled op {op:?}"),
        }
    }

    ctx.end_block(&[], vec![]);
    (ctx.finish(), frame)
}