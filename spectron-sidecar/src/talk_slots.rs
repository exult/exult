//! Normalise Name / Job dialogue replies into short People-panel fields.

/// Strip speech marks and light punctuation wrappers.
fn strip_wrapping(s: &str) -> String {
    let mut t = s.trim();
    if t.len() >= 2 {
        let bytes = t.as_bytes();
        if (bytes[0] == b'"' && bytes[t.len() - 1] == b'"')
            || (bytes[0] == b'\'' && bytes[t.len() - 1] == b'\'')
        {
            t = &t[1..t.len() - 1];
        }
    }
    t.trim_matches(|c: char| c == '"' || c == '\'' || c == '*')
        .trim()
        .trim_end_matches('.')
        .trim()
        .to_string()
}

fn is_name_refusal(lower: &str) -> bool {
    lower.contains("not important")
        || lower.contains("none of thy business")
        || lower.contains("none of your business")
        || lower.contains("i will not say")
        || lower.contains("won't tell")
        || lower.contains("will not tell")
        || lower.contains("that is my affair")
        || lower == "unknown"
        || lower == "n/a"
}

/// Turn a raw Name-topic reply into a short personal name, or `None` if refused / unusable.
pub fn normalize_personal_name(raw: &str) -> Option<String> {
    let t = strip_wrapping(raw);
    if t.is_empty() {
        return None;
    }
    let lower = t.to_ascii_lowercase();
    if is_name_refusal(&lower) {
        return None;
    }

    // "My name is Finnigan." / "I am called Dell."
    for prefix in [
        "my name is ",
        "i am called ",
        "i am known as ",
        "they call me ",
        "call me ",
        "i'm ",
        "i am ",
    ] {
        if let Some(rest) = lower.strip_prefix(prefix) {
            // Re-slice from original casing using byte/char alignment on ASCII prefixes.
            let rest_orig = t.get(prefix.len()..).unwrap_or(rest).trim();
            let name = first_name_token(rest_orig);
            if !name.is_empty() && !is_name_refusal(&name.to_ascii_lowercase()) {
                // "I am Markus the trainer" → Markus; "I am the Mayor" → not a personal name.
                if name.eq_ignore_ascii_case("the") || name.eq_ignore_ascii_case("a") {
                    return None;
                }
                return Some(name);
            }
            return None;
        }
    }

    // Bare single-token name.
    if !t.contains(' ') && t.chars().all(|c| c.is_alphabetic() || c == '\'' || c == '-') {
        return Some(t);
    }

    // Otherwise keep a short cleaned line only if it looks like a name phrase (≤ 3 words).
    let words: Vec<&str> = t.split_whitespace().collect();
    if words.len() <= 3 && words.iter().all(|w| w.chars().all(|c| c.is_alphabetic() || c == '\'' || c == '-' || c == '.'))
    {
        let cleaned = words
            .iter()
            .map(|w| w.trim_end_matches('.'))
            .collect::<Vec<_>>()
            .join(" ");
        if !is_name_refusal(&cleaned.to_ascii_lowercase()) {
            return Some(cleaned);
        }
    }
    None
}

fn first_name_token(s: &str) -> String {
    let tok = s
        .split(|c: char| c == ',' || c == '.' || c == '!' || c == '?' || c == ';' || c == ':')
        .next()
        .unwrap_or(s)
        .trim();
    let first = tok.split_whitespace().next().unwrap_or("").trim_matches(|c: char| {
        c == '"' || c == '\'' || c == '*'
    });
    first.to_string()
}

/// Clean a Job-topic reply for display (strip quotes; light “I am …” compression).
pub fn normalize_job(raw: &str) -> Option<String> {
    let t = strip_wrapping(raw);
    if t.is_empty() {
        return None;
    }
    let lower = t.to_ascii_lowercase();
    if lower == "unknown" || lower == "n/a" || lower == "not asked" {
        return None;
    }

    for prefix in ["i am the ", "i'm the ", "i am a ", "i'm a ", "i am ", "i'm "] {
        if let Some(rest) = lower.strip_prefix(prefix) {
            let rest_orig = t.get(prefix.len()..).unwrap_or(rest).trim();
            // Cut at sentence end for a short role line.
            let cut = rest_orig
                .split_once('.')
                .map(|(a, _)| a.trim())
                .unwrap_or(rest_orig);
            if !cut.is_empty() {
                // Capitalise first letter for panel display.
                let mut chars = cut.chars();
                if let Some(f) = chars.next() {
                    return Some(format!("{}{}", f.to_uppercase(), chars.as_str()));
                }
            }
        }
    }
    Some(t)
}

/// People-panel heading when no personal name is known.
pub fn unknown_role_label(appears_as: &str) -> String {
    let role = appears_as
        .trim()
        .strip_prefix("a ")
        .or_else(|| appears_as.trim().strip_prefix("an "))
        .or_else(|| appears_as.trim().strip_prefix("A "))
        .or_else(|| appears_as.trim().strip_prefix("An "))
        .unwrap_or(appears_as.trim());
    if role.is_empty() {
        "Unknown person".into()
    } else {
        format!("Unknown {role}")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn extracts_finnigan() {
        assert_eq!(
            normalize_personal_name("\"My name is Finnigan.\"").as_deref(),
            Some("Finnigan")
        );
        assert_eq!(
            normalize_personal_name("My name is Dell. Did I not say that already?").as_deref(),
            Some("Dell")
        );
    }

    #[test]
    fn rejects_not_important() {
        assert_eq!(normalize_personal_name("\"My name is not important.\""), None);
        assert_eq!(normalize_personal_name("My name is not important."), None);
    }

    #[test]
    fn job_mayor_short() {
        let j = normalize_job(
            "\"I am the Mayor of Trinsic and have been since I arrived here three years ago.\"",
        )
        .unwrap();
        assert!(j.to_ascii_lowercase().starts_with("mayor of trinsic"));
    }

    #[test]
    fn unknown_paladin() {
        assert_eq!(unknown_role_label("a paladin"), "Unknown paladin");
        assert_eq!(unknown_role_label("paladin"), "Unknown paladin");
    }
}
