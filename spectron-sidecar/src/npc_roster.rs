//! Local NPC roster for testing how well Spectron summarises people.
//!
//! Cards are keyed primarily by Exult `npc_id` when known, else by appearance /
//! personal name. Spectron fills summaries via structured `/chat` replies.

use std::fmt::Write as _;

#[derive(Debug, Clone, Default)]
pub struct NpcCard {
    /// Exult schedule slot when known (`-1` / `0` = unknown).
    pub npc_id: i32,
    /// Shape / role label from the conversation face ("a paladin", "Spark").
    pub appears_as: String,
    /// Personal name if learned (or Spectron reports one).
    pub name: Option<String>,
    /// Job / role if learned.
    pub job: Option<String>,
    /// Spectron (or local stub) summary of what the Avatar knows.
    pub summary: String,
}

impl NpcCard {
    pub fn display_name(&self) -> String {
        if let Some(n) = self
            .name
            .as_deref()
            .map(str::trim)
            .filter(|s| !s.is_empty() && !is_unknown_token(s) && !looks_like_name_quote(s))
        {
            // Prefer cleaned personal name.
            if let Some(clean) = crate::talk_slots::normalize_personal_name(n) {
                return clean;
            }
            if !n.contains(' ') || n.split_whitespace().count() <= 2 {
                return n.to_string();
            }
        }
        crate::talk_slots::unknown_role_label(&self.appears_as)
    }

    pub fn merge_from(&mut self, other: &NpcCard) {
        if other.npc_id > 0 {
            self.npc_id = other.npc_id;
        }
        if !other.appears_as.is_empty() {
            // Prefer a more specific label only when we have nothing useful yet.
            if self.appears_as.is_empty()
                || (self.name.is_none() && other.appears_as.len() >= self.appears_as.len())
            {
                self.appears_as = other.appears_as.clone();
            }
        }
        // Never forget a known name/job because a later empty talk said "unknown".
        if let Some(n) = other
            .name
            .as_ref()
            .and_then(|s| crate::talk_slots::normalize_personal_name(s))
        {
            self.name = Some(n);
        }
        if let Some(j) = other
            .job
            .as_ref()
            .and_then(|s| crate::talk_slots::normalize_job(s))
        {
            self.job = Some(j);
        }
        let other_sum = other.summary.trim();
        if other_sum.is_empty() {
            return;
        }
        if self.summary.trim().is_empty() || card_richness(other) >= card_richness(self) {
            self.summary = other_sum.to_string();
        }
    }
}

fn looks_like_name_quote(s: &str) -> bool {
    let l = s.to_ascii_lowercase();
    l.contains("my name is") || l.contains("not important")
}

/// Rough signal so an empty Bye-dossier cannot wipe a filled People card.
fn card_richness(card: &NpcCard) -> u32 {
    let mut score = 0u32;
    if card
        .name
        .as_deref()
        .and_then(crate::talk_slots::normalize_personal_name)
        .is_some()
    {
        score += 8;
    }
    if card
        .job
        .as_deref()
        .and_then(crate::talk_slots::normalize_job)
        .is_some()
    {
        score += 8;
    }
    let sum = card.summary.trim();
    score += (sum.len() as u32).min(200) / 20;
    // Thin "I learned nothing" replies score low even if a bit long.
    let low = sum.to_ascii_lowercase();
    if low.contains("didn't learn")
        || low.contains("did not learn")
        || low.contains("nothing specific")
        || low.contains("wish i had asked")
    {
        score = score.saturating_sub(6);
    }
    score
}

fn is_unknown_token(s: &str) -> bool {
    let t = s.trim();
    t.is_empty()
        || t.eq_ignore_ascii_case("unknown")
        || t.eq_ignore_ascii_case("(unknown)")
        || t.eq_ignore_ascii_case("not asked")
        || t.eq_ignore_ascii_case("n/a")
        || t.eq_ignore_ascii_case("none")
}

fn norm_key(s: &str) -> String {
    s.trim().to_ascii_lowercase()
}

