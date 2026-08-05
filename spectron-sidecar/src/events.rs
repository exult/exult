use serde::{Deserialize, Serialize};

/// Ambient context attached to meaningful game events (no map coordinates).
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct Ambient {
    pub time_of_day: String,
    pub weather: String,
    pub setting: String,
}

/// One stackable (or unique) item seen inside an opened container.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContainerItem {
    pub name: String,
    #[serde(default = "default_qty")]
    pub quantity: i32,
    #[serde(default = "default_shape")]
    pub shape: i32,
}

fn default_qty() -> i32 {
    1
}

fn default_shape() -> i32 {
    -1
}

/// Events Exult (or the demo buttons) POST to the sidecar.
///
/// Do **not** treat schedule/`npc_name` data as what the Avatar knows.
/// Durable identity is `npc_id`. Player-facing identity comes from dialogue
/// text (`You see…`, `My name is…`) plus the shape label `appears_as`.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum GameEvent {
    /// Conversation with an NPC just opened.
    TalkStart {
        npc_id: i32,
        /// Shape/role label ("a guard"), not the schedule personal name.
        appears_as: String,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// One spoken / narrative line during the conversation.
    TalkLine {
        npc_id: i32,
        appears_as: String,
        text: String,
    },
    /// Player picked a conversation choice (often "Bye").
    TalkChoice {
        npc_id: i32,
        appears_as: String,
        choice: String,
    },
    /// Conversation closed (`end_conversation` / faces cleared).
    TalkEnd {
        npc_id: i32,
        appears_as: String,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Player opened a book/scroll; full text is already assembled by Exult.
    BookRead { title: Option<String>, text: String },
    /// Sign, plaque, tombstone, or similar (`display_runes`).
    SignRead {
        gump: String,
        script: String,
        text_raw: String,
        text_latin: String,
        #[serde(default)]
        text_runes: String,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Single-click examine label ("a guard", "gold", …).
    Examine {
        text: String,
        shape: i32,
        frame: i32,
        quantity: i32,
        npc_id: i32,
    },
    /// Opened a world chest/bag/corpse gump and saw top-level contents (not taken).
    ContainerOpened {
        container: String,
        shape: i32,
        #[serde(default)]
        empty: bool,
        #[serde(default)]
        items: Vec<ContainerItem>,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Combat mode enabled.
    CombatStart {
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Combat mode disabled.
    CombatEnd {
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Avatar entered sleep schedule.
    SleepStart {
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Avatar left sleep schedule.
    SleepEnd {
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Avatar moved underground (dungeon lighting).
    DungeonEnter {
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Avatar returned outdoors (surface lighting).
    DungeonLeave {
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Quantised situation snapshot (every ~50 steps or on talk / death / relocate / …).
    /// `seen` is a short glance of nearby player-facing names (not a tile dump).
    /// Optional `sextant` is the latest cloth-map reading on this stretch.
    /// Prefer this over legacy `Travel` / `SextantReading` / `Relocated`.
    Situation {
        #[serde(default = "default_situation_trigger")]
        trigger: String,
        #[serde(default, alias = "in_view")]
        seen: Vec<String>,
        #[serde(default = "default_travel_mode")]
        mode: String,
        #[serde(default)]
        steps: u32,
        #[serde(default)]
        distance: u32,
        #[serde(default = "default_heading")]
        heading: String,
        #[serde(default)]
        via: Option<String>,
        #[serde(default)]
        sextant: Option<SextantFix>,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Legacy travel stretch — accepted for older demos; prefer `Situation`.
    Travel {
        #[serde(default = "default_travel_mode")]
        mode: String,
        steps: u32,
        distance: u32,
        heading: String,
        short: u32,
        medium: u32,
        long: u32,
        #[serde(default)]
        in_view: Vec<String>,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Avatar died (Main_actor::die — BG wakes in Paws; SI via hourglass).
    Death {
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Legacy standalone sextant — prefer `Situation` with `sextant` set.
    SextantReading {
        latitude: i32,
        latitude_hemi: String,
        longitude: i32,
        longitude_hemi: String,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Watch / sundial / desk-clock bark ("10 o'clock", "Noon", "14:05").
    TimeReading {
        text: String,
        #[serde(default = "default_timepiece_source")]
        source: String,
        #[serde(default = "default_clock_field")]
        hour: i32,
        #[serde(default = "default_clock_field")]
        minute: i32,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Companion joined the Avatar's party.
    CompanionJoin {
        npc_id: i32,
        appears_as: String,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Companion left the Avatar's party.
    CompanionLeave {
        npc_id: i32,
        appears_as: String,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Avatar or party companion reached HP ≤ 0 without dying (lies down until safe).
    PartyKnockedOut {
        npc_id: i32,
        appears_as: String,
        #[serde(default)]
        is_avatar: bool,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Party companion died for real (body left; Avatar true-death is `Death`).
    CompanionDied {
        npc_id: i32,
        appears_as: String,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
    /// Legacy sudden place-change — prefer `Situation` with `trigger=relocated`.
    Relocated {
        #[serde(default = "default_relocate_via")]
        via: String,
        #[serde(default)]
        distance: u32,
        #[serde(default)]
        in_view: Vec<String>,
        #[serde(default)]
        ambient: Option<Ambient>,
    },
}

/// Cloth-map sextant fix attached to a situation stretch.
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct SextantFix {
    pub latitude: i32,
    pub latitude_hemi: String,
    pub longitude: i32,
    pub longitude_hemi: String,
}

fn default_travel_mode() -> String {
    "foot".into()
}

fn default_situation_trigger() -> String {
    "cadence".into()
}

fn default_heading() -> String {
    "none".into()
}

fn default_relocate_via() -> String {
    "teleport".into()
}

fn default_timepiece_source() -> String {
    "timepiece".into()
}

fn default_clock_field() -> i32 {
    -1
}

impl Ambient {
    pub fn describe(&self) -> String {
        format!(
            "It is {}, weather {}, {}.",
            self.time_of_day, self.weather, self.setting
        )
    }
}

const CHRONICLE_TEXT_MAX: usize = 40;

fn truncate_chars(s: &str, max: usize) -> String {
    let count = s.chars().count();
    if count <= max {
        s.to_string()
    } else {
        let cut: String = s.chars().take(max).collect();
        format!("{cut}...")
    }
}

fn ambient_json(ambient: &Option<Ambient>) -> Option<serde_json::Value> {
    ambient.as_ref().map(|a| {
        serde_json::json!({
            "time_of_day": a.time_of_day,
            "weather": a.weather,
            "setting": a.setting,
        })
    })
}

impl GameEvent {
    /// Avatar-facing chronicle line: same event as the bridge POST, minus internal
    /// engine fields (`npc_id`, shape/frame, usecode `text_raw`, …). Long text is
    /// clipped to 40 characters. Full payloads still appear on the Exult debug log.
    pub fn chronicle_json(&self) -> String {
        let mut map = serde_json::Map::new();
        match self {
            Self::TalkStart {
                appears_as,
                ambient,
                ..
            } => {
                map.insert("type".into(), "talk_start".into());
                map.insert("appears_as".into(), appears_as.clone().into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::TalkLine {
                appears_as, text, ..
            } => {
                map.insert("type".into(), "talk_line".into());
                map.insert("appears_as".into(), appears_as.clone().into());
                map.insert("text".into(), truncate_chars(text, CHRONICLE_TEXT_MAX).into());
            }
            Self::TalkChoice {
                appears_as, choice, ..
            } => {
                map.insert("type".into(), "talk_choice".into());
                map.insert("appears_as".into(), appears_as.clone().into());
                map.insert(
                    "choice".into(),
                    truncate_chars(choice, CHRONICLE_TEXT_MAX).into(),
                );
            }
            Self::TalkEnd {
                appears_as,
                ambient,
                ..
            } => {
                map.insert("type".into(), "talk_end".into());
                map.insert("appears_as".into(), appears_as.clone().into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::BookRead { title, text } => {
                map.insert("type".into(), "book_read".into());
                if let Some(t) = title {
                    map.insert("title".into(), t.clone().into());
                }
                map.insert("text".into(), truncate_chars(text, CHRONICLE_TEXT_MAX).into());
            }
            Self::SignRead {
                gump,
                script,
                text_latin,
                ambient,
                ..
            } => {
                // Avatar sees the plaque form + readable Latin (not usecode encoding /
                // phonetic Unicode stand-ins).
                map.insert("type".into(), "sign_read".into());
                map.insert("gump".into(), gump.clone().into());
                map.insert("script".into(), script.clone().into());
                map.insert(
                    "text".into(),
                    truncate_chars(text_latin, CHRONICLE_TEXT_MAX).into(),
                );
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::Examine { text, quantity, .. } => {
                map.insert("type".into(), "examine".into());
                map.insert("text".into(), truncate_chars(text, CHRONICLE_TEXT_MAX).into());
                map.insert("quantity".into(), (*quantity).into());
            }
            Self::ContainerOpened {
                container,
                empty,
                items,
                ambient,
                ..
            } => {
                map.insert("type".into(), "container_opened".into());
                map.insert(
                    "container".into(),
                    truncate_chars(container, CHRONICLE_TEXT_MAX).into(),
                );
                map.insert("empty".into(), (*empty).into());
                let item_names: Vec<serde_json::Value> = items
                    .iter()
                    .take(12)
                    .map(|i| {
                        if i.quantity > 1 {
                            format!("{}×{}", truncate_chars(&i.name, 24), i.quantity).into()
                        } else {
                            truncate_chars(&i.name, 28).into()
                        }
                    })
                    .collect();
                map.insert("items".into(), item_names.into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::CombatStart { ambient } => {
                map.insert("type".into(), "combat_start".into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::CombatEnd { ambient } => {
                map.insert("type".into(), "combat_end".into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::SleepStart { ambient } => {
                map.insert("type".into(), "sleep_start".into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::SleepEnd { ambient } => {
                map.insert("type".into(), "sleep_end".into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::DungeonEnter { ambient } => {
                map.insert("type".into(), "dungeon_enter".into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::DungeonLeave { ambient } => {
                map.insert("type".into(), "dungeon_leave".into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::Situation {
                trigger,
                seen,
                mode,
                steps,
                distance,
                heading,
                via,
                sextant,
                ambient,
            } => {
                map.insert("type".into(), "situation".into());
                map.insert("trigger".into(), trigger.clone().into());
                map.insert("mode".into(), mode.clone().into());
                map.insert("steps".into(), (*steps).into());
                map.insert("distance".into(), (*distance).into());
                map.insert("heading".into(), heading.clone().into());
                if !seen.is_empty() {
                    map.insert(
                        "seen".into(),
                        serde_json::Value::Array(
                            seen.iter()
                                .map(|s| serde_json::Value::String(s.clone()))
                                .collect(),
                        ),
                    );
                }
                if let Some(v) = via {
                    map.insert("via".into(), v.clone().into());
                }
                if let Some(sx) = sextant {
                    map.insert(
                        "sextant".into(),
                        serde_json::json!({
                            "latitude": sx.latitude,
                            "latitude_hemi": sx.latitude_hemi,
                            "longitude": sx.longitude,
                            "longitude_hemi": sx.longitude_hemi,
                        }),
                    );
                }
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::Travel {
                mode,
                steps,
                distance,
                heading,
                short,
                medium,
                long,
                in_view,
                ambient,
            } => {
                map.insert("type".into(), "travel".into());
                map.insert("mode".into(), mode.clone().into());
                map.insert("steps".into(), (*steps).into());
                map.insert("distance".into(), (*distance).into());
                map.insert("heading".into(), heading.clone().into());
                map.insert("short".into(), (*short).into());
                map.insert("medium".into(), (*medium).into());
                map.insert("long".into(), (*long).into());
                if !in_view.is_empty() {
                    map.insert(
                        "in_view".into(),
                        serde_json::Value::Array(
                            in_view
                                .iter()
                                .map(|s| serde_json::Value::String(s.clone()))
                                .collect(),
                        ),
                    );
                }
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::Death { ambient } => {
                map.insert("type".into(), "death".into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::SextantReading {
                latitude,
                latitude_hemi,
                longitude,
                longitude_hemi,
                ambient,
            } => {
                map.insert("type".into(), "sextant_reading".into());
                map.insert("latitude".into(), (*latitude).into());
                map.insert("latitude_hemi".into(), latitude_hemi.clone().into());
                map.insert("longitude".into(), (*longitude).into());
                map.insert("longitude_hemi".into(), longitude_hemi.clone().into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::TimeReading {
                text,
                source,
                hour,
                minute,
                ambient,
            } => {
                map.insert("type".into(), "time_reading".into());
                map.insert("text".into(), truncate_chars(text, CHRONICLE_TEXT_MAX).into());
                map.insert("source".into(), source.clone().into());
                map.insert("hour".into(), (*hour).into());
                map.insert("minute".into(), (*minute).into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::CompanionJoin { appears_as, ambient, .. } => {
                map.insert("type".into(), "companion_join".into());
                map.insert("appears_as".into(), appears_as.clone().into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::CompanionLeave { appears_as, ambient, .. } => {
                map.insert("type".into(), "companion_leave".into());
                map.insert("appears_as".into(), appears_as.clone().into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::PartyKnockedOut {
                appears_as,
                is_avatar,
                ambient,
                ..
            } => {
                map.insert("type".into(), "party_knocked_out".into());
                map.insert("appears_as".into(), appears_as.clone().into());
                map.insert("is_avatar".into(), (*is_avatar).into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::CompanionDied { appears_as, ambient, .. } => {
                map.insert("type".into(), "companion_died".into());
                map.insert("appears_as".into(), appears_as.clone().into());
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
            Self::Relocated {
                via,
                distance,
                in_view,
                ambient,
            } => {
                map.insert("type".into(), "relocated".into());
                map.insert("via".into(), via.clone().into());
                map.insert("distance".into(), (*distance).into());
                if !in_view.is_empty() {
                    map.insert(
                        "in_view".into(),
                        serde_json::Value::Array(
                            in_view
                                .iter()
                                .map(|s| serde_json::Value::String(s.clone()))
                                .collect(),
                        ),
                    );
                }
                if let Some(a) = ambient_json(ambient) {
                    map.insert("ambient".into(), a);
                }
            }
        }
        serde_json::Value::Object(map).to_string()
    }

    /// Compact chronicle line for the UI (newest-first scroll list).
    pub fn chronicle_display(&self) -> String {
        match self {
            Self::TalkLine { appears_as, text, .. } => {
                format!("{appears_as}: {}", truncate_chars(text, 72))
            }
            Self::TalkStart { appears_as, .. } => format!("Began speaking with {appears_as}"),
            Self::TalkEnd { appears_as, .. } => format!("Parted from {appears_as}"),
            Self::TalkChoice {
                appears_as, choice, ..
            } => format!("To {appears_as}, I chose: {}", truncate_chars(choice, 40)),
            Self::Examine { text, quantity, .. } => {
                if *quantity > 1 {
                    format!("Looked at {text} (×{quantity})")
                } else {
                    format!("Looked at {text}")
                }
            }
            Self::ContainerOpened {
                container, empty, items, ..
            } => {
                if *empty || items.is_empty() {
                    format!("Looked inside {container} (empty)")
                } else {
                    let preview: Vec<String> = items
                        .iter()
                        .take(4)
                        .map(|i| {
                            if i.quantity > 1 {
                                format!("{}×{}", i.name, i.quantity)
                            } else {
                                i.name.clone()
                            }
                        })
                        .collect();
                    let more = if items.len() > 4 {
                        format!(", +{} more", items.len() - 4)
                    } else {
                        String::new()
                    };
                    format!(
                        "Looked inside {container}: {}{more}",
                        preview.join(", ")
                    )
                }
            }
            Self::BookRead { title, text } => {
                let cleaned = crate::books::normalize_book_text(text);
                let label = crate::books::resolve_title(title.as_deref(), &cleaned)
                    .unwrap_or_else(|| "a book".into());
                format!("Read {label}: {}", truncate_chars(&cleaned, 48))
            }
            Self::SignRead {
                script, text_latin, ..
            } => format!("Read a {script} sign: {}", truncate_chars(text_latin, 48)),
            Self::Situation {
                trigger,
                seen,
                mode,
                distance,
                heading,
                sextant,
                ..
            } => {
                let glance = if seen.is_empty() {
                    String::new()
                } else {
                    format!("; saw {}", seen.join(", "))
                };
                let fix = sextant.as_ref().map(|s| {
                    format!(
                        "; sextant {}{} {}{}",
                        s.latitude, s.latitude_hemi, s.longitude, s.longitude_hemi
                    )
                });
                let move_bit = if *distance == 0 && heading == "none" {
                    String::new()
                } else if mode == "teleport" {
                    format!("teleport ~{distance} tiles")
                } else if mode == "foot" {
                    format!("{distance} net tiles {heading}")
                } else {
                    format!("{distance} net tiles {heading} by {mode}")
                };
                format!(
                    "Situation ({trigger}): {move_bit}{}{}",
                    glance,
                    fix.unwrap_or_default()
                )
            }
            Self::Travel {
                mode,
                distance,
                heading,
                in_view,
                ..
            } => {
                let base = if mode == "foot" {
                    format!("Travelled {distance} net tile-steps {heading}")
                } else {
                    format!("Travelled {distance} net tiles {heading} by {mode}")
                };
                if in_view.is_empty() {
                    base
                } else {
                    format!("{base}; saw {}", in_view.join(", "))
                }
            }
            other => other.summary_label(),
        }
    }

    pub fn summary_label(&self) -> String {
        match self {
            Self::TalkStart {
                appears_as, ..
            } => format!("Talk start: {appears_as}"),
            Self::TalkLine { text, .. } => text.clone(),
            Self::TalkChoice { choice, .. } => format!("Avatar: {choice}"),
            Self::TalkEnd {
                appears_as, ..
            } => format!("Talk end: {appears_as}"),
            Self::BookRead { title, text } => {
                let cleaned = crate::books::normalize_book_text(text);
                let resolved = crate::books::resolve_title(title.as_deref(), &cleaned)
                    .unwrap_or_else(|| "(untitled)".into());
                let preview = cleaned
                    .lines()
                    .map(str::trim)
                    .find(|l| !l.is_empty() && !l.eq_ignore_ascii_case(resolved.as_str()))
                    .unwrap_or("")
                    .trim_matches('*')
                    .trim();
                if preview.is_empty() {
                    format!("Book: {resolved}")
                } else {
                    format!(
                        "Book: {resolved} — {}",
                        truncate_chars(preview, 72)
                    )
                }
            }
            Self::SignRead {
                gump,
                script,
                text_latin,
                text_runes,
                ..
            } => {
                if text_runes.is_empty() {
                    format!("Sign [{script}/{gump}]: {text_latin}")
                } else {
                    format!("Sign [{script}/{gump}]: {text_latin} · {text_runes}")
                }
            }
            Self::Examine { text, quantity, .. } => {
                if *quantity > 1 {
                    format!("Examine: {text} ×{quantity}")
                } else {
                    format!("Examine: {text}")
                }
            }
            Self::ContainerOpened {
                container, empty, items, ..
            } => {
                if *empty || items.is_empty() {
                    format!("Container: {container} (empty)")
                } else {
                    format!("Container: {container} ({} kinds)", items.len())
                }
            }
            Self::CombatStart { .. } => "Combat start".into(),
            Self::CombatEnd { .. } => "Combat end".into(),
            Self::SleepStart { .. } => "Sleep start".into(),
            Self::SleepEnd { .. } => "Sleep end".into(),
            Self::DungeonEnter { .. } => "Dungeon enter".into(),
            Self::DungeonLeave { .. } => "Dungeon leave".into(),
            Self::Situation {
                trigger,
                seen,
                mode,
                distance,
                heading,
                sextant,
                ..
            } => {
                let glance = if seen.is_empty() {
                    String::new()
                } else {
                    format!(" · {}", seen.join(", "))
                };
                let fix = sextant.as_ref().map(|s| {
                    format!(
                        " · sextant {}{} {}{}",
                        s.latitude, s.latitude_hemi, s.longitude, s.longitude_hemi
                    )
                });
                let move_bit = if mode == "teleport" {
                    format!("teleport ~{distance}")
                } else if *distance == 0 && heading == "none" {
                    "standing".into()
                } else {
                    format!("{distance} {heading}")
                };
                format!("Situation ({trigger}): {move_bit}{glance}{}", fix.unwrap_or_default())
            }
            Self::Travel {
                mode,
                distance,
                heading,
                in_view,
                ..
            } => {
                let base = if mode == "foot" {
                    format!("Travel: {distance} net tile-steps heading {heading}")
                } else {
                    format!("Travel: {distance} net tiles heading {heading} by {mode}")
                };
                if in_view.is_empty() {
                    base
                } else {
                    format!("{base} · {}", in_view.join(", "))
                }
            }
            Self::Death { .. } => "Death".into(),
            Self::SextantReading {
                latitude,
                latitude_hemi,
                longitude,
                longitude_hemi,
                ..
            } => format!("Sextant: {latitude}{latitude_hemi}, {longitude}{longitude_hemi}"),
            Self::TimeReading { text, source, .. } => {
                format!("Time ({source}): {text}")
            }
            Self::CompanionJoin { appears_as, .. } => format!("Companion joined: {appears_as}"),
            Self::CompanionLeave { appears_as, .. } => format!("Companion left: {appears_as}"),
            Self::PartyKnockedOut {
                appears_as,
                is_avatar,
                ..
            } => {
                if *is_avatar {
                    "Knocked out (Avatar)".into()
                } else {
                    format!("Knocked out: {appears_as}")
                }
            }
            Self::CompanionDied { appears_as, .. } => format!("Companion died: {appears_as}"),
            Self::Relocated {
                via,
                distance,
                in_view,
                ..
            } => {
                let base = format!("Suddenly relocated via {via} (~{distance} tiles)");
                if in_view.is_empty() {
                    base
                } else {
                    format!("{base} · {}", in_view.join(", "))
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn chronicle_json_truncates_book_text() {
        let ev = GameEvent::BookRead {
            title: Some("Vetron".into()),
            text: "x".repeat(80),
        };
        let line = ev.chronicle_json();
        assert!(line.contains("..."));
        assert!(!line.contains(&"x".repeat(50)));
    }

    #[test]
    fn book_summary_resolves_title_from_body() {
        let ev = GameEvent::BookRead {
            title: None,
            text: "~~ ~~THE HONORABLE HOUND\n~~ ~~Register*\n~ ~Walter of Britain".into(),
        };
        let label = ev.summary_label();
        assert!(
            label.to_uppercase().contains("HONORABLE HOUND"),
            "got {label}"
        );
        assert!(!label.contains("(untitled)"), "got {label}");
        assert!(label.contains("Walter") || label.contains("Register"), "got {label}");
    }

    #[test]
    fn chronicle_examine_hides_engine_fields() {
        let ev = GameEvent::Examine {
            text: "lit lamp".into(),
            shape: 526,
            frame: 0,
            quantity: 1,
            npc_id: -1,
        };
        let line = ev.chronicle_json();
        let v: serde_json::Value = serde_json::from_str(&line).unwrap();
        assert_eq!(v["type"], "examine");
        assert_eq!(v["text"], "lit lamp");
        assert_eq!(v["quantity"], 1);
        assert!(v.get("shape").is_none());
        assert!(v.get("frame").is_none());
        assert!(v.get("npc_id").is_none());
    }

    #[test]
    fn chronicle_talk_hides_npc_id() {
        let ev = GameEvent::TalkLine {
            npc_id: 11,
            appears_as: "peasant".into(),
            text: "\"Goodbye,\" the man sniffs.*".into(),
        };
        let line = ev.chronicle_json();
        assert!(line.contains(r#""type":"talk_line""#));
        assert!(line.contains(r#""appears_as":"peasant""#));
        assert!(!line.contains("npc_id"));
    }

    #[test]
    fn chronicle_display_is_readable() {
        let ev = GameEvent::Examine {
            text: "tree".into(),
            shape: 453,
            frame: 0,
            quantity: 1,
            npc_id: -1,
        };
        assert_eq!(ev.chronicle_display(), "Looked at tree");
        let talk = GameEvent::TalkLine {
            npc_id: 1,
            appears_as: "Iolo".into(),
            text: "\"Yes, my friend?\" Iolo asks.".into(),
        };
        assert!(talk.chronicle_display().starts_with("Iolo:"));
    }

    #[test]
    fn situation_json_parses_seen_and_sextant() {
        let raw = r#"{
            "type":"situation",
            "trigger":"talk",
            "seen":["dog","wall"],
            "mode":"foot",
            "steps":15,
            "distance":12,
            "heading":"WSW",
            "sextant":{"latitude":106,"latitude_hemi":"S","longitude":7,"longitude_hemi":"E"},
            "ambient":{"time_of_day":"day","weather":"clear","setting":"outdoors"}
        }"#;
        let ev: GameEvent = serde_json::from_str(raw).expect("situation");
        match ev {
            GameEvent::Situation {
                trigger,
                seen,
                steps,
                distance,
                heading,
                sextant,
                ..
            } => {
                assert_eq!(trigger, "talk");
                assert_eq!(seen, vec!["dog", "wall"]);
                assert_eq!(steps, 15);
                assert_eq!(distance, 12);
                assert_eq!(heading, "WSW");
                let sx = sextant.expect("sextant");
                assert_eq!(sx.latitude, 106);
                assert_eq!(sx.latitude_hemi, "S");
            }
            other => panic!("expected Situation, got {other:?}"),
        }
    }
}
