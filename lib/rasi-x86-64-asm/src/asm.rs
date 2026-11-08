use rasi_codegen::buffer::{Buffer, FixupKind, Label};
use crate::encode::{emit_rm, emit_rmx, rex};
use crate::operand::*;
use crate::regs::*;

pub struct X86Assembler<'a> {
    buf: &'a mut Buffer,
}

fn wbit(size: Size) -> u8 { u8::from(size != Size::S8) }

impl<'a> X86Assembler<'a> {
    pub fn new(buf: &'a mut Buffer) -> Self { Self { buf } }

    pub fn new_label(&mut self) -> Label {
        self.buf.new_label()
    }

    pub fn bind(&mut self, l: Label) {
        self.buf.bind(l)
    }

    pub fn mov(&mut self, size: Size, dst: impl Into<Rm>, src: impl Into<RegImm>) {
        let dst: Rm = dst.into();
        match src.into() {
            RegImm::Reg(r) => emit_rm(self.buf, size, &[0x88 | wbit(size)], r, &dst),
            RegImm::Imm(i) => {
                if let (Size::S32, Rm::Reg(r)) = (size, dst) {
                    if r.hi() { self.buf.put_u8(0x41); }
                    self.buf.put_u8(0xB8 + r.enc());
                    self.buf.put(&i.to_le_bytes());
                    return;
                }

                let op = if size == Size::S8 { 0xC6 } else { 0xC7 };
                emit_rm(self.buf, size, &[op], 0u8, &dst);
                self.imm(size, i);
            }
        }
    }

    pub fn test(&mut self, size: Size, dst: impl Into<Rm>, src: impl Into<RegImm>) {
        let dst = dst.into();
        match src.into() {
            RegImm::Reg(r) => emit_rm(self.buf, size, &[0x84 | wbit(size)], r, &dst),
            RegImm::Imm(i) => {
                emit_rm(self.buf, size, &[0xF6 | wbit(size)], 0u8, &dst);
                self.imm(size, i);
                
            }
        }
    }

    pub fn mov_load(&mut self, size: Size, dst: Gpr, src: impl Into<Rm>) {
        emit_rm(self.buf, size, &[0x8A | wbit(size)], dst, &src.into());
    }

    pub fn movabs(&mut self, dst: Gpr, imm: i64) {
        self.buf.put_u8(rex(true, false, false, dst.hi()));
        self.buf.put_u8(0xB8 + dst.enc());
        self.buf.put(&imm.to_le_bytes());
    }

    pub fn jmp(&mut self, l: Label) { self.buf.put_u8(0xE9); self.rel32(l); }

    pub fn call(&mut self, l: Label) { self.buf.put_u8(0xE8); self.rel32(l); }

    pub fn jcc(&mut self, cc: Cc, l: Label) { self.buf.put(&[0x0F, 0x80 | cc as u8]); self.rel32(l); }

    pub fn jmp_rm(&mut self, t: impl Into<Rm>) { emit_rm(self.buf, Size::S32, &[0xFF], 4u8, &t.into()); }

    pub fn call_rm(&mut self, t: impl Into<Rm>) { emit_rm(self.buf, Size::S32, &[0xFF], 2u8, &t.into()); }

    pub fn setcc(&mut self, cc: Cc, dst: impl Into<Rm>) { emit_rm(self.buf, Size::S8, &[0x0F, 0x90 | cc as u8], 0u8, &dst.into()); }

    pub fn cmovcc(&mut self, cc: Cc, size: Size, dst: Gpr, src: impl Into<Rm>) {
        debug_assert!(size != Size::S8);
        emit_rm(self.buf, size, &[0x0F, 0x40 | cc as u8], dst, &src.into());
    }

    pub fn lea(&mut self, size: Size, dst: Gpr, src: Mem) {
        debug_assert!(size != Size::S8);
        emit_rm(self.buf, size, &[0x8D], dst, &Rm::Mem(src));
    }

    pub fn imul(&mut self, size: Size, dst: Gpr, src: impl Into<Rm>) {
        debug_assert!(size != Size::S8);
        emit_rm(self.buf, size, &[0x0F, 0xAF], dst, &src.into());
    }

    pub fn imul_imm(&mut self, size: Size, dst: Gpr, src: impl Into<Rm>, imm: i32) {
        debug_assert!(size != Size::S8);

        let src = src.into();
        if i8::try_from(imm).is_ok() {
            emit_rm(self.buf, size, &[0x6B], dst, &src);
            self.buf.put_u8(imm as u8);
        } else {
            emit_rm(self.buf, size, &[0x69], dst, &src);
            self.imm(size, imm);
        }
    }

    pub fn movzx(&mut self, dsz: Size, dst: Gpr, ssz: Size, src: impl Into<Rm>) {
        debug_assert!(dsz != Size::S8);

        let src = src.into();
        let op: &[u8] = match ssz {
            Size::S8  => &[0x0F, 0xB6],
            Size::S16 => &[0x0F, 0xB7],
            _ => return self.mov_load(Size::S32, dst, src),
        };

        emit_rmx(&mut self.buf, dsz, false, ssz == Size::S8, op, dst, &src);
    }

    pub fn movsx(&mut self, dsz: Size, dst: Gpr, ssz: Size, src: impl Into<Rm>) {
        debug_assert!(dsz != Size::S8);

        let src = src.into();
        let op: &[u8] = match ssz {
            Size::S8  => &[0x0F, 0xBE],
            Size::S16 => &[0x0F, 0xBF],
            _ => { debug_assert!(dsz == Size::S64); &[0x63] },
        };

        emit_rmx(&mut self.buf, dsz, false, ssz == Size::S8, op, dst, &src);
    }

    pub fn push(&mut self, r: Gpr) { self.opreg(0x50, r); }

    pub fn pop(&mut self, r: Gpr) { self.opreg(0x58, r); }

    pub fn push_imm(&mut self, i: i32) {
        if i8::try_from(i).is_ok() { self.buf.put(&[0x6A, i as u8]); }
        else { self.buf.put_u8(0x68); self.buf.put(&i.to_le_bytes()); }
    }

    fn opreg(&mut self, base: u8, r: Gpr) {
        if r.hi() { self.buf.put_u8(0x41); }
        self.buf.put_u8(base + r.enc());
    }

    fn rel32(&mut self, l: Label) {
        self.buf.use_label(l, FixupKind::Rel32);
    }

    fn imm(&mut self, size: Size, value: i32) {
        match size {
            Size::S8 => {
                debug_assert!((i8::MIN..=i8::MAX).contains(&(value as i8)));
                self.buf.put_u8(value as u8);
            }

            Size::S16 => {
                debug_assert!((i16::MIN..=i16::MAX).contains(&(value as i16)));
                self.buf.put(&(value as u16).to_le_bytes());
            }

            Size::S32 | Size::S64 => self.buf.put(&value.to_le_bytes())
        }
    }

    fn alu(&mut self, n: u8, size: Size, dst: Rm, src: RegImm) {
        match src {
            RegImm::Reg(r) => emit_rm(self.buf, size, &[n * 8 + wbit(size)], r, &dst),

            RegImm::Imm(i) if size != Size::S8 && i8::try_from(i).is_ok() => {
                emit_rm(self.buf, size, &[0x83], n, &dst);
                self.buf.put_u8(i as u8);
            }

            RegImm::Imm(i) => {
                let op = if size == Size::S8 { 0x80 } else { 0x81 };
                emit_rm(self.buf, size, &[op], n, &dst);
                self.imm(size, i);
            }
        }
    }

    fn shift(&mut self, n: u8, size: Size, dst: Rm, c: ShiftCount) {
        let (op, imm) = match c {
            ShiftCount::One | ShiftCount::Imm(1) => (0xD0, None),
            ShiftCount::Cl                       => (0xD2, None),
            ShiftCount::Imm(i)                   => (0xC0, Some(i)),
        };

        emit_rm(self.buf, size, &[op | wbit(size)], n, &dst);
        if let Some(i) = imm { self.buf.put_u8(i); }
    }

    fn unary(&mut self, n: u8, size: Size, dst: Rm) {
        emit_rm(self.buf, size, &[0xF6 | wbit(size)], n, &dst);
    }
}

macro_rules! alu { ($($name:ident = $n:expr),* $(,)?) => {
    impl X86Assembler<'_> {$(
        pub fn $name(&mut self, size: Size, dst: impl Into<Rm>, src: impl Into<RegImm>) {
            self.alu($n, size, dst.into(), src.into());
        }
    )*
    }
}}

