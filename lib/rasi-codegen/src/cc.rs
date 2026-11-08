pub struct Reloc {
    pub offset: u32,
}

pub struct CompiledCode {
    pub bytes: Vec<u8>,
    pub relocs: Vec<Reloc>
}