/// Upsert by `npc_id` when positive, else by name / appears_as.
pub fn upsert(roster: &mut Vec<NpcCard>, card: NpcCard) {
    let idx = roster.iter().position(|c| {
        (card.npc_id > 0 && c.npc_id == card.npc_id)
            || (!card.appears_as.is_empty()
                && norm_key(&c.appears_as) == norm_key(&card.appears_as))
            || card
                .name
                .as_ref()
                .filter(|n| !is_unknown_token(n))
                .is_some_and(|n| {
                    c.name
                        .as_ref()
                        .is_some_and(|cn| norm_key(cn) == norm_key(n))
                        || norm_key(&c.appears_as) == norm_key(n)
                })
    });
    match idx {
        Some(i) => roster[i].merge_from(&card),
        None => roster.insert(0, card),
    }
}

/// Parse one or more `===NPC===` … `===END===` blocks from a Spectron reply.
pub fn parse_roster_reply(text: &str) -> Vec<NpcCard> {
    let mut out = Vec::new();
    let mut in_block = false;
    let mut appears_as = String::new();
    let mut name: Option<String> = None;
    let mut job: Option<String> = None;
    let mut summary = String::new();
    let mut npc_id = 0i32;

    let flush = |appears_as: &mut String,
                 name: &mut Option<String>,
                 job: &mut Option<String>,
                 summary: &mut String,
                 npc_id: &mut i32,
                 out: &mut Vec<NpcCard>| {
        if appears_as.trim().is_empty() && name.as_ref().is_none_or(|n| is_unknown_token(n)) {
            appears_as.clear();
            *name = None;
            *job = None;
            summary.clear();
            *npc_id = 0;
            return;
        }
        if appears_as.trim().is_empty() {
            if let Some(n) = name.clone() {
                *appears_as = n;
            }
        }
        out.push(NpcCard {
            npc_id: *npc_id,
            appears_as: appears_as.trim().to_string(),
            name: name.take().filter(|s| !is_unknown_token(s)),
            job: job.take().filter(|s| !is_unknown_token(s)),
            summary: summary.trim().to_string(),
        });
        appears_as.clear();
        summary.clear();
        *npc_id = 0;
    };

    for raw in text.lines() {
        let line = raw.trim();
        if line.eq_ignore_ascii_case("===NPC===") || line.eq_ignore_ascii_case("---NPC---") {
            if in_block {
                flush(
                    &mut appears_as,
                    &mut name,
                    &mut job,
                    &mut summary,
                    &mut npc_id,
                    &mut out,
                );
            }
            in_block = true;
            continue;
        }
        if line.eq_ignore_ascii_case("===END===") || line.eq_ignore_ascii_case("---END---") {
            if in_block {
                flush(
                    &mut appears_as,
                    &mut name,
                    &mut job,
                    &mut summary,
                    &mut npc_id,
                    &mut out,
                );
            }
            in_block = false;
            continue;
        }
        if !in_block {
            // Tolerate a single card with no markers (dossier refresh).
            if let Some((k, v)) = split_field(line) {
                in_block = true;
                apply_field(k, v, &mut appears_as, &mut name, &mut job, &mut summary, &mut npc_id);
            }
            continue;
        }
        if let Some((k, v)) = split_field(line) {
            apply_field(k, v, &mut appears_as, &mut name, &mut job, &mut summary, &mut npc_id);
        } else if !line.is_empty() && summary.is_empty() && !line.contains(':') {
            // Bare continuation lines append to summary.
            if !summary.is_empty() {
                summary.push(' ');
            }
            summary.push_str(line);
        }
    }
    if in_block {
        flush(
            &mut appears_as,
            &mut name,
            &mut job,
            &mut summary,
            &mut npc_id,
            &mut out,
        );
    }
    out
}

fn split_field(line: &str) -> Option<(&str, &str)> {
    let (k, v) = line.split_once(':')?;
    let k = k.trim();
    let v = v.trim();
    if k.is_empty() {
        return None;
    }
    Some((k, v))
}

