use std::fmt;
use std::fmt::Formatter;
use rasi_codegen::{AbiSpec, Buffer, CompiledCode, Flags, MachInst, TargetIsa};
use rasi_ir::function::Function;
use rasi_x86_64_asm::asm::X86Assembler;
use rasi_x86_64_asm::regs::{Size, RBP, RSP};
use crate::{lower_function, RegAlloc, X86Inst};
use crate::abi::host_abi;

pub struct X86Isa {
    flags: Flags,
    abi: &'static AbiSpec,
}

impl X86Isa {
    pub fn new(flags: Flags) -> Self {
        Self { flags, abi: host_abi() }
    }

    pub fn with_abi(flags: Flags, abi: &'static AbiSpec) -> Self {
        Self { flags, abi: host_abi() }
    }
}

impl TargetIsa for X86Isa {
    fn compile(&self, func: &Function) -> CompiledCode {
        let (mut vc, frame) = lower_function(func);
        let total_size = RegAlloc::run(&mut vc, &frame, self.abi);
        
        println!("{}", vc);

        let mut buf = Buffer::new();
        {
            let mut a = X86Assembler::new(&mut buf);
            a.push(RBP);
            a.mov(Size::S64, RBP, RSP);
            if total_size > 0 { a.sub(Size::S64, RSP, total_size); }
        }

        for inst in &vc.insts {
            inst.emit(&mut buf);
        }

        CompiledCode { bytes: buf.finish(), relocs: vec![] }
    }
}

impl fmt::Display for X86Isa {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "x86_64")
    }
}