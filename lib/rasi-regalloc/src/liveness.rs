use std::collections::HashSet;
use rasi_codegen::{machinst::MachInst, Block, Collector, OpKind, VCode};

#[derive(Debug)]
pub struct BlockLiveness {
    pub live_in: HashSet<u32>,
    pub live_out: HashSet<u32>,
}

#[derive(Debug)]
pub struct Liveness {
    pub blocks: Vec<BlockLiveness>
}

impl Liveness {
    pub fn compute<I: MachInst + Clone>(vc: &VCode<I>) -> Liveness {
        let mut genkill: Vec<(HashSet<u32>, HashSet<u32>)> = Vec::new();
        for b in 0..vc.n_blocks() {
            let block = Block(b as u32);

            let mut kill: HashSet<u32> = HashSet::new();
            let mut genn: HashSet<u32> = HashSet::new();

            for mut inst in vc.block_insts(block).iter().cloned() {
                let mut c = Collector::default();
                inst.visit_regs(&mut c);

                for (reg, kind, _constraints) in &c.ops {
                    let Some(vr) = reg.as_vreg() else { continue };
                    let is_use = matches!(kind, OpKind::Use | OpKind::UseDef);
                    let is_def = matches!(kind, OpKind::Def | OpKind::UseDef);

                    if is_use && !kill.contains(&vr.0) {
                        genn.insert(vr.0);
                    }

                    if is_def {
                        kill.insert(vr.0);
                    }
                }
            }

            genkill.push((genn, kill));
        }

        let mut live_in: Vec<HashSet<u32>> = vec![HashSet::new(); vc.n_blocks()];
        let mut live_out: Vec<HashSet<u32>> = vec![HashSet::new(); vc.n_blocks()];

        loop {
            let mut chg = false;
            for b in 0..vc.n_blocks() {
                let mut new_o: HashSet<u32> = HashSet::new();
                for s in &vc.succs[b] {
                    new_o.extend(&live_in[s.0 as usize]);
                }

                let (gen_b, kill_b) = &genkill[b];
                let mut new_in = gen_b.clone();
                new_in.extend(new_o.difference(kill_b));

                if new_in != live_in[b] || new_o != live_out[b] {
                    live_in[b] = new_in;
                    live_out[b] = new_o;
                    chg = true;
                }
            }

            if !chg { break; }
        }

        let blocks = (0..vc.n_blocks())
            .map(|b| BlockLiveness { live_in: live_in[b].clone(), live_out: live_out[b].clone()})
            .collect();

        Liveness { blocks }
    }   
}