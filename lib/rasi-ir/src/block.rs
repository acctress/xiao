use crate::insts::{Inst, Value};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Block(pub u32);

#[derive(Debug)]
pub struct BasicBlock {
    pub params: Vec<Value>,
    pub insts: Vec<Inst>,
}

impl BasicBlock {
    pub fn new() -> Self {
        Self {
            params: vec![],
            insts: vec![]
        }
    }
}