use std::sync::Arc;
use rasi_ir::{builder::IRBuilder, function::Function, types::Type};
use rasi_codegen::Flags;
use rasi_jit::JITModule;
use rasi_x86_64::X86Isa;

fn main() {
    let mut func = Function::new("main", &[], Type::I32);
    let mut builder = IRBuilder::new(&mut func);

    let a = builder.iconst(10i32);
    let b = builder.iconst(20i32);
    let c = builder.iconst(30i32);
    let d = builder.iconst(40i32);

    let t1 = builder.iadd(a, b);
    let t2 = builder.iadd(c, d);
    let t3 = builder.iadd(t1, t2);
    let t4 = builder.iadd(t3, a);
    let t5 = builder.iadd(t4, b);
    let t6 = builder.iadd(t5, c);
    let t7 = builder.iadd(t6, d);
    builder.ret(Some(t7));

    println!("{}", builder.func);

    let isa = X86Isa::new(Flags::default());
    let mut module = JITModule::new(Arc::new(isa));
    let id = module.compile("main", &func);
    module.finalize();

    let f: extern "C" fn() -> i32 = unsafe { module.get(id) };
    let result = f();
    println!("result = {result}");
    assert_eq!(result, 200, "spill-pressure test FAILED: expected 200");
    println!("PASS");
}