use std::fmt;

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum RegClass { Int, Float }

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct PReg { pub class: RegClass, pub hw: u8 }

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct VReg(pub u32);

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum Reg { V(VReg), P(PReg) }

impl PReg {
    pub const fn new(class: RegClass, hw: u8) -> Self { Self { class, hw } }
    pub const fn int(hw: u8) -> Self { Self::new(RegClass::Int, hw) }
    pub const fn float(hw: u8) -> Self { Self::new(RegClass::Float, hw) }
}

impl Reg {
    pub fn as_preg(self) -> Option<PReg> { if let Reg::P(p) = self { Some(p) } else { None } }
    pub fn as_vreg(self) -> Option<VReg> { if let Reg::V(p) = self { Some(p) } else { None } }
    pub fn preg(self) -> PReg { self.as_preg().expect("vreg survbived regalloc") }
}

impl From<VReg> for Reg { fn from(value: VReg) -> Self { Reg::V(value) } }
impl From<PReg> for Reg { fn from(value: PReg) -> Self { Reg::P(value) } }

const GPR_NAMES: [&str; 16] = [
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
];

impl fmt::Display for PReg {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.class {
            RegClass::Int => write!(f, "\x1b[38;5;218m{}\x1b[0m", GPR_NAMES.get(self.hw as usize).unwrap_or(&"r?")),
            RegClass::Float => write!(f, "\x1b[38;5;218mxmm{}\x1b[0m", self.hw),
        }
    }
}

impl fmt::Display for VReg {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result { write!(f, "\x1b[38;5;230mv{}\x1b[0m", self.0) }
}

impl fmt::Display for Reg {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Reg::V(v) => write!(f, "{v}"),
            Reg::P(p) => write!(f, "{p}"),
        }
    }
}