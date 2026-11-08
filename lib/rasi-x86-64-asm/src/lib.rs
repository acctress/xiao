pub mod regs;
pub mod operand;
pub mod encode;
pub mod asm;
pub mod dis;

#[cfg(test)]
mod tests {
    use rasi_codegen::buffer::Buffer;
    use crate::{asm::X86Assembler, operand::{Mem, Scale}, regs::*};
    use Size::*;
    use crate::operand::Cc;

    fn enc(f: impl FnOnce(&mut X86Assembler)) -> Vec<u8> {
        let mut buf = Buffer::new();
        f(&mut X86Assembler::new(&mut buf));
        buf.finish()
    }

    #[test]
    fn alu() {
        assert_eq!(enc(|a| a.add(S64, RAX, RCX)), [0x48, 0x01, 0xC8]);
        assert_eq!(enc(|a| a.or (S64, RAX, RCX)), [0x48, 0x09, 0xC8]);
        assert_eq!(enc(|a| a.adc(S64, RAX, RCX)), [0x48, 0x11, 0xC8]);
        assert_eq!(enc(|a| a.sbb(S64, RAX, RCX)), [0x48, 0x19, 0xC8]);
        assert_eq!(enc(|a| a.and(S64, RAX, RCX)), [0x48, 0x21, 0xC8]);
        assert_eq!(enc(|a| a.sub(S64, RAX, RCX)), [0x48, 0x29, 0xC8]);
        assert_eq!(enc(|a| a.xor(S64, RAX, RCX)), [0x48, 0x31, 0xC8]);
        assert_eq!(enc(|a| a.cmp(S64, RAX, RCX)), [0x48, 0x39, 0xC8]);
        assert_eq!(enc(|a| a.add(S64, RAX, 8)), [0x48, 0x83, 0xC0, 0x08]);
        assert_eq!(enc(|a| a.or (S64, RAX, 8)), [0x48, 0x83, 0xC8, 0x08]);
        assert_eq!(enc(|a| a.adc(S64, RAX, 8)), [0x48, 0x83, 0xD0, 0x08]);
        assert_eq!(enc(|a| a.sbb(S64, RAX, 8)), [0x48, 0x83, 0xD8, 0x08]);
        assert_eq!(enc(|a| a.and(S64, RAX, 8)), [0x48, 0x83, 0xE0, 0x08]);
        assert_eq!(enc(|a| a.xor(S64, RAX, 8)), [0x48, 0x83, 0xF0, 0x08]);
        assert_eq!(enc(|a| a.cmp(S64, RAX, 8)), [0x48, 0x83, 0xF8, 0x08]);
        assert_eq!(enc(|a| a.and(S32, RAX, -16)), [0x83, 0xE0, 0xF0]);
        assert_eq!(enc(|a| a.cmp(S32, Mem::base(RAX), 5)),          [0x83, 0x38, 0x05]);
        assert_eq!(enc(|a| a.xor(S64, R9, R10)),                    [0x4D, 0x31, 0xD1]);
        assert_eq!(enc(|a| a.add(S8, Mem::base(RBX), RSI)),         [0x40, 0x00, 0x33]);
    }

    #[test]
    fn alu_with_size_imms() {
        assert_eq!(enc(|a| a.sub(S64, RAX, 8)),      [0x48, 0x83, 0xE8, 0x08]);
        assert_eq!(enc(|a| a.sub(S64, RAX, 0x1000)), [0x48, 0x81, 0xE8, 0x00, 0x10, 0x00, 0x00]);
        assert_eq!(enc(|a| a.add(S32, RAX, 0x1000)), [0x81, 0xC0, 0x00, 0x10, 0x00, 0x00]);
        assert_eq!(enc(|a| a.add(S16, RAX, RCX)),    [0x66, 0x01, 0xC8]);
        assert_eq!(enc(|a| a.add(S16, RAX, 8)),      [0x66, 0x83, 0xC0, 0x08]);
        assert_eq!(enc(|a| a.add(S16, RAX, 0x1000)), [0x66, 0x81, 0xC0, 0x00, 0x10]);
        assert_eq!(enc(|a| a.add(S8,  RAX, RCX)),    [0x00, 0xC8]);
        assert_eq!(enc(|a| a.sub(S8,  RAX, 5)),      [0x80, 0xE8, 0x05]);
    }

    #[test]
    fn mov() {
        assert_eq!(enc(|a| a.mov(S64, RAX, RCX)), [0x48, 0x89, 0xC8]);
        assert_eq!(enc(|a| a.mov(S64, R9, R10)),  [0x4D, 0x89, 0xD1]);
        assert_eq!(enc(|a| a.mov(S32, R9, R10)),  [0x45, 0x89, 0xD1]);
        assert_eq!(enc(|a| a.mov(S32, RAX, RCX)), [0x89, 0xC8]);
        assert_eq!(enc(|a| a.mov(S16, RAX, RCX)), [0x66, 0x89, 0xC8]);
        assert_eq!(enc(|a| a.mov(S8,  RAX, RCX)), [0x88, 0xC8]);
        assert_eq!(enc(|a| a.mov(S64, Mem::base(RSP).disp(8), RAX)), [0x48, 0x89, 0x44, 0x24, 0x08]);
        assert_eq!(enc(|a| a.mov(S64, Mem::base(RBP), RAX)),         [0x48, 0x89, 0x45, 0x00]);
    }

