//! Ultima VII Black Gate copy-protection Q&A (Finnigan / Batlin).
//!
//! Answers come from the bundled manuals / cloth map — mirrored in
//! `docs/copy-protection.md`. Matching is local and deterministic so the
//! Avatar's mind can prompt mid-conversation without waiting on Spectron
//! retrieval.

#[derive(Debug, Clone, Copy)]
pub struct CopyProtectHit {
    pub answer: &'static str,
    /// Short cue for the thought ("cloth map" / "Book of Archaic Knowledge").
    pub source: &'static str,
}

/// Fingerprints are normalised question tails (after strip); used for dedupe.
pub fn match_question(spoken: &str) -> Option<(&'static str, CopyProtectHit)> {
    let norm = normalize(spoken);
    if norm.len() < 24 {
        return None;
    }
    for (needle, hit) in QUESTIONS {
        if norm.contains(needle) {
            return Some((*needle, *hit));
        }
    }
    None
}

fn normalize(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for ch in s.chars() {
        let c = match ch {
            '\'' | '’' | '`' => continue,
            '—' | '–' | '-' => {
                out.push(' ');
                continue;
            }
            other => other.to_ascii_lowercase(),
        };
        if c.is_ascii_alphanumeric() || c.is_whitespace() {
            out.push(c);
        } else if c == '?' || c == '!' || c == '.' || c == ',' || c == '"' {
            out.push(' ');
        }
    }
    out.split_whitespace().collect::<Vec<_>>().join(" ")
}

const QUESTIONS: &[(&str, CopyProtectHit)] = &[
    // Finnigan — cloth map
    (
        "longitude runs through the center of the island buccaneers den",
        CopyProtectHit {
            answer: "60",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "longitude runs through the centre of the island buccaneers den",
        CopyProtectHit {
            answer: "60",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "latitude of the northernmost point of the island spektran",
        CopyProtectHit {
            answer: "120",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "longitude runs through the center of the island terfin",
        CopyProtectHit {
            answer: "120",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "longitude runs through the centre of the island terfin",
        CopyProtectHit {
            answer: "120",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "latitude runs through the center of dagger isle",
        CopyProtectHit {
            answer: "0",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "latitude runs through the centre of dagger isle",
        CopyProtectHit {
            answer: "0",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "latitude runs through the center of skara brae",
        CopyProtectHit {
            answer: "30",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "latitude runs through the centre of skara brae",
        CopyProtectHit {
            answer: "30",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "latitude runs through the center of the deep forest",
        CopyProtectHit {
            answer: "60",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "latitude runs through the centre of the deep forest",
        CopyProtectHit {
            answer: "60",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "latitude runs through the center of buccaneers den",
        CopyProtectHit {
            answer: "60",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "latitude runs through the centre of buccaneers den",
        CopyProtectHit {
            answer: "60",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "longitude runs through the center of skara brae",
        CopyProtectHit {
            answer: "60",
            source: "the cloth map of Britannia",
        },
    ),
    (
        "longitude runs through the centre of skara brae",
        CopyProtectHit {
            answer: "60",
            source: "the cloth map of Britannia",
        },
    ),
    // Batlin — manuals
    (
        "how many times must ginseng be reboiled",
        CopyProtectHit {
            answer: "40",
            source: "the Book of Archaic Knowledge",
        },
    ),
    (
        "how many runes are in the archaic script",
        CopyProtectHit {
            answer: "31",
            source: "the Book of Archaic Knowledge",
        },
    ),
    (
        "how many places may the mandrake root naturally be found",
        CopyProtectHit {
            answer: "2",
            source: "the Book of Archaic Knowledge",
        },
    ),
    (
        "how many bandits can be seen surrounding the old man",
        CopyProtectHit {
            answer: "6",
            source: "the Book of Fellowship",
        },
    ),
    (
        "how many parts of the body should one wish to protect with armour",
        CopyProtectHit {
            answer: "6",
            source: "the Traveller's Companion",
        },
    ),
    (
        "how many parts of the body should one wish to protect with armor",
        CopyProtectHit {
            answer: "6",
            source: "the Traveller's Companion",
        },
    ),
    (
        "fewer than how many pearls in 10000 are black",
        CopyProtectHit {
            answer: "1",
            source: "the Book of Archaic Knowledge",
        },
    ),
    (
        "fewer than how many pearls in 10 000 are black",
        CopyProtectHit {
            answer: "1",
            source: "the Book of Archaic Knowledge",
        },
    ),
    (
        "on what page of the book of archaic knowledge is the spell known as an zu",
        CopyProtectHit {
            answer: "42",
            source: "the Book of Archaic Knowledge",
        },
    ),
];

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn matches_buccaneers_den_longitude() {
        let line = "What longitude runs through the center of the island Buccaneer's Den?";
        let (key, hit) = match_question(line).expect("match");
        assert!(key.contains("buccaneers den"));
        assert_eq!(hit.answer, "60");
    }

    #[test]
    fn matches_british_centre_spelling() {
        let line = "What longitude runs through the centre of the island Buccaneer's Den?";
        assert_eq!(match_question(line).unwrap().1.answer, "60");
    }

    #[test]
    fn ignores_ordinary_talk() {
        assert!(match_question("Christopher's son is called Spark.").is_none());
    }
}
