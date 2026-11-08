use std::sync::Arc;
use rasi_ir::{builder::IRBuilder, function::Function, types::Type};
use rasi_codegen::Flags;
use rasi_jit::JITModule;
use rasi_x86_64::X86Isa;

fn main() {
    let mut func = Function::new("foo", &[], Type::I32);
    let mut builder = IRBuilder::new(&mut func);

    let a = builder.iconst(123i32);
    let b = builder.iconst(89i32);
    let r = builder.iadd(a, b);
    builder.ret(Some(r));

    println!("{}", builder.func);

    let isa = X86Isa::new(Flags::default());
    let mut module = JITModule::new(Arc::new(isa));

    let id = module.compile("main", &func);
    module.finalize();

    let f: extern "C" fn() -> i32 = unsafe { module.get(id) };
    println!("{}", f());
}