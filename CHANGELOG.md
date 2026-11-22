# Changelog

All notable changes to xiao are listed in this file. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and xiao follows
[Semantic Versioning](https://semver.org). See [API stability](README.md#api-stability) for what
counts as a breaking change.

## [Unreleased]

### Added

- `icmp` on `i8`, `i16` and `i32` operands, compared at their own width
- `ushr` and `sshr` on `i8`, `i16` and `i32` values
- `rotl` and `rotr`, with the count masked to the value's width
- `bswap` on `i16`, `i32` and `i64`
- `clz`, `ctz` and `popcnt`. they use `lzcnt`, `tzcnt` and `popcnt` when `Flags::has_lzcnt`,
  `has_bmi1` and `has_popcnt` are set, and portable sequences otherwise
- `umulhi` and `smulhi`, the high half of a double width multiply
- `trap`, a terminator, and `trapz` / `trapnz`, which stop the program with a `TrapCode`. each
  trap site is recorded in `CompiledCode::traps` with its offset and code

### Changed

- Shifts of narrow values shift at the value's width and mask the count to `bits - 1`. A narrow
  `ishl` by its width or more used to shift as 64 bits.

## [0.1.0] - 2026-11-22

The first release of xiao. It compiles integer code correctly for x86-64 on Windows x64 and
System V, and input it cannot compile returns a `CodegenError` instead of producing wrong code.

### Added

**Intermediate representation**
- SSA form with block parameters, built with `IRBuilder` and printed as text
- Integer types `i8`, `i16`, `i32`, `i64` and `ptr`
- Arithmetic, bitwise operations, shifts, conversions, `icmp` and `select`
- Control flow with `br`, `brif` and `ret`, and memory access with `load`, `store` and stack slots
- Direct, external and indirect calls, global values and `symbol_value`
- `IRVerifier`, which checks types, returns, terminator placement, value ids and that every value
  is defined before it is used

**Code generation**
- `codegen::Context`, which verifies a function and then compiles it
- `codegen::Flags` to turn the verifier and stack probes on or off, both on by default
- `CodegenError`, with the kinds `Verifier`, `Unsupported`, `ImplLimitExceeded` and
  `RegallocFailed`

**x86-64 backend**
- Lowering to machine instructions and backtracking register allocation, with a symbolic checker
  in debug builds
- Prologue and epilogue generation with stack alignment and an outgoing argument area
- Windows x64 and System V calling conventions, including arguments passed on the stack
- Inline stack probes for frames larger than a page

**Modules**
- A `Module` interface with function and data declarations, `Import`, `Local` and `Export`
  linkage, and `DataDescription`
- `xiao-jit`, which compiles functions into memory and calls them at runtime, with host symbol
  lookup and code that is writable until finalisation and executable afterwards
- `xiao-object`, which writes COFF object files, including sections with more than 65535
  relocations

**Packaging**
- A CMake package: `find_package(xiao 0.1)`, `add_subdirectory` and `FetchContent` all provide
  `xiao::xiao`
- `xiao/version.hh`, which CMake reads for the project version
- CI on Windows with Clang and Linux with GCC 14, in Debug and Release

### Known limitations

- Floating point types are rejected with `CodegenError::Unsupported`. They are planned for 0.2.0.
- Varargs, `br_table`, atomics and traps are not supported yet.
- `icmp`, the division operations, `ushr` and `sshr` need `i64` operands. Narrow values have to be
  extended first.
- The verifier rejects a `windows_fastcall` function that calls a `system_v` function.
- Object files are COFF only. ELF and Mach-O are not supported.
- There is no thread-local data or unwind information.
- x86-64 is the only target.

[Unreleased]: https://github.com/acctress/xiao/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/acctress/xiao/releases/tag/v0.1.0
