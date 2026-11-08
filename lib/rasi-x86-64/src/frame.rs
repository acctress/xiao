use rasi_ir::function::StackSlotData;

pub struct FrameLayout {
    pub exp_size: i32,
    pub slot_offsets: Vec<i32>,
}

fn align_up(a: i32, b: i32) -> i32 { (a + b - 1) / b * b }

impl FrameLayout {
    pub fn layout(slots: &[StackSlotData]) -> FrameLayout {
        let mut offset = 0i32;
        let slot_offsets = slots.iter()
            .map(|s| { offset = align_up(offset + s.size as i32, s.align as i32); -offset }).collect();

        FrameLayout { slot_offsets, exp_size: offset }
    }

    pub fn spill_offset(&self, idx: usize) -> i32 {
        -(self.exp_size + (idx as i32 + 1) * 8)
    }

    pub fn total_size(&self, n_spills: usize) -> i32 {
        align_up(self.exp_size + n_spills as i32 * 8, 16)
    }
}