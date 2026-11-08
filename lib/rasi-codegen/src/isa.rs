use crate::cc::CompiledCode;
use rasi_ir::function::Function;
use std::fmt;

pub trait TargetIsa: fmt::Display + Send + Sync {
    fn compile(&self, func: &Function) -> CompiledCode;
}