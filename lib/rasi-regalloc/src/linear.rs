use std::collections::HashMap;
use rasi_codegen::{PReg, RegClass};
use crate::interval::{LiveInterval, Point};

struct Active { vreg: u32, preg: PReg, end: Point, fixed: bool }

pub struct LinearScan {
    intervals: Vec<LiveInterval>,
    ints_pool: Vec<PReg>,
    floats_pool: Vec<PReg>,
    active: Vec<Active>,
    assignments: HashMap<u32, PReg>,
    spilled: Vec<u32>
}

fn same_pr(a: PReg, b: PReg) -> bool { a.hw == b.hw && a.class == b.class }

impl LinearScan {
    pub fn new(intervals: Vec<LiveInterval>, ints_pool: Vec<PReg>, floats_pool: Vec<PReg>) -> Self {
        Self {
            intervals,
            ints_pool,
            floats_pool,
            active: Vec::new(),
            assignments: HashMap::new(),
            spilled: Vec::new(),
        }
    }

    fn pool(&mut self, class: RegClass) -> &mut Vec<PReg> {
        match class { RegClass::Int => &mut self.ints_pool, RegClass::Float => &mut self.floats_pool }
    }

    pub fn run(mut self) -> (HashMap<u32, PReg>, Vec<u32>) {
        let mut inters = std::mem::take(&mut self.intervals);
        inters.sort_by_key(|v| (v.start(), v.fixed.is_none()));

        for interval in &inters {
            let mut curr_active = Vec::new();
            for a in self.active.drain(..) {
                if a.end <= interval.start() {
                    match a.preg.class {
                        RegClass::Int       => self.ints_pool.push(a.preg),
                        RegClass::Float     => self.floats_pool.push(a.preg)
                    }
                } else { curr_active.push(a); }
            }

            self.active = curr_active;

            if let Some(preg) = interval.fixed {
                let pool = self.pool(preg.class);

                if let Some(pos) = pool.iter().position(|p| same_pr(*p, preg)) {
                    pool.remove(pos);
                } else {
                    let idx = self.active.iter().position(|a| !a.fixed && same_pr(a.preg, preg))
                        .expect("fixed reg requested but not free or held by active vreg");

                    if self.active[idx].end > interval.start() + 1 {
                        let held = self.active.remove(idx);
                        self.spilled.push(held.vreg);
                    } else {
                        self.active.remove(idx);
                    }
                }

                self.active.push(Active { vreg: interval.vreg, preg, end: interval.end(),
                    fixed: true });
                continue;
            }

            let pool = self.pool(interval.class);

            if let Some(preg) = pool.pop() {
                self.assignments.insert(interval.vreg, preg);
                self.active.push(Active { vreg: interval.vreg, preg, end: interval.end(),
                    fixed: false });
            } else {
                let vic_idx = self.active.iter().enumerate()
                    .filter(|(_, a)| !a.fixed && a.preg.class == interval.class)
                    .max_by_key(|(_, a)| a.end)
                    .map(|(i, _)| i);

                match vic_idx {
                    None      => self.spilled.push(interval.vreg),
                    Some(idx) => {
                        if interval.end() > self.active[idx].end {
                            self.spilled.push(interval.vreg);
                        } else {
                            let vic = self.active.remove(idx);
                            self.spilled.push(vic.vreg);
                            self.assignments.insert(interval.vreg, vic.preg);
                            self.active.push(Active { vreg: interval.vreg, preg: vic.preg,
                                end: interval.end(), fixed: false });
                        }
                    }
                }
            }
        }

        (self.assignments, self.spilled)
    }
}