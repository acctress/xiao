use crate::reg::{PReg, Reg};

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum OpKind { Use, Def, UseDef }

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Constraint { Any, Fixed(PReg) }

#[derive(Default, Debug)]
pub struct Collector {
    pub ops: Vec<(Reg, OpKind, Constraint)>,
    pub clobbers: Vec<PReg>
}

pub trait RegVisitor {
    fn reg(&mut self, r: &mut Reg, kind: OpKind, c: Constraint);
    fn clobber(&mut self, p: PReg);
}

impl RegVisitor for Collector {
    fn reg(&mut self, r: &mut Reg, kind: OpKind, c: Constraint) {
        self.ops.push((*r, kind, c));
    }

    fn clobber(&mut self, p: PReg) {
        self.clobbers.push(p);
    }
}