use crate::{buffer::Buffer, operand::RegVisitor};

pub trait MachInst: Sized {
    fn visit_regs(&mut self, v: &mut impl RegVisitor);
    fn emit(&self, buf: &mut Buffer);
}