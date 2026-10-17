//! Plain-text data files: `Texte.dat` and the keyed files in `Data/`
//! (`BGruppen.dat`, `Wachstum.dat`, `wegspeed.dat`, `Felder.dat`, …).
//!
//! Spec and evidence: `docs/formats/data-text.md`. Summary (P = checked
//! over the GOG 2.0.0.5 files, `crates/formats/tests/text_corpus.rs`):
//!
//! - Bytes are Windows-1252 text; lines end with CR LF (no lone CR or LF in
//!   any file). This module works on raw bytes and leaves decoding to the
//!   caller.
//! - **Keyed files:** a line is blank, a comment (`;` to the end of the
//!   line; never inside a `"…"` string in the corpus), a section header
//!   `[NAME]`, or an entry `Key: value` (`Key` = `[A-Za-z_][A-Za-z0-9_]*`,
//!   spaces or tabs allowed before the colon). Values are comma-separated
//!   fields ([`fields`]). Lines that are none of these are kept as
//!   [`KeyedKind::Other`] (one in `Felder.dat`, three in the tool script
//!   `font.dat`).
//! - **`Texte.dat`:** section headers `[NAME]` (names may contain `:`),
//!   each followed by its text lines verbatim; no comments. In `[ANLAGEN]`
//!   a line is `<id>:<name>` with an `Anlagen.dat` entry id ([`id_text`]).
//!
//! Round trip: [`write_lines`] rebuilds a file byte-identically from
//! [`lines`]; the structured views keep a reference to their line.

/// One line without its terminator.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Line<'a> {
    /// 1-based line number.
    pub number: usize,
    pub text: &'a [u8],
    /// Terminator: `\r\n`, `\n`, or none (last line).
    pub ending: Ending,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Ending {
    CrLf,
    Lf,
    None,
}

/// Splits `buf` into lines at `\n` (a preceding `\r` belongs to the
/// terminator). A file that ends with a terminator yields a last, empty
/// line without one, so that [`write_lines`] restores it exactly.
pub fn lines(buf: &[u8]) -> Vec<Line<'_>> {
    let mut out = Vec::new();
    let mut rest = buf;
    while let Some(i) = rest.iter().position(|&b| b == b'\n') {
        let (text, ending) = match rest[..i].strip_suffix(b"\r") {
            Some(t) => (t, Ending::CrLf),
            None => (&rest[..i], Ending::Lf),
        };
        out.push(Line {
            number: out.len() + 1,
            text,
            ending,
        });
        rest = &rest[i + 1..];
    }
    out.push(Line {
        number: out.len() + 1,
        text: rest,
        ending: Ending::None,
    });
    out
}

