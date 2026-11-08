use crate::{
    block::Block,
    function::Function,
    insts::{Inst, Opcode, Value, StackSlot},
    types::Type
};
use crate::insts::{BlockCall, IntCC};

macro_rules! binop {
    ($name:ident, $opcode:ident) => {
        pub fn $name(&mut self, a: Value, b: Value) -> Value {
            let ty = self.func.value_type(a);
            self.emit(Opcode::$opcode, &[a, b], ty)
        }
    };
}

pub trait IntoConst {
    fn ty(&self) -> Type;
    fn as_i64(self) -> i64;
}

impl IntoConst for i32 {
    fn ty(&self) -> Type { Type::I32 }
    fn as_i64(self) -> i64 { self as i64 }
}

impl IntoConst for i64 {
    fn ty(&self) -> Type { Type::I64 }
    fn as_i64(self) -> i64 { self }
}

pub struct IRBuilder<'a> {
    pub func: &'a mut Function,
    pub current: Block,
}

impl<'a> IRBuilder<'a> {
    pub fn new(func: &'a mut Function) -> Self {
        let entry = func.entry;
        Self { func, current: entry }
    }

    pub fn switch(&mut self, block: Block) {
        self.current = block;
    }

    pub fn iconst(&mut self, val: impl IntoConst) -> Value {
        let ty = val.ty();
        let cref = self.func.alloc_const(val.as_i64());
        self.emit(Opcode::Iconst(cref), &[], ty)
    }

    binop!(iadd, Iadd);
    binop!(isub, Isub);
    binop!(imul, Imul);
    binop!(iand, And);
    binop!(ior, Or);
    binop!(ixor, Xor);
    binop!(srem, Srem);
    binop!(urem, Urem);
    binop!(sdiv, Sdiv);
    binop!(udiv, Udiv);

    pub fn sext(&mut self, v: Value, to: Type) -> Value  { self.emit(Opcode::Sext, &[v], to) }
    
    pub fn zext(&mut self, v: Value, to: Type) -> Value  { self.emit(Opcode::Zext, &[v], to) }
    
    pub fn trunc(&mut self, v: Value, to: Type) -> Value { self.emit(Opcode::Trunc, &[v], to) }

    pub fn stack_addr(&mut self, slot: StackSlot) -> Value { self.emit(Opcode::StackAddr(slot), &[], Type::I64) }

    pub fn icmp(&mut self, cc: IntCC, a: Value, b: Value) -> Value {
        self.emit(Opcode::Icmp(cc), &[a, b], Type::I8)
    }

    pub fn brif(
        &mut self,
        condition: Value,
        then: Block,
        then_args: &[Value],
        els: Block,
        else_args: &[Value],
    ) {
        self.emit_void(
            Opcode::Brif(
                condition,
                BlockCall { block: then, args: then_args.to_vec() },
                BlockCall { block: els, args: else_args.to_vec() },
            ),
            &[],
        );
    }

    pub fn ret(&mut self, val: Option<Value>) {
        self.emit_void(Opcode::Ret, val.as_slice());
    }

    fn emit(&mut self, op: Opcode, args: &[Value], ty: Type) -> Value {
        debug_assert!(!self.func.is_terminated(self.current));

        let res = self.func.alloc_value(ty);
        self.func.push(self.current, Inst::new(op, Vec::from(args), Some(res), ty));
        res
    }

    fn emit_void(&mut self, op: Opcode, args: &[Value]) {
        debug_assert!(!self.func.is_terminated(self.current));

        self.func.push(self.current, Inst::new(op, Vec::from(args), None, Type::Void));
    }
}