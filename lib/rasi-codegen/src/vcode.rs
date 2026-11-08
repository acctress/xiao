use std::fmt;
use crate::reg::{RegClass, VReg};
use std::ops::Range;

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct Block(pub u32);

#[derive(Debug)]
pub struct VCode<I> {
    pub insts: Vec<I>,
    pub blocks: Vec<Range<u32>>,
    pub succs: Vec<Vec<Block>>,
    pub vclass: Vec<RegClass>,
    pub block_params: Vec<Vec<VReg>>,
    pub branch_args: Vec<Vec<Vec<VReg>>>
}

impl<I> VCode<I> {
    pub fn new() -> Self {
        Self {
            insts: vec![],
            blocks: vec![],
            succs: vec![],
            vclass: vec![],
            block_params: vec![],
            branch_args: vec![]
        }
    }

    pub fn n_blocks(&self) -> usize { self.blocks.len() }

    pub fn block_params(&self, b: Block) -> &[VReg] { &self.block_params[b.0 as usize] }

    pub fn block_args(&self, b: Block, s: usize) -> &[VReg] { &self.branch_args[b.0 as usize][s] }

    pub fn block_insts(&self, b: Block) -> &[I] {
        let r = &self.blocks[b.0 as usize];
        &self.insts[r.start as usize..r.end as usize]
    }

    pub fn block_insts_mut(&mut self, b: Block) -> &mut [I] {
        let r = &self.blocks[b.0 as usize];
        &mut self.insts[r.start as usize..r.end as usize]
    }

    pub fn preds(&self) -> Vec<Vec<Block>> {
        let mut p = vec![vec![]; self.n_blocks()];
        for (b, ss) in self.succs.iter().enumerate() {
            for s in ss { p[s.0 as usize].push(Block(b as u32)); }
        }

        p
    }
}

pub struct VCodeBuilder<I> {
    vc: VCode<I>,
    open: Option<u32>,
}

impl<I> VCodeBuilder<I> {
    pub fn new() -> Self { Self { vc: VCode::new(), open: None } }

    pub fn new_vreg(&mut self, class: RegClass) -> VReg {
        self.vc.vclass.push(class);
        VReg(self.vc.vclass.len() as u32 - 1)
    }

    pub fn start_block(&mut self, params: &[VReg]) -> Block {
        assert!(self.open.is_none(), "prev block not ended");

        self.open = Some(self.vc.insts.len() as u32);
        self.vc.block_params.push(params.to_vec());
        Block(self.vc.blocks.len() as u32)
    }

    pub fn end_block(&mut self, succs: &[Block], branch_args: Vec<Vec<VReg>>) {
        let s = self.open.take().expect("mo open block");
        self.vc.blocks.push(s..self.vc.insts.len() as u32);
        self.vc.succs.push(succs.to_vec());
        self.vc.branch_args.push(branch_args);
    }

    pub fn push(&mut self, inst: I) {
        debug_assert!(self.open.is_some(), "push outside a block");
        self.vc.insts.push(inst);
    }

    pub fn finish(self) -> VCode<I> {
        assert!(self.open.is_none(), "unterminated block");
        self.vc
    }
}

impl<I> Default for VCodeBuilder<I> {
    fn default() -> Self { Self::new() } 
}

impl<I: fmt::Display> fmt::Display for VCode<I> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for (i, r) in self.blocks.iter().enumerate() {
            writeln!(f, "block{i}:")?;
            for inst in &self.insts[r.start as usize..r.end as usize] {
                writeln!(f, "    {inst}")?;
            }
        }
        Ok(())
    }
}