    #[test]
    fn mov_imms_loads() {
        assert_eq!(enc(|a| a.mov(S64, RAX, 1)),  [0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00]);
        assert_eq!(enc(|a| a.mov(S8,  RAX, 1)),  [0xC6, 0xC0, 0x01]);
        assert_eq!(enc(|a| a.mov(S64, Mem::base(RSP).disp(8), -1)),
                   [0x48, 0xC7, 0x44, 0x24, 0x08, 0xFF, 0xFF, 0xFF, 0xFF]);
        assert_eq!(enc(|a| a.mov_load(S64, RCX, Mem::base(RSP).disp(8))), [0x48, 0x8B, 0x4C, 0x24, 0x08]);
        assert_eq!(enc(|a| a.mov_load(S8,  RSI, Mem::base(RAX))),         [0x40, 0x8A, 0x30]);
        assert_eq!(enc(|a| a.movabs(RAX, 0x1122334455667788)),
                   [0x48, 0xB8, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11]);
        assert_eq!(enc(|a| a.movabs(R9, 0x1122334455667788)),
                   [0x49, 0xB9, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11]);
    }

    #[test]
    fn byte_regs_nr() {
        assert_eq!(enc(|a| a.add(S8, RSI, RDI)), [0x40, 0x00, 0xFE]);
        assert_eq!(enc(|a| a.sub(S8, RSI, 1)),   [0x40, 0x80, 0xEE, 0x01]);
        assert_eq!(enc(|a| a.mov(S8, Mem::base(RSP).disp(8), RSI)), [0x40, 0x88, 0x74, 0x24, 0x08]);
    }

    #[test]
    fn addressing() {
        assert_eq!(enc(|a| a.sub(S64, Mem::base(RSP).disp(8), RAX)), [0x48, 0x29, 0x44, 0x24, 0x08]);
        assert_eq!(enc(|a| a.sub(S64, Mem::base(RSP), RAX)),         [0x48, 0x29, 0x04, 0x24]);
        assert_eq!(enc(|a| a.sub(S64, Mem::base(RBP), RAX)),         [0x48, 0x29, 0x45, 0x00]);
        assert_eq!(enc(|a| a.sub(S64, Mem::base(R12), R13)),         [0x4D, 0x29, 0x2C, 0x24]);
        assert_eq!(enc(|a| a.sub(S64, Mem::base(R13), RAX)),         [0x49, 0x29, 0x45, 0x00]);
        assert_eq!(
            enc(|a| a.sub(S64, Mem::base(RAX).index(RCX, Scale::X4).disp(8), RDX)),
            [0x48, 0x29, 0x54, 0x88, 0x08]
        );
    }

    #[test]
    fn unary_all() {
        assert_eq!(enc(|a| a.not (S64, RAX)), [0x48, 0xF7, 0xD0]);
        assert_eq!(enc(|a| a.neg (S64, RAX)), [0x48, 0xF7, 0xD8]);
        assert_eq!(enc(|a| a.mul (S64, RAX)), [0x48, 0xF7, 0xE0]);
        assert_eq!(enc(|a| a.imul1(S64, RAX)), [0x48, 0xF7, 0xE8]);
        assert_eq!(enc(|a| a.div (S64, RAX)), [0x48, 0xF7, 0xF0]);
        assert_eq!(enc(|a| a.idiv(S64, RAX)), [0x48, 0xF7, 0xF8]);
        assert_eq!(enc(|a| a.not(S32, RCX)),  [0xF7, 0xD1]);
        assert_eq!(enc(|a| a.not(S16, RAX)),  [0x66, 0xF7, 0xD0]);
        assert_eq!(enc(|a| a.neg(S8,  RAX)),  [0xF6, 0xD8]);
        assert_eq!(enc(|a| a.not(S8,  RSI)),  [0x40, 0xF6, 0xD6]);
        assert_eq!(enc(|a| a.neg(S64, R9)),                 [0x49, 0xF7, 0xD9]);
        assert_eq!(enc(|a| a.div(S32, Mem::base(RBX))),     [0xF7, 0x33]);
    }

    #[test]
    fn fixed_all() {
        assert_eq!(enc(|a| a.ret()),   [0xC3]);
        assert_eq!(enc(|a| a.cqo()),   [0x48, 0x99]);
        assert_eq!(enc(|a| a.cdq()),   [0x99]);
        assert_eq!(enc(|a| a.nop()),   [0x90]);
        assert_eq!(enc(|a| a.int3()),  [0xCC]);
        assert_eq!(enc(|a| a.leave()), [0xC9]);
        assert_eq!(enc(|a| a.ud2()),   [0x0F, 0x0B]);
    }

