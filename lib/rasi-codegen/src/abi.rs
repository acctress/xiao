use crate::PReg;

pub struct AbiSpec {
    pub int_args:           &'static [PReg],
    pub float_args:         &'static [PReg],
    pub int_return:         &'static [PReg],
    pub float_return:       &'static [PReg],
    pub caller_saved_int:   &'static [PReg],
    pub caller_saved_float: &'static [PReg],
    pub callee_saved_int:   &'static [PReg],
    pub callee_saved_float: &'static [PReg],
    pub scratch:            [PReg; 2],
    pub stack_alignment:    u32,
    pub shadow_space:       u32,
}

impl AbiSpec {
    pub fn int_pool(&self) -> Vec<PReg> {
        self.caller_saved_int.iter().copied().filter(|r| !self.scratch.contains(r)).collect()
    }
    
    pub fn float_pool(&self) -> Vec<PReg> {
        self.caller_saved_float.iter().copied().filter(|r| !self.scratch.contains(r)).collect()
    }
}