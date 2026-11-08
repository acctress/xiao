use std::collections::HashMap;
use rasi_codegen::{AbiSpec, Constraint, MachInst, OpKind, PReg, Reg, RegVisitor, VCode};
use rasi_regalloc::interval::LiveIntervals;
use rasi_regalloc::linear::LinearScan;
use rasi_regalloc::liveness::Liveness;
use rasi_x86_64_asm::regs::Size;
use crate::frame::FrameLayout;
use crate::X86Inst;

struct SpillRw<'a> {
    assignments: &'a HashMap<u32, PReg>,
    spill_slots: &'a HashMap<u32, usize>,
    frame: &'a FrameLayout,
    scratch: [PReg; 2],
    used: Vec<(u32, PReg)>,
    reloads: Vec<X86Inst>,
    stores: Vec<X86Inst>,
}

impl<'a> SpillRw<'a> {
    fn new(
        assignments: &'a HashMap<u32, PReg>,
        spill_slots: &'a HashMap<u32, usize>,
        frame: &'a FrameLayout,
        scratch: [PReg; 2]
    ) -> Self {
        Self {
            assignments,
            spill_slots,
            frame,
            scratch,
            used: Vec::new(),
            reloads: Vec::new(),
            stores: Vec::new()
        }
    }
}

impl RegVisitor for SpillRw<'_> {
    fn reg(&mut self, r: &mut Reg, kind: OpKind, _c: Constraint) {
        if let Reg::V(v) = *r {
            if let Some(&idx) = self.spill_slots.get(&v.0) {
                let dsp = self.frame.spill_offset(idx);

                let pr = match self.used.iter().find(|(vr, _)| *vr == v.0) {
                    Some(&(_, p)) => p,
                    None => {
                        let p = *self.scratch.get(self.used.len())
                            .expect(">2 distinct spilled vregs in inst");
                        self.used.push((v.0, p));

                        p
                    }
                };

                *r = Reg::P(pr);

                match kind {
                    OpKind::Use => self.reloads.push(X86Inst::MovFromMem { size: Size::S64, dst: Reg::P(pr), dsp }),
                    OpKind::Def => self.stores.push(X86Inst::MovToMem { size: Size::S64, dsp, src: Reg::P(pr) }),
                    OpKind::UseDef => {
                        self.reloads.push(X86Inst::MovFromMem { size: Size::S64, dst: Reg::P(pr), dsp });
                        self.stores.push(X86Inst::MovToMem { size: Size::S64, dsp, src: Reg::P(pr) });
                    }
                }
            } else {
                *r = Reg::P(self.assignments[&v.0]);
            }
        }
    }

    fn clobber(&mut self, _p: PReg) {}
}

pub struct RegAlloc;

impl RegAlloc {
    pub fn run(vc: &mut VCode<X86Inst>, frame: &FrameLayout, abi: &AbiSpec) -> i32 {
        let liveness = Liveness::compute(vc);
        let intervals = LiveIntervals::build(vc, &liveness);
        let scan = LinearScan::new(intervals.intervals, abi.int_pool(), abi.float_pool());

        let (assignments, spilled) = scan.run();

        let mut spill_slots: HashMap<u32, usize> = HashMap::new();
        for v in &spilled {
            let idx = spill_slots.len();
            spill_slots.entry(*v).or_insert(idx);
        }

        let mut out = Vec::with_capacity(vc.insts.len());
        for mut inst in vc.insts.drain(..) {
            let mut rw = SpillRw::new(&assignments, &spill_slots, frame, abi.scratch);
            inst.visit_regs(&mut rw);

            out.extend(rw.reloads);
            out.push(inst);
            out.extend(rw.stores);
        }

        vc.insts = out;
        frame.total_size(spill_slots.len())
    }
}