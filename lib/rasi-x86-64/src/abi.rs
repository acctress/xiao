use rasi_codegen::AbiSpec;
use crate::*;

pub const SYSV: AbiSpec = AbiSpec {
    int_args:           &[RDI, RSI, RDX, RCX, R8, R9],
    int_return:         &[RAX],
    caller_saved_int:   &[RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11],
    callee_saved_int:   &[RBX, RBP, R12, R13, R14, R15],
    scratch:             [R10, R11],
    stack_alignment:    16,
    shadow_space:       0,

    float_args:         &[],
    float_return:       &[],
    caller_saved_float: &[],
    callee_saved_float: &[],
};

pub const WIN64: AbiSpec = AbiSpec {
    int_args:           &[RCX, RDX, R8, R9],
    int_return:         &[RAX],
    caller_saved_int:   &[RAX, RCX, RDX, R8, R9, R10, R11],
    callee_saved_int:   &[RBX, RBP, RDI, RSI, R12, R13, R14, R15],
    scratch:             [R10, R11],
    stack_alignment:    16,
    shadow_space:       32,

    float_args:         &[],
    float_return:       &[],
    caller_saved_float: &[],
    callee_saved_float: &[],
};

pub fn host_abi() -> &'static AbiSpec {
    #[cfg(windows)] { &WIN64 }
    #[cfg(unix)]    { &SYSV }
}