/// Joins lines with their terminators.
pub fn write_lines(lines: &[Line<'_>]) -> Vec<u8> {
    let mut out = Vec::new();
    for l in lines {
        out.extend_from_slice(l.text);
        out.extend_from_slice(match l.ending {
            Ending::CrLf => b"\r\n",
            Ending::Lf => b"\n",
            Ending::None => b"",
        });
    }
    out
}

fn trim(mut s: &[u8]) -> &[u8] {
    while let [b' ' | b'\t', rest @ ..] = s {
        s = rest;
    }
    while let [rest @ .., b' ' | b'\t'] = s {
        s = rest;
    }
    s
}

/// The text before a `;` comment (a `;` inside `"…"` does not start one).
pub fn strip_comment(text: &[u8]) -> &[u8] {
    let mut quoted = false;
    for (i, &b) in text.iter().enumerate() {
        match b {
            b'"' => quoted = !quoted,
            b';' if !quoted => return &text[..i],
            _ => {}
        }
    }
    text
}

/// What a line of a keyed file holds.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeyedKind<'a> {
    /// Empty, whitespace or comment only.
    Blank,
    /// `[NAME]`: the name between the brackets.
    Section(&'a [u8]),
    /// `Key: value`: the key and the value with surrounding whitespace and
    /// any comment removed.
    Entry { key: &'a [u8], value: &'a [u8] },
    /// Anything else (kept, not interpreted).
    Other(&'a [u8]),
}

/// Classifies one line of a keyed file.
pub fn keyed_kind(text: &[u8]) -> KeyedKind<'_> {
    let code = trim(strip_comment(text));
    if code.is_empty() {
        return KeyedKind::Blank;
    }
    if let Some(name) = code.strip_prefix(b"[").and_then(|c| c.strip_suffix(b"]"))
        && !name.is_empty()
        && !name.contains(&b']')
    {
        return KeyedKind::Section(name);
    }
    let key_len = code
        .iter()
        .position(|&b| !(b.is_ascii_alphanumeric() || b == b'_'))
        .unwrap_or(code.len());
    let starts_ok = code
        .first()
        .is_some_and(|&b| b.is_ascii_alphabetic() || b == b'_');
    if starts_ok && let Some(rest) = trim_start(&code[key_len..]).strip_prefix(b":") {
        return KeyedKind::Entry {
            key: &code[..key_len],
            value: trim(rest),
        };
    }
    KeyedKind::Other(code)
}

fn trim_start(mut s: &[u8]) -> &[u8] {
    while let [b' ' | b'\t', rest @ ..] = s {
        s = rest;
    }
    s
}

/// The lines of a keyed file with their kinds.
pub fn keyed(buf: &[u8]) -> Vec<(Line<'_>, KeyedKind<'_>)> {
    lines(buf)
        .into_iter()
        .map(|l| (l, keyed_kind(l.text)))
        .collect()
}

/// Splits a value at commas outside `"…"`; each field trimmed. An empty
/// value has no fields.
pub fn fields(value: &[u8]) -> Vec<&[u8]> {
    if value.is_empty() {
        return Vec::new();
    }
    let mut out = Vec::new();
    let (mut start, mut quoted) = (0, false);
    for (i, &b) in value.iter().enumerate() {
        match b {
            b'"' => quoted = !quoted,
            b',' if !quoted => {
                out.push(trim(&value[start..i]));
                start = i + 1;
            }
            _ => {}
        }
    }
    out.push(trim(&value[start..]));
    out
}

/// A field as a decimal integer (optional sign).
pub fn int(field: &[u8]) -> Option<i64> {
    std::str::from_utf8(field).ok()?.parse().ok()
}

/// A `"…"` field without its quotes.
pub fn quoted(field: &[u8]) -> Option<&[u8]> {
    field.strip_prefix(b"\"")?.strip_suffix(b"\"")
}

/// A section of `Texte.dat`: header line and the text lines up to the next
/// header.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TextSection<'a> {
    pub name: &'a [u8],
    /// Line number of the header.
    pub line: usize,
    pub lines: Vec<&'a [u8]>,
}

/// `Texte.dat`: lines before the first header (none in the corpus) and the
/// sections in file order. The empty last line after the final terminator
/// is not a text line.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Texte<'a> {
    pub preamble: Vec<&'a [u8]>,
    pub sections: Vec<TextSection<'a>>,
}

impl<'a> Texte<'a> {
    pub fn parse(buf: &'a [u8]) -> Self {
        let mut all = lines(buf);
        if all
            .last()
            .is_some_and(|l| l.text.is_empty() && l.ending == Ending::None)
        {
            all.pop();
        }
        let mut out = Texte {
            preamble: Vec::new(),
            sections: Vec::new(),
        };
        for l in all {
            let header = l
                .text
                .strip_prefix(b"[")
                .and_then(|t| t.strip_suffix(b"]"))
                .filter(|n| !n.is_empty() && !n.contains(&b']'));
            match (header, out.sections.last_mut()) {
                (Some(name), _) => out.sections.push(TextSection {
                    name,
                    line: l.number,
                    lines: Vec::new(),
                }),
                (None, Some(s)) => s.lines.push(l.text),
                (None, None) => out.preamble.push(l.text),
            }
        }
        out
    }