macro_rules! shifts { ($($name:ident = $n:expr),* $(,)?) => {
    impl X86Assembler<'_> {$(
        pub fn $name(&mut self, size: Size, dst: impl Into<Rm>, c: impl Into<ShiftCount>) {
            self.shift($n, size, dst.into(), c.into());
        }
    )*
    }
}}

macro_rules! unary { ($($name:ident = $n:expr),* $(,)?) => {
    impl X86Assembler<'_> {$(
        pub fn $name(&mut self, size: Size, dst: impl Into<Rm>) {
            self.unary($n, size, dst.into());
        }
    )*
    }
}}

macro_rules! fixed { ($($name:ident = [$($b:expr),+]),* $(,)?) => {
    impl X86Assembler<'_> {$(
        pub fn $name(&mut self) {
            self.buf.put(&[$($b),+]);
        }
    )*
    }
}}

alu!(add = 0, or = 1, adc = 2, sbb = 3, and = 4, sub = 5, xor = 6, cmp = 7);
shifts!(rol = 0, ror = 1, rcl = 2, rcr = 3, shl = 4, shr = 5, sar = 7);
unary!(not = 2, neg = 3, mul = 4, imul1 = 5, div = 6, idiv = 7);
fixed!(ret = [0xC3], cqo = [0x48, 0x99], cdq = [0x99], nop = [0x90], int3 = [0xCC], leave = [0xC9], ud2 = [0x0F, 0x0B]);