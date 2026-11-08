use std::fmt;
use std::fmt::{Debug, Formatter};
use crate::{block::{Block, BasicBlock}, insts::{Value, ConstRef}, types::Type};
use crate::insts::{FuncRef, Inst, IntCC, Opcode, StackSlot};

#[derive(Debug, Clone, Copy)]
pub struct StackSlotData { pub size: u32, pub align: u32 }

#[derive(Debug)]
pub struct Function {
    pub name: String,
    pub params: Vec<Type>,
    pub ret: Type,
    pub blocks: Vec<BasicBlock>,
    pub entry: Block,
    pub constants: Vec<i64>,
    pub value_types: Vec<Type>,
    pub stack_slots: Vec<StackSlotData>,
}

impl Function {
    pub fn new(name: impl Into<String>, params: &[Type], ret: Type) -> Self {
        let mut fun = Self {
            name: name.into(),
            params: Vec::from(params),
            ret,
            blocks: vec![],
            entry: Block(0),
            constants: vec![],
            value_types: vec![],
            stack_slots: vec![],
        };

        let entry = fun.alloc_block();
        for ty in params {
            fun.append_block_param(entry, *ty);
        }

        fun
    }

    pub fn value_type(&self, val: Value) -> Type {
        self.value_types[val.0 as usize]
    }

    pub fn alloc_block( &mut self ) -> Block {
        let blk = Block(self.blocks.len() as u32);
        self.blocks.push(BasicBlock::new());
        blk
    }
    pub fn alloc_const(&mut self, val: i64) -> ConstRef {
        let c = ConstRef(self.constants.len() as u32);
        self.constants.push(val);

        c
    }

    pub fn alloc_value(&mut self, ty: Type) -> Value {
        let v = Value(self.value_types.len() as u32);
        self.value_types.push(ty);
        v
    }

    pub fn block(&self, b: Block) -> &BasicBlock { &self.blocks[b.0 as usize] }

    pub fn block_mut(&mut self, b: Block) -> &mut BasicBlock { &mut self.blocks[b.0 as usize] }

    pub fn append_block_param(&mut self, b: Block, ty: Type) -> Value {
        let v = self.alloc_value(ty);
        self.block_mut(b).params.push(v);
        v
    }

    pub fn arg_values(&self) -> &[Value] {
        &self.block(self.entry).params
    }

    pub fn push(&mut self, b: Block, inst: Inst) {
        self.block_mut(b).insts.push(inst);
    }

    pub fn is_terminated(&self, b: Block) -> bool {
        self.block(b).insts.last().is_some_and(Inst::is_terminator)
    }
    
    pub fn constant(&self, cref: ConstRef) -> i64 {
        self.constants[cref.0 as usize]
    }

    pub fn create_stack_slot(&mut self, size: u32, align: u32) -> StackSlot {
        let s = StackSlot(self.stack_slots.len() as u32);
        self.stack_slots.push(StackSlotData { size, align });

        s
    }
}

impl fmt::Display for Value { fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result { write!(f, "%{}", self.0) } }
impl fmt::Display for Block { fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result { write!(f, "block{}", self.0) } }
impl fmt::Display for FuncRef { fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result { write!(f, "@fn{}", self.0) } }
impl fmt::Display for Type { fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result { write!(f, "{}", format!("{self:?}").to_lowercase()) }}
impl fmt::Display for IntCC { fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result { write!(f, "{}", format!("{self:?}").to_lowercase()) }}

fn write_list<T: fmt::Display>(f: &mut Formatter, items: &[T]) -> fmt::Result {
    for (i, it) in items.iter().enumerate() {
        if i > 0 { write!(f, ", ")?; }
        write!(f, "{it}")?;
    }

    Ok(())
}

fn write_addr(f: &mut Formatter, base: Value, offset: i32) -> fmt::Result {
    match offset {
        0 => write!(f, "[{base}]"),
        o if o < 0 => write!(f, "[{base} - {}]", -(o as i64)),
        o => write!(f, "[{base} + {o}]"),
    }
}

impl fmt::Display for Function {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "define @{}(", self.name)?;
        write_list(f, &self.params)?;
        writeln!(f, ") -> {} {{", self.ret)?;

        for (i, blk) in self.blocks.iter().enumerate() {
            write!(f, "block{i}")?;

            if !blk.params.is_empty() {
                write!(f, "(")?;
                for (j, &p) in blk.params.iter().enumerate() {
                    if j > 0 { write!(f, ", ")?; }
                    write!(f, "{p}: {}", self.value_type(p))?;
                }

                write!(f, ")")?;
            }

            writeln!(f, ":")?;
            for inst in &blk.insts {
                self.fmt_inst(f, inst)?;
                writeln!(f)?;
            }
        }

        writeln!(f, "}}")
    }
}

impl Function {
    fn fmt_inst(&self, f: &mut Formatter, i: &Inst) -> fmt::Result {
        write!(f, "    ")?;

        if let Some(r) = i.result { write!(f, "{r} = ")?; }
        let a = &i.args;
        let ty = |n: usize| self.value_type(a[n]);

        match &i.opcode {
            Opcode::Iconst(c) => write!(f, "iconst {} {}", i.ty, self.constants[c.0 as usize]),
            Opcode::Icmp(cc)  => write!(f, "icmp {cc} {} {}, {}", ty(0), a[0], a[1]),
            Opcode::Load(off) => { write!(f, "load {} ", i.ty)?; write_addr(f, a[0], *off) }
            Opcode::Store(off) => {
                write!(f, "store {} {}, ", ty(1), a[1])?;
                write_addr(f, a[0], *off)
            }
            
            Opcode::Call(fr) => {
                write!(f, "call {} {fr}(", i.ty)?;
                write_list(f, a)?;
                write!(f, ")")
            }

            Opcode::Br(bc) => {
                write!(f, "br {}", bc.block)?;
                if !bc.args.is_empty() {
                    write!(f, "(")?;
                    write_list(f, &bc.args)?;
                    write!(f, ")")?;
                }
                Ok(())
            }

            Opcode::Brif(cond, then, els) => {
                write!(f, "brif {cond}, {}", then.block)?;
                if !then.args.is_empty() {
                    write!(f, "(")?;
                    write_list(f, &then.args)?;
                    write!(f, ")")?;
                }
                write!(f, ", {}", els.block)?;
                if !els.args.is_empty() {
                    write!(f, "(")?;
                    write_list(f, &els.args)?;
                    write!(f, ")")?;
                }
                Ok(())
            }

            Opcode::Ret if a.is_empty() => write!(f, "ret void"),
            Opcode::Ret => write!(f, "ret {} {}", ty(0), a[0]),
            op => {
                write!(f, "{} {} ", op.name(), i.ty)?;
                write_list(f, a)
            }
        }
    }
}