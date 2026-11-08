use std::sync::Arc;
use rasi_codegen::{TargetIsa, CompiledCode};
use rasi_ir::function::Function;
use crate::ExecMemory;

struct CompiledBlob { name: String, ptr: *mut u8 }

pub struct FuncId(u32);
pub struct JITModule {
    isa: Arc<dyn TargetIsa>,
    mem: ExecMemory,
    funcs: Vec<CompiledBlob>,
    finalized: bool
}

impl JITModule {
    pub fn new(isa: Arc<dyn TargetIsa>) -> Self {
        Self { isa, mem: ExecMemory::new(), funcs: Vec::new(), finalized: false }
    }

    pub fn finalize(&mut self) { self.mem.finalize(); self.finalized = true; }

    pub fn compile(&mut self, name: &str, func: &Function) -> FuncId {
        assert!(!self.finalized, "unable to compile after finalize");

        let code = self.isa.compile(func);
        let ptr = self.mem.append(&code.bytes);
        self.funcs.push(CompiledBlob { name: name.to_owned(), ptr });

        FuncId(self.funcs.len() as u32 - 1)
    }

    pub unsafe fn get<F: Copy>(&self, id: FuncId) -> F {
        assert!(self.finalized, "module not finalised");
        unsafe { std::mem::transmute_copy(&self.funcs[id.0 as usize].ptr) }
    }
}