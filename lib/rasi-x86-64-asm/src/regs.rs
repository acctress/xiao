#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct Gpr(u8);

impl Gpr {
    pub const fn name(n: u8) -> Self { assert!(n < 16); Self(n) }
    pub const fn num(self) -> u8 { self.0 }
    pub const fn enc(self) -> u8 { self.0 & 7 }
    pub const fn hi(self) -> bool { self.0 >= 8 }
}

pub const RAX: Gpr = Gpr(0);
pub const RCX: Gpr = Gpr(1);
pub const RDX: Gpr = Gpr(2);
pub const RBX: Gpr = Gpr(3);
pub const RSP: Gpr = Gpr(4);
pub const RBP: Gpr = Gpr(5);
pub const RSI: Gpr = Gpr(6);
pub const RDI: Gpr = Gpr(7);
pub const R8: Gpr = Gpr(8);
pub const R9: Gpr = Gpr(9);
pub const R10: Gpr = Gpr(10);
pub const R11: Gpr = Gpr(11);
pub const R12: Gpr = Gpr(12);
pub const R13: Gpr = Gpr(13);
pub const R14: Gpr = Gpr(14);
pub const R15: Gpr = Gpr(15);

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct Xmm(pub u8);

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Size { S8, S16, S32, S64 }
