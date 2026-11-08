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
- Integer types `i8`, `i16`, `i32`, `i64` and `ptr`
- Arithmetic: `iconst`, `iadd`, `isub`, `imul`, `ineg`, `idiv`, `udiv`, `srem`, `urem`
- Bitwise and shifts: `band`, `bor`, `bxor`, `bnot`, `ishl`, `ushr`, `sshr`
- Conversions: `uextend`, `sextend`, `ireduce`
- Comparisons and selection: `icmp` with all signed and unsigned conditions, `select`
- Control flow: `br`, `brif` and `ret` with block arguments, supporting loops and merges
- Memory: 8, 16, 32 and 64-bit `load` and `store` through pointers with a constant offset
- Calls to functions in the same module or to external symbols
- Global values and `symbol_value` for the address of a data object, with an optional offset

**x86-64 backend**
- Lowering to machine instructions on virtual registers, with constants folded into immediates
- Backtracking register allocation in the style of LLVM's greedy allocator and regalloc2: live
  ranges with holes, copy coalescing into bundles, fixed register hints, exact call clobbers,
  eviction by spill weight, live range splitting with parallel move resolution, and a symbolic
  checker in debug builds
- Prologue and epilogue generation with stack alignment and Win64 shadow space
- Windows x64 and System V calling conventions
- Relocations for calls and data: RIP-relative `lea` for data in the module, absolute addresses for
  imported data

**Module layer**
- Named function and data declarations with `Import`, `Local` and `Export` linkage
- Data objects described by `DataDescription`: byte contents or zero fill, alignment, and pointers
  to functions or other data written in at link time
- A `Module` interface implemented by output modules

**xiao-jit**
- Declare, define, finalise and call functions at runtime
- Host symbol registration, with fallback lookup in the executable and the C runtime
- Code memory that is writable until finalisation and executable afterwards
- Read-only data protected at finalisation, writable data that stays writable
- Code and data allocated from one reserved address range, so RIP-relative references always fit

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

### Data and strings

Strings and other constant or global data live in data objects, as in Cranelift. A data object is
declared and defined on the module, and a function reads its address with `symbol_value`:

```cpp
const auto hello_id = *jit.declare_data( "hello", module::Linkage::Local, false );

module::DataDescription desc;
desc.define( std::string_view { "hello, world" } );
if ( !jit.define_data( hello_id, desc ) ) return 1;

ir::IRBuilder b { *irm.get( fn_ref ) };
const auto    hello = jit.declare_data_in_func( hello_id, b );
b.switch_to_block( b.add_block( { } ) );

const auto text  = b.symbol_value( Type::ptr, hello );
const auto first = b.uextend( Type::i64, b.load( Type::i8, text ) );
```

Data in the same module is addressed with a RIP-relative `lea`. Writable data (`declare_data` with
`writable = true`) can be read and written by both compiled code and the host through
`get_finalised_data`. A `DataDescription` can also hold the addresses of functions and other data
objects, which are filled in when the module is finalised.

A larger example using every supported feature can be found in [`source/entry.cc`](source/entry.cc).
Run it with `ir`, `asm` or `all` to print the IR and machine code of every function.

## Architecture

1. Functions are built in the IR with `IRBuilder` and checked by `IRVerifier`.
2. `Context::compile` lowers each function to target instructions on virtual registers.
3. The register allocator assigns physical registers.
4. Emission produces `CompiledCode`: the machine code and its relocations.
5. An output module consumes the compiled code and the module's data objects. `xiao-jit` places
   them in memory, resolves their relocations and protects them.

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
| `include/xiao/regalloc` | Liveness, live ranges, coalescing, the backtracking allocator, move resolution and the checker |
| `include/xiao/module` | Function and data declarations, `DataDescription`, linkage and the `Module` interface |
| `include/xiao-jit` | JIT module, builder, and code and data memory |
| `include/xiao-disasm` | Optional disassembler for compiled code, built on Zydis |

## Status

xiao is in early development. The following are not yet supported:

- Targets other than x86-64. Windows x64 is tested; System V is implemented but untested.
- Floating point types.
- Arguments passed on the stack.
- Indirect calls through function pointers.
- Ahead of time compilation and object file output.
- Thread-local data and unwind information.

## Acknowledgements

xiao's design is based on [Cranelift](https://github.com/bytecodealliance/wasmtime/tree/main/cranelift).

## License

xiao is available under the [MIT License](LICENSE).
