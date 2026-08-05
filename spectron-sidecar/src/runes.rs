//! Britannian / Ultima rune encoding → Unicode Runic block.
//!
//! Ultima's Britannian alphabet is not in Unicode. We map it phonetically onto
//! the Unicode Runic block (Elder Futhark / Anglo-Saxon), which stores as real
//! characters and reads as "runes" even though the glyphs are not pixel-identical
//! to Exult's fonts.vga runes.
//!
//! Ligatures from Exult's usecode encoding (`Sign_gump`):
//! `(` TH, `)` EE, `*` NG, `+` EA, `,` ST, `|` space.

/// Phonetic map for A–Z → Unicode Runic.
fn letter_rune(ch: char) -> Option<char> {
    Some(match ch.to_ascii_uppercase() {
        'A' => '\u{16A8}', // ᚨ ANSUZ
        'B' => '\u{16D2}', // ᛒ BERKANAN
        'C' => '\u{16B3}', // ᚳ CEN
        'D' => '\u{16DE}', // ᛞ DAGAZ
        'E' => '\u{16D6}', // ᛖ EHWAZ
        'F' => '\u{16A0}', // ᚠ FEHU
        'G' => '\u{16B7}', // ᚷ GEBO
        'H' => '\u{16BB}', // ᚻ HAEGL
        'I' => '\u{16C1}', // ᛁ ISA
        'J' => '\u{16C3}', // ᛃ JERAN
        'K' => '\u{16B2}', // ᚲ KAUNA
        'L' => '\u{16DA}', // ᛚ LAGUZ
        'M' => '\u{16D7}', // ᛗ MANNAZ
        'N' => '\u{16BE}', // ᚾ NAUDIZ
        'O' => '\u{16DF}', // ᛟ OTHALAN
        'P' => '\u{16C8}', // ᛈ PERTHO
        'Q' => '\u{16E9}', // ᛩ (no historic Britannian Q)
        'R' => '\u{16B1}', // ᚱ RAIDO
        'S' => '\u{16CB}', // ᛋ SIGEL
        'T' => '\u{16CF}', // ᛏ TIWAZ
        'U' => '\u{16A2}', // ᚢ URUZ
        'V' => '\u{16A1}', // ᚡ V
        'W' => '\u{16B9}', // ᚹ WUNJO
        'X' => '\u{16EA}', // approx.
        'Y' => '\u{16A3}', // ᚣ YR
        'Z' => '\u{16E3}', // approx.
        _ => return None,
    })
}

/// Convert Exult usecode rune encoding (`text_raw`) to Unicode Runic.
pub fn raw_encoding_to_unicode(raw: &str) -> String {
    let mut out = String::with_capacity(raw.len() * 3);
    for ch in raw.chars() {
        match ch {
            '(' => out.push('\u{16A6}'), // ᚦ THURISAZ (TH)
            ')' => out.push('\u{16C7}'), // ᛇ EIHWAZ (EE digraph stand-in)
            '*' => out.push('\u{16DC}'), // ᛜ INGWAZ (NG)
            '+' => out.push('\u{16E0}'), // ᛠ EAR (EA) — Anglo-Saxon
            ',' => out.push('\u{16E5}'), // ᛥ STAN (ST) — Anglo-Saxon
            '|' | ' ' => out.push(' '),
            '\n' => out.push('\n'),
            c if c.is_ascii_alphabetic() => {
                if let Some(r) = letter_rune(c) {
                    out.push(r);
                }
            }
            // Keep digits / punctuation that sometimes appear on signs.
            c if c.is_ascii_digit() || c == '.' || c == '-' || c == '\'' => out.push(c),
            _ => {}
        }
    }
    out
}

/// Convert already-expanded Latin (with TH/EE/NG/EA/ST) to Unicode Runic.
pub fn latin_to_unicode(latin: &str) -> String {
    let upper = latin.to_ascii_uppercase();
    let bytes = upper.as_bytes();
    let mut out = String::with_capacity(bytes.len() * 3);
    let mut i = 0;
    while i < bytes.len() {
        let digraph = if i + 1 < bytes.len() {
            Some([bytes[i], bytes[i + 1]])
        } else {
            None
        };
        match digraph {
            Some([b'T', b'H']) => {
                out.push('\u{16A6}');
                i += 2;
            }
            Some([b'E', b'E']) => {
                out.push('\u{16C7}');
                i += 2;
            }
            Some([b'N', b'G']) => {
                out.push('\u{16DC}');
                i += 2;
            }
            Some([b'E', b'A']) => {
                out.push('\u{16E0}');
                i += 2;
            }
            Some([b'S', b'T']) => {
                out.push('\u{16E5}');
                i += 2;
            }
            _ => {
                let c = bytes[i] as char;
                if c == ' ' || c == '\n' {
                    out.push(c);
                } else if let Some(r) = letter_rune(c) {
                    out.push(r);
                } else if c.is_ascii_digit() || c == '.' || c == '-' || c == '\'' {
                    out.push(c);
                }
                i += 1;
            }
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ligatures_from_raw() {
        assert_eq!(raw_encoding_to_unicode("(e"), "ᚦᛖ");
        assert_eq!(raw_encoding_to_unicode("+,"), "ᛠᛥ");
    }

    #[test]
    fn erat_of_style() {
        // Example wood-sign style Latin.
        let u = latin_to_unicode("ERAT OF PRIER");
        assert!(u
            .chars()
            .all(|c| c == ' ' || ('\u{16A0}'..='\u{16FF}').contains(&c)));
        assert_eq!(u.chars().filter(|c| *c == ' ').count(), 2);
    }
}
