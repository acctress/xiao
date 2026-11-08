use std::sync::Arc;
use rasi_ir::{builder::IRBuilder, function::Function, types::Type};
use rasi_codegen::Flags;
use rasi_jit::JITModule;
use rasi_x86_64::X86Isa;

fn main() {
    let mut func = Function::new("main", &[], Type::I32);
    let mut builder = IRBuilder::new(&mut func);

    let v: Vec<_> = (1..=9i32).map(|n| builder.iconst(n)).collect();
    let mut sum = v[0];
    for &vi in &v[1..] {
        sum = builder.iadd(sum, vi);
    }
    builder.ret(Some(sum));

    println!("{}", builder.func);

    let isa = X86Isa::new(Flags::default());
    let mut module = JITModule::new(Arc::new(isa));
    let id = module.compile("main", &func);

    module.finalize();

    let f: extern "C" fn() -> i32 = unsafe { module.get(id) };
    let result = f();
    println!("result = {result}");
    assert_eq!(result, 45, "spill repro FAILED: expected 45");
    println!("PASS");
}