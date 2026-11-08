<p align="center">
  <img src="assets/logo.svg" width="112" alt="xiao logo">
</p>

<h1 align="center">xiao</h1>

<p align="center">
  A small, general purpose compiler backend written in C++23.
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License: MIT"></a>
  <img src="https://img.shields.io/badge/language-C%2B%2B23-blue" alt="C++23">
  <img src="https://img.shields.io/badge/target-x86--64-lightgrey" alt="x86-64">
  <img src="https://img.shields.io/github/last-commit/acctress/xiao" alt="Last commit">
  <img src="https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/acctress/xiao/cxx/.github/badges/loc.json" alt="Lines of code">
</p>

## Overview

xiao (小, *xiǎo*, meaning "small") is a compiler backend. It takes functions written in an SSA
intermediate representation and compiles them to native machine code. It is independent of any
frontend, so it can be used beneath a programming language, a scripting engine, a domain specific
language or any other tool that needs to generate code.

How the compiled code is used is decided by the output module. xiao currently provides `xiao-jit`,
which loads code into memory for immediate execution. The module layer is designed so that other
outputs, such as ahead of time compilation to object files, can be added alongside it.

The architecture is modelled on [Cranelift](https://cranelift.dev), and the library is header-only.

## Features

**Intermediate representation**
- SSA form with block parameters
- Builder API, verifier and text printer
- Integer arithmetic: `iconst`, `iadd`, `isub`, `imul`, `idiv`
- Comparisons: `icmp` with all signed and unsigned conditions
- Control flow: `br`, `brif` and `ret` with block arguments, supporting loops and merges
- Memory: `load` and `store` through pointers with a constant offset
- Calls to functions in the same module or to external symbols

**x86-64 backend**
- Lowering to machine instructions on virtual registers
- Linear scan register allocation with fixed register constraints, using callee saved registers for
  values that live across calls and spilling to stack slots under register pressure
- Prologue and epilogue generation with stack alignment and Win64 shadow space
- Windows x64 and System V calling conventions
- Relocations for calls

**Module layer**
- Named function declarations with `Import`, `Local` and `Export` linkage
- A `Module` interface implemented by output modules

**xiao-jit**
- Declare, define, finalise and call functions at runtime
- Host symbol registration, with fallback lookup in the executable and the C runtime
- Code memory that is writable until finalisation and executable afterwards

**xiao-disasm** (optional)
- Disassembles compiled code to Intel syntax using [Zydis](https://github.com/zyantific/zydis),
  annotated with relocations

## Example

The following compiles `calc(x, y) = sqr_func(x + y) - x * y` with `xiao-jit`, where `sqr_func` is
an ordinary C++ function.

```cpp
#include <xiao-jit/module.hh>
#include <xiao/isa/host.hh>
#include <xiao/ir/builder.hh>
#include <xiao/ir/module.hh>

using namespace xiao;
using ir::Type;

extern "C" int64_t sqr_func( const int64_t v )
{ return v * v; }

int main( )
{
    jit::JITBuilder builder { isa::lookup_host( ) };
    builder.symbol( "sqr_func", reinterpret_cast< const void* >( &sqr_func ) );
    jit::JITModule jit { std::move( builder ) };

    const auto square_id = *jit.declare_function( "sqr_func", module::Linkage::Import,
                                                  { { Type::i64 }, Type::i64, codegen::host_call_conv( ) } );

    ir::IRModule irm;
    const auto   calc_ref = irm.declare( "calc", Type::i64, { Type::i64, Type::i64 } );
    const auto   calc_id  = *jit.declare_function( "calc", module::Linkage::Export,
                                                   module::signature_of( *irm.get( calc_ref ) ) );

    {
        ir::IRBuilder b { *irm.get( calc_ref ) };
        const auto    square = jit.declare_func_in_func( square_id, b );
        const auto    entry  = b.add_block( { Type::i64, Type::i64 } );
        b.switch_to_block( entry );

        const auto         x      = b.param( 0 );
        const auto         y      = b.param( 1 );
        const ir::ValueRef args[] = { b.iadd( x, y ) };
        b.ret( b.isub( b.call( square, args ), b.imul( x, y ) ) );
        b.finalise( );
    }

    codegen::Context ctx { *irm.get( calc_ref ) };
    if ( !jit.define_function( calc_id, ctx ) || !jit.finalise_definitions( ) ) return 1;

    auto* const calc = jit.get_finalised_function< int64_t( int64_t, int64_t ) >( calc_id );
    return calc( 3, 4 ) == 37 ? 0 : 1;
}
```

The function's IR:

```
fn calc(v0: i64, v1: i64) -> i64 {
    fn0 = u0:0(i64) -> i64
b0(v0: i64, v1: i64):
    v2 = iadd v0, v1
    v3 = call fn0(v2)
    v4 = imul v0, v1
    v5 = isub v3, v4
    ret v5
}
```

A larger example using every supported feature can be found in [`source/entry.cc`](source/entry.cc).

## Architecture

1. Functions are built in the IR with `IRBuilder` and checked by `IRVerifier`.
2. `Context::compile` lowers each function to target instructions on virtual registers.
3. The register allocator assigns physical registers.
4. Emission produces `CompiledCode`: the machine code and its relocations.
5. An output module consumes the compiled code. `xiao-jit` places it in executable memory and
   resolves its relocations.

## Building

xiao is header-only. Add `include/` to your include path and compile with a C++23 compiler.

The repository includes an example program, which can be built with CMake:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

The executable is written to `bin/exe/`. xiao is developed and tested with Clang on Windows.

By default the example also prints the generated machine code using `xiao-disasm`, which downloads
Zydis at configure time. Pass `-DXIAO_DISASM=OFF` to build without it. The xiao library itself has
no dependencies.

## Project layout

| Path | Contents |
|---|---|
| `include/xiao/ir` | IR definitions, builder, verifier and printer |
| `include/xiao/codegen` | Target independent code generation: VCode, machine buffer, relocations, `Context`, `TargetIsa` |
| `include/xiao/isa/x86` | x86-64 instructions, lowering, encoding, frames and calling conventions |
| `include/xiao/regalloc` | Liveness analysis, live intervals and linear scan allocation |
| `include/xiao/module` | Function declarations, linkage and the `Module` interface |
| `include/xiao-jit` | JIT module, builder and executable memory |
| `include/xiao-disasm` | Optional disassembler for compiled code, built on Zydis |

## Status

xiao is in early development. The following are not yet supported:

- Targets other than x86-64. Windows x64 is tested; System V is implemented but untested.
- Floating point types, and loads and stores narrower than 64 bits.
- Arguments passed on the stack.
- Ahead of time compilation and object file output.
- Global data, data objects and unwind information.

## Acknowledgements

xiao's design is based on [Cranelift](https://github.com/bytecodealliance/wasmtime/tree/main/cranelift).

## License

xiao is available under the [MIT License](LICENSE).
