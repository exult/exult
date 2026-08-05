//! Book text hygiene for Exult usecode dumps.

/// Exult book glyphs often use `~~` as a soft line / section break.
pub fn normalize_book_text(raw: &str) -> String {
    raw.replace("~~", "\n")
        .lines()
        .map(str::trim)
        .filter(|line| !line.is_empty())
        .collect::<Vec<_>>()
        .join("\n")
}

/// Prefer an explicit title; otherwise take the first substantial heading-like line.
pub fn resolve_title(explicit: Option<&str>, body: &str) -> Option<String> {
    if let Some(t) = explicit.map(str::trim).filter(|t| !t.is_empty()) {
        return Some(t.to_string());
    }
    body.lines().find_map(|line| {
        let t = line.trim().trim_matches('*').trim();
        if t.len() >= 8 && t.chars().filter(|c| c.is_alphabetic()).count() >= 6 {
            Some(t.to_string())
        } else {
            None
        }
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn strips_tilde_breaks() {
        let raw = "~~ ~~VETRONS GUIDE~~Axe, two-handed: 10~~Bow: 8";
        let n = normalize_book_text(raw);
        assert!(n.contains("VETRONS GUIDE"));
        assert!(n.contains("Axe, two-handed: 10"));
        assert!(!n.contains('~'));
    }

    #[test]
    fn title_from_body() {
        let body = normalize_book_text(
            "~~ ~~VETRONS GUIDE TO WEAPONS AND ARMOUR~~ ~~Their effectiveness*",
        );
        let title = resolve_title(None, &body).unwrap();
        assert!(title.to_uppercase().contains("VETRON"));
    }
}
