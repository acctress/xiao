use iced_x86::{Decoder, DecoderOptions, Formatter, IntelFormatter};
use std::fmt::Write;

pub fn dis(bytes: &[u8]) -> String {
    let mut dec = Decoder::with_ip(64, bytes, 0, DecoderOptions::NONE);
    let mut fmt = IntelFormatter::new();

    fmt.options_mut().set_hex_prefix("0x");
    fmt.options_mut().set_hex_suffix("");

    let (mut out, mut txt) = (String::new(), String::new());
    while dec.can_decode() {
        let ins = dec.decode();

        txt.clear();
        fmt.format(&ins, &mut txt);

        let at = ins.ip() as usize;
        let hex: String = bytes[at..at+ins.len()].iter().map(|b| format!("{b:02x} ")).collect();

        writeln!(out, "{at:04x}  {hex:<24}{txt}").unwrap();
    }
    
    out
}