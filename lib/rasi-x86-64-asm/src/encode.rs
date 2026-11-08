use rasi_codegen::buffer::Buffer;
use crate::{operand::{Mem, Rm}, regs::{Gpr, Size}};

#[derive(Clone, Copy)]
pub enum RegField {
    Reg(Gpr),
    Ext(u8)
}

impl From<Gpr> for RegField { fn from(value: Gpr) -> Self { Self::Reg(value) } }
impl From<u8>  for RegField { fn from(value: u8)  -> Self { Self::Ext(value) } }

/// byte needs rex
const fn byte_nr(r: Gpr) -> bool { matches!(r.num(), 4..=7) }

pub const fn rex(w: bool, r: bool, x: bool, b: bool) -> u8 {
    0x40 | ((w as u8) << 3) | ((r as u8) << 2) | ((x as u8) << 1) | b as u8
}

pub const fn modrm(md: u8, reg: u8, rm: u8) -> u8 {
    (md << 6) | ((reg & 7) << 3) | (rm & 7)
}

pub const fn sib(scale: u8, index: u8, base: u8) -> u8 {
    (scale << 6) | ((index & 7) << 3) | (base & 7)
}

pub fn emit_rm(buf: &mut Buffer, size: Size, opcode: &[u8], reg: impl Into<RegField>, rm: &Rm) {
    let b = size == Size::S8;
    emit_rmx(buf, size, b, b, opcode, reg, rm);
}

pub fn emit_rmx(buf: &mut Buffer, size: Size, r8: bool, rm8: bool, opcode: &[u8], reg: impl Into<RegField>, rm: &Rm) {
    let reg = reg.into();

    let (bits, r, byte) = match reg {
        RegField::Reg(gpr) => (gpr.enc(), gpr.hi(), r8 && byte_nr(gpr)),
        RegField::Ext(val) => (val, false, false)
    };

    let (x, b) = match rm {
        Rm::Reg(gpr) => (false, gpr.hi()),
        Rm::Mem(mem) => (mem.index.is_some_and(|(i, _)| i.hi()), mem.base.hi()),
    };

    let rm_byte = rm8 && matches!(rm, Rm::Reg(gpr) if byte_nr(*gpr));
    let f = byte || rm_byte;
    let w = size == Size::S64;

    if size == Size::S16 { buf.put_u8(0x66); }
    if w || r || x || b || f { buf.put_u8(rex(w, r, x, b)); }

    buf.put(opcode);
    match rm {
        Rm::Reg(gpr) => buf.put_u8(modrm(0b11, bits, gpr.enc())),
        Rm::Mem(mem) => emit_mem(buf, bits, mem),
    }
}

fn emit_mem(buf: &mut Buffer, reg: u8, m: &Mem) {
    let md = if m.disp == 0 && m.base.enc() != 5  { 0b00 }
             else if i8::try_from(m.disp).is_ok() { 0b01 }
             else                                 { 0b10 };

    match m.index {
        None if m.base.enc() == 4 => {
            buf.put_u8(modrm(md, reg, 4));
            buf.put_u8(sib(0, 4, m.base.enc()));
        }

        None => buf.put_u8(modrm(md, reg, m.base.enc())),

        Some((idx, sc)) => {
            buf.put_u8(modrm(md, reg, 4));
            buf.put_u8(sib(sc as u8, idx.enc(), m.base.enc()));
        }
    }

    match md {
        0b01 => buf.put_u8(m.disp as i8 as u8),
        0b10 => buf.put(&m.disp.to_le_bytes()),
        _    => {}
    }
}