fn apply_field(
    key: &str,
    value: &str,
    appears_as: &mut String,
    name: &mut Option<String>,
    job: &mut Option<String>,
    summary: &mut String,
    npc_id: &mut i32,
) {
    let k = key.to_ascii_lowercase().replace('-', "_");
    match k.as_str() {
        "appeared_as" | "appears_as" | "appearance" | "role" => {
            *appears_as = value.to_string();
        }
        "name" | "personal_name" => {
            *name = crate::talk_slots::normalize_personal_name(value);
        }
        "job" | "occupation" => {
            *job = crate::talk_slots::normalize_job(value);
        }
        "knows" | "summary" | "about" => {
            *summary = value.to_string();
        }
        "npc_id" | "id" => {
            if let Ok(n) = value.trim().parse::<i32>() {
                *npc_id = n;
            }
        }
        _ => {}
    }
}

/// Human-readable card for the People panel.
pub fn format_card_lines(card: &NpcCard) -> String {
    let mut s = String::new();
    let heading = card.display_name();
    let _ = writeln!(s, "{heading}");
    let clean_name = card
        .name
        .as_deref()
        .and_then(crate::talk_slots::normalize_personal_name);
    if clean_name.is_some() {
        if heading != card.appears_as {
            let _ = writeln!(s, "appeared as: {}", card.appears_as);
        }
    } else {
        let _ = writeln!(s, "(personal name unknown)");
    }
    match card
        .job
        .as_deref()
        .and_then(crate::talk_slots::normalize_job)
    {
        Some(j) => {
            let _ = writeln!(s, "job: {j}");
        }
        None => {
            let _ = writeln!(s, "job: (unknown)");
        }
    }
    if !card.summary.is_empty() {
        let _ = write!(s, "{}", card.summary);
    } else {
        let _ = write!(s, "(waiting on Spectron…)");
    }
    s
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_marked_blocks() {
        let text = r#"
Here is everyone:

===NPC===
appeared_as: a paladin
name: unknown
job: keep villains out of Trinsic
knows: Guards the gate; password needed to leave.
===END===
===NPC===
appeared_as: Spark
name: Spark
job: unknown
knows: Boy with a sling who joined my party.
===END===
"#;
        let cards = parse_roster_reply(text);
        assert_eq!(cards.len(), 2);
        assert_eq!(cards[0].appears_as, "a paladin");
        assert!(cards[0].name.is_none());
        assert!(cards[0].job.as_deref().unwrap().contains("villains"));
        assert_eq!(cards[1].display_name(), "Spark");
    }

    #[test]
    fn parses_single_dossier_without_markers() {
        let text = "appeared_as: shopkeeper\nname: Gargan\njob: shipwright\nknows: Sells ships and sextants in Trinsic.\n";
        let cards = parse_roster_reply(text);
        assert_eq!(cards.len(), 1);
        assert_eq!(cards[0].name.as_deref(), Some("Gargan"));
        assert!(cards[0].summary.contains("sextants"));
    }

    #[test]
    fn upsert_merges_by_npc_id() {
        let mut roster = vec![NpcCard {
            npc_id: 21,
            appears_as: "shopkeeper".into(),
            name: None,
            job: None,
            summary: "stub".into(),
        }];
        upsert(
            &mut roster,
            NpcCard {
                npc_id: 21,
                appears_as: "shopkeeper".into(),
                name: Some("Gargan".into()),
                job: Some("shipwright".into()),
                summary: "Trinsic shipwright.".into(),
            },
        );
        assert_eq!(roster.len(), 1);
        assert_eq!(roster[0].name.as_deref(), Some("Gargan"));
        assert_eq!(roster[0].summary, "Trinsic shipwright.");
    }

    #[test]
    fn empty_dossier_does_not_wipe_known_name() {
        let mut roster = vec![NpcCard {
            npc_id: 10,
            appears_as: "shopkeeper".into(),
            name: Some("Dell".into()),
            job: Some("weapons and armour".into()),
            summary: "Dell sells weapons, armour, and provisions in Trinsic.".into(),
        }];
        upsert(
            &mut roster,
            NpcCard {
                npc_id: 10,
                appears_as: "shopkeeper".into(),
                name: None,
                job: None,
                summary: "I didn't learn anything specific from them, and I wish I had asked \
                     their name and what their job entails."
                    .into(),
            },
        );
        assert_eq!(roster[0].name.as_deref(), Some("Dell"));
        assert!(roster[0].job.as_deref().unwrap().contains("weapons"));
        assert!(roster[0].summary.contains("Dell sells"));
    }
}
