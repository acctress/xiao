# rasi

[![CI](https://github.com/acctress/rasi/actions/workflows/rust.yml/badge.svg?branch=rust)](https://github.com/acctress/rasi/actions/workflows/rust.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![Rust](https://img.shields.io/badge/rust-2024-orange?logo=rust)
![Targets](https://img.shields.io/badge/target-x86--64-informational)
![ABI](https://img.shields.io/badge/ABI-SysV%20%7C%20Win64-informational)
![Last commit](https://img.shields.io/github/last-commit/acctress/rasi)

A retargetable compiler backend suite in Rust, targeting x86-64.

## Example

```rust
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
    let c = builder.iadd(a, b);
    builder.ret(Some(c));

    println!("{}", builder.func);

    let isa = X86Isa::new(Flags::default());
    let mut module = JITModule::new(Arc::new(isa));

    let id = module.compile("main", &func);
    module.finalize();

    let f: extern "C" fn() -> i32 = unsafe { module.get(id) };
    println!("{}", f());
}
```

Here is the debug output of the IR, VCode and the result of the function `main`.

```
define @main() -> i32 {
block0:
    %0 = iconst i32 123
    %1 = iconst i32 89
    %2 = iadd i32 %0, %1
    ret i32 %2
}

block0:
    MovImm { size: S32, dst: rdx, imm: 123 }
    MovImm { size: S32, dst: rcx, imm: 89 }
    Mov { size: S32, dst: rax, src: rdx }
    Alu { op: Add, size: S32, dst: rax, src: rcx }
    Mov { size: S32, dst: rax, src: rax }
    Ret

212
```