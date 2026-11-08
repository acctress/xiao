use std::sync::Arc;
use rasi_ir::{builder::IRBuilder, function::Function, types::Type};
use rasi_codegen::Flags;
use rasi_jit::JITModule;
use rasi_x86_64::X86Isa;

fn main() {
    let mut func = Function::new("main", &[], Type::I32);
    let mut builder = IRBuilder::new(&mut func);

    let a = builder.iconst(123i32);
    let b = builder.iconst(89i32);
    let s = builder.iadd(a, b);
    let c = builder.iconst(7i32);
    let q = builder.sdiv(s, c);
    let r = builder.iadd(q, s);
    builder.ret(Some(r));

    println!("{}", builder.func);

    let isa = X86Isa::new(Flags::default());
    let mut module = JITModule::new(Arc::new(isa));

    let id = module.compile("main", &func);
    module.finalize();

    let f: extern "C" fn() -> i32 = unsafe { module.get(id) };
    println!("{}", f());
}