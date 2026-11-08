#[derive(Debug, Clone)]
pub struct Flags {
    pub opt: OptLevel,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum OptLevel {
    #[default]
    None,
    O1,
}

impl Default for Flags {
    fn default() -> Self {
        Self { opt: OptLevel::default() }
    }
}