    /// The first section called `name`.
    pub fn section(&self, name: &str) -> Option<&TextSection<'a>> {
        self.sections.iter().find(|s| s.name == name.as_bytes())
    }
}

/// `<id>:<text>` (decimal id), as in `Texte.dat` `[ANLAGEN]`.
pub fn id_text(line: &[u8]) -> Option<(u32, &[u8])> {
    let colon = line.iter().position(|&b| b == b':')?;
    let id = &line[..colon];
    if id.is_empty() || !id.iter().all(u8::is_ascii_digit) {
        return None;
    }
    Some((
        std::str::from_utf8(id).ok()?.parse().ok()?,
        &line[colon + 1..],
    ))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lines_round_trip() {
        for buf in [&b"a\r\nb\nc"[..], b"a\r\n", b"", b"\r\n\r\n", b"x\ry\r\n"] {
            let ls = lines(buf);
            assert_eq!(write_lines(&ls), buf, "{buf:?}");
        }
        let ls = lines(b"a\r\nb\n");
        assert_eq!(
            ls.iter()
                .map(|l| (l.number, l.text, l.ending))
                .collect::<Vec<_>>(),
            [
                (1, &b"a"[..], Ending::CrLf),
                (2, b"b", Ending::Lf),
                (3, b"", Ending::None)
            ]
        );
    }

    #[test]
    fn keyed_lines() {
        assert_eq!(keyed_kind(b"  ; only a comment"), KeyedKind::Blank);
        assert_eq!(keyed_kind(b"\t"), KeyedKind::Blank);
        assert_eq!(keyed_kind(b"[WAREN]  ; x"), KeyedKind::Section(b"WAREN"));
        assert_eq!(
            keyed_kind(b"  Id:\t\tEISENERZ"),
            KeyedKind::Entry {
                key: b"Id",
                value: b"EISENERZ"
            }
        );
        assert_eq!(
            keyed_kind(b"Mask: 11111111    ; Alles eben"),
            KeyedKind::Entry {
                key: b"Mask",
                value: b"11111111"
            }
        );
        assert_eq!(
            keyed_kind(b"Name :  \"a;b\" ; c"),
            KeyedKind::Entry {
                key: b"Name",
                value: b"\"a;b\""
            }
        );
        assert_eq!(
            keyed_kind(b"Pos     0, 0"),
            KeyedKind::Other(b"Pos     0, 0")
        );
        assert_eq!(keyed_kind(b"EndObj;"), KeyedKind::Other(b"EndObj"));
        assert_eq!(keyed_kind(b"1x: y"), KeyedKind::Other(b"1x: y"));
    }

    #[test]
    fn value_fields() {
        assert_eq!(
            fields(b"100,  90 , \"a,b\", -3"),
            [&b"100"[..], b"90", b"\"a,b\"", b"-3"]
        );
        assert!(fields(b"").is_empty());
        assert_eq!(int(b"-3"), Some(-3));
        assert_eq!(int(b"0x3"), None);
        assert_eq!(quoted(b"\"a,b\""), Some(&b"a,b"[..]));
        assert_eq!(quoted(b"a"), None);
    }

    #[test]
    fn texte_sections() {
        let t = Texte::parse(b"[ANLAGEN]\r\n104:Forest\r\n501:Pioneer house\r\n[STARTUP:SPLASH]\r\nProfile: %s\r\n\r\n[END]\r\n");
        assert!(t.preamble.is_empty());
        let names: Vec<_> = t
            .sections
            .iter()
            .map(|s| (s.name, s.line, s.lines.len()))
            .collect();
        assert_eq!(
            names,
            [
                (&b"ANLAGEN"[..], 1, 2),
                (b"STARTUP:SPLASH", 4, 2),
                (b"END", 7, 0)
            ]
        );
        let anl = t.section("ANLAGEN").unwrap();
        assert_eq!(id_text(anl.lines[1]), Some((501, &b"Pioneer house"[..])));
        assert_eq!(id_text(b"Profile: %s"), None);
        assert_eq!(id_text(b":x"), None);
    }
}