    #[test]
    fn cc_ops() {
        assert_eq!(enc(|a| a.setcc(Cc::E,  RAX)),                [0x0F, 0x94, 0xC0]);
        assert_eq!(enc(|a| a.setcc(Cc::Ne, RSI)),                [0x40, 0x0F, 0x95, 0xC6]);
        assert_eq!(enc(|a| a.setcc(Cc::L,  Mem::base(RAX))),     [0x0F, 0x9C, 0x00]);
        assert_eq!(enc(|a| a.cmovcc(Cc::E, S64, RAX, RCX)),      [0x48, 0x0F, 0x44, 0xC1]);
        assert_eq!(enc(|a| a.cmovcc(Cc::G, S32, RAX, RCX)),      [0x0F, 0x4F, 0xC1]);
        assert_eq!(enc(|a| a.cmovcc(Cc::Ne, S64, R9, R10)),      [0x4D, 0x0F, 0x45, 0xCA]);
        assert_eq!(Cc::E.invert(), Cc::Ne);
        assert_eq!(Cc::L.invert(), Cc::Ge);
    }

    #[test]
    fn lea_imul_ext() {
        assert_eq!(enc(|a| a.lea(S64, RAX, Mem::base(RSP).disp(8))), [0x48, 0x8D, 0x44, 0x24, 0x08]);
        assert_eq!(enc(|a| a.lea(S64, RAX, Mem::base(RAX).index(RCX, Scale::X4).disp(8))),
                   [0x48, 0x8D, 0x44, 0x88, 0x08]);
        assert_eq!(enc(|a| a.imul(S64, RAX, RCX)),         [0x48, 0x0F, 0xAF, 0xC1]);
        assert_eq!(enc(|a| a.imul(S64, R9, R10)),          [0x4D, 0x0F, 0xAF, 0xCA]);
        assert_eq!(enc(|a| a.imul_imm(S32, RAX, RCX, 10)),    [0x6B, 0xC1, 0x0A]);
        assert_eq!(enc(|a| a.imul_imm(S64, RAX, RCX, 0x1000)),[0x48, 0x69, 0xC1, 0x00, 0x10, 0x00, 0x00]);

        assert_eq!(enc(|a| a.movzx(S32, RAX, S8,  RCX)),   [0x0F, 0xB6, 0xC1]);
        assert_eq!(enc(|a| a.movzx(S32, RAX, S8,  RSI)),   [0x40, 0x0F, 0xB6, 0xC6]);
        assert_eq!(enc(|a| a.movzx(S32, R9,  S8,  RSI)),   [0x44, 0x0F, 0xB6, 0xCE]);
        assert_eq!(enc(|a| a.movzx(S32, RAX, S16, RCX)),   [0x0F, 0xB7, 0xC1]);
        assert_eq!(enc(|a| a.movzx(S64, RAX, S8,  RCX)),   [0x48, 0x0F, 0xB6, 0xC1]);
        assert_eq!(enc(|a| a.movsx(S64, RAX, S8,  RCX)),   [0x48, 0x0F, 0xBE, 0xC1]);
        assert_eq!(enc(|a| a.movsx(S32, RAX, S16, RCX)),   [0x0F, 0xBF, 0xC1]);
        assert_eq!(enc(|a| a.movsx(S64, RAX, S32, RCX)),   [0x48, 0x63, 0xC1]);
        assert_eq!(enc(|a| a.movsx(S32, RAX, S8, Mem::base(RAX))), [0x0F, 0xBE, 0x00]);
    }

    #[test]
    fn push_pop() {
        assert_eq!(enc(|a| a.push(RAX)),        [0x50]);
        assert_eq!(enc(|a| a.push(R15)),        [0x41, 0x57]);
        assert_eq!(enc(|a| a.pop(RBX)),         [0x5B]);
        assert_eq!(enc(|a| a.pop(R12)),         [0x41, 0x5C]);
        assert_eq!(enc(|a| a.push_imm(8)),      [0x6A, 0x08]);
        assert_eq!(enc(|a| a.push_imm(0x1000)), [0x68, 0x00, 0x10, 0x00, 0x00]);
    }

    #[test]
    fn control_flow() {
        assert_eq!(enc(|a| { let l = a.new_label(); a.jmp(l); a.nop(); a.bind(l); }),
                   [0xE9, 0x01, 0x00, 0x00, 0x00, 0x90]);
        assert_eq!(enc(|a| { let l = a.new_label(); a.bind(l); a.nop(); a.jcc(Cc::Ne, l); }),
                   [0x90, 0x0F, 0x85, 0xF9, 0xFF, 0xFF, 0xFF]);
        assert_eq!(enc(|a| a.jmp_rm(RAX)),  [0xFF, 0xE0]);
        assert_eq!(enc(|a| a.call_rm(RAX)), [0xFF, 0xD0]);
        assert_eq!(enc(|a| a.call_rm(R11)), [0x41, 0xFF, 0xD3]);
    }
}