use std::collections::{HashMap, HashSet};
use std::ops::Range;
use rasi_codegen::{machinst::MachInst, Block, Collector, OpKind, PReg, Reg, RegClass, VCode};
use crate::liveness::Liveness;

pub type Point = u32;

#[derive(Debug)]
pub struct LiveInterval {
    pub vreg: u32,
    pub class: RegClass,
    pub ranges: Vec<Range<Point>>,
    pub fixed: Option<PReg>
}

#[derive(Debug)]
pub struct LiveIntervals {
    pub intervals: Vec<LiveInterval>
}

impl LiveInterval {
    pub fn start(&self) -> Point {
        self.ranges.iter().map(|r| r.start).min().unwrap()
    }

    pub fn end(&self) -> Point {
        self.ranges.iter().map(|r| r.end).max().unwrap()
    }
}

impl LiveIntervals {
    pub fn build<I: MachInst + Clone>(vc: &VCode<I>, liveness: &Liveness) -> Self {
        let mut ranges: HashMap<u32, Vec<Range<Point>>> = HashMap::new();
        let mut fix_intervals: Vec<LiveInterval> = Vec::new();

        for b in 0..vc.n_blocks() {
            let block = Block(b as u32);
            let range = &vc.blocks[b];

            let mut first_def: HashMap<u32, Point> = HashMap::new();
            let mut last_use: HashMap<u32, Point> = HashMap::new();
            let mut fix_ranges: Vec<(PReg, Range<Point>)> = Vec::new();

            for (idx, mut inst) in vc.block_insts(block).iter().cloned().enumerate() {
                let abs: Point = range.start + idx as u32;
                let mut c = Collector::default();

                inst.visit_regs(&mut c);
                for (reg, kind, _const) in &c.ops {
                    let is_use = matches!(kind, OpKind::Use | OpKind::UseDef);
                    let is_def = matches!(kind, OpKind::Def | OpKind::UseDef);

                    match *reg {
                        Reg::V(vr) => {
                            if is_use { last_use.insert(vr.0, abs);           }
                            if is_def { first_def.entry(vr.0).or_insert(abs); }
                        }

                        Reg::P(pr) => {
                            match fix_ranges.iter_mut().find(|(p, _)| p.hw == pr.hw && p.class == pr.class) {
                                Some((_, r)) => { r.start = r.start.min(abs); r.end = r.end.max(abs + 1); }
                                None         =>   fix_ranges.push((pr, abs..abs + 1))
                            }
                        }
                    }
                }
            }

            let live_in = &liveness.blocks[b].live_in;
            let live_out = &liveness.blocks[b].live_out;

            let mut tch: HashSet<u32> = HashSet::new();
            tch.extend(first_def.keys());
            tch.extend(last_use.keys());
            tch.extend(live_in.iter());
            tch.extend(live_out.iter());

            for &vr in &tch {
                let start = if live_in.contains(&vr)  { range.start } else { first_def[&vr]    };
                let end   = if live_out.contains(&vr) { range.end   } else { last_use[&vr] + 1 };

                ranges.entry(vr).or_default().push(start..end);
            }

            for (pr, r) in fix_ranges {
                fix_intervals.push(LiveInterval { vreg: u32::MAX, class: pr.class, ranges: vec![r], fixed: Some(pr) });
            }
        }

        let mut intervals: Vec<LiveInterval> = ranges.into_iter()
            .map(|(vreg, ranges)| LiveInterval { vreg, class: vc.vclass[vreg as usize], ranges, fixed: None })
            .collect();

        intervals.extend(fix_intervals);
        LiveIntervals { intervals }
    }
}