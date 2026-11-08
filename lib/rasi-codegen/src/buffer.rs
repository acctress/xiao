#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Label(pub u32);

#[derive(Clone, Copy, Debug)]
pub enum FixupKind { Rel32 }

struct Fixup { at: u32, label: Label, kind: FixupKind, trailing: u8 }

#[derive(Default)]
pub struct Buffer {
    bytes: Vec<u8>,
    labels: Vec<Option<u32>>,
    fixups: Vec<Fixup>,
}

impl Buffer {
    pub fn new() -> Self { Self::default() }

    pub fn len(&self) -> usize { self.bytes.len() }
    pub fn is_empty(&self) -> bool { self.bytes.is_empty() }
    pub fn put(&mut self, b: &[u8]) { self.bytes.extend_from_slice(b); }
    pub fn put_u8(&mut self, b: u8) { self.bytes.push(b); }

    pub fn new_label(&mut self) -> Label {
        self.labels.push(None);
        Label(self.labels.len() as u32 - 1)
    }

    pub fn bind(&mut self, l: Label) {
        debug_assert!(self.labels[l.0 as usize].is_none(), "label bound twice");
        self.labels[l.0 as usize] = Some(self.bytes.len() as u32);
    }

    pub fn use_label(&mut self, label: Label, kind: FixupKind) {
        self.fixups.push(Fixup { at: self.bytes.len() as u32, label, kind, trailing: 0 });
        match kind {
            FixupKind::Rel32 => self.put(&[0; 4]),
        }
    }

    pub fn finish(mut self) -> Vec<u8> {
        for f in &self.fixups {
            let target = self.labels[f.label.0 as usize].expect("unbound label") as i64;
            let at = f.at as usize;
            match f.kind {
                FixupKind::Rel32 => {
                    let end = at as i64 + 4 + f.trailing as i64;
                    let rel = i32::try_from(target - end).expect("rel32 out of range");
                    self.bytes[at..at + 4].copy_from_slice(&rel.to_le_bytes());
                }
            }
        }

        self.bytes
    }
}