/// Fixed prompt templates. The player never types; Exult events fill these in.
///
/// Voice: first-person Avatar interior monologue. Spectron remembers Britannia;
/// the sidecar is a window into the Avatar's mind as they learn.
///
/// Never treat Exult schedule names as known identity. Durable person keys stay
/// on Spectron labels (`npc_id=…`), never in Avatar-facing prose. Use
/// `appears_as` (shape/role) until the transcript itself reveals a name.
use crate::events::Ambient;

const VOICE: &str = "\
You are me: the Avatar, summoned again to Britannia (Ultima VII / Exult). \
This window is my inner voice — always first person (I / me / my). \
I am not a narrator describing \"the Avatar\" or \"an individual\"; I AM that person. \
I may hold memories of earlier Quests (Ultima IV, V, VI) only if those facts are \
already in my memory from this Context; do not invent a full past if I have none yet. \
Plain modern English with a light period flavour: warm, curious, short. \
Do NOT use archaic or Middle English (no thee/thou/hast/doth, no faux-Shakespeare). \
Never claim certainty you do not have; hunches are fine if labelled as such. \
Do not invent personal names the dialogue has not revealed. \
Do not invent organisations or factions I have not yet encountered in this visit. \
Never mention engine or debugger identifiers (npc_id, shape numbers, traces). \
No markdown, no bullet lists, no third-person narrator. \
Never emit retrieval or citation tokens of any kind: not [S1], not S1, not \"source 1\".";

fn ambient_line(ambient: Option<&Ambient>) -> String {
    ambient
        .map(|a| format!("Right now: {}.\n", a.describe()))
        .unwrap_or_default()
}

pub fn briefing_prompt(appears_as: &str, _npc_id: i32, ambient: Option<&Ambient>) -> String {
    format!(
        "{VOICE}\n\n\
         I am now beginning to converse with someone who appears as \"{appears_as}\". \
         The conversation has already opened in the game.\n\
         {ambient}\
         From what I already remember about this person only: what I know of \
         their role or place; subjects we covered last time if we have spoken; \
         and subjects I considered or wished I had raised but never brought up \
         (stables, keys, rumours, names — whatever prior farewell or transcript \
         memory still holds). If I barely know them, say so and note one thing \
         I hope to learn.\n\
         Reply in 2–4 short sentences. Under ~55 words. Use present tense \
         (we are speaking now), not future \"about to\"."
    , ambient = ambient_line(ambient))
}

/// Mid-conversation recall when Finnigan / Batlin asks a copy-protection question.
/// `answer` is injected so the mind states the fact even if Spectron retrieval is thin.
pub fn copy_protect_thought_prompt(
    appears_as: &str,
    question: &str,
    answer: &str,
    source: &str,
) -> String {
    format!(
        "{VOICE}\n\n\
         Someone who appears as \"{appears_as}\" is testing me with a Black Gate \
         copy-protection question (manual / cloth-map knowledge, not something I \
         must invent):\n\
         \"{question}\"\n\n\
         I already know the answer from {source}: {answer}.\n\
         In first person, one or two short sentences: note that I am being tested \
         and state that I would answer {answer}. Must include the answer {answer}. \
         Under ~40 words. Do not invent other map facts."
    )
}

pub fn farewell_prompt(
    appears_as: &str,
    _npc_id: i32,
    transcript: &str,
    learned_name: Option<&str>,
    learned_job: Option<&str>,
    asked_name: bool,
    asked_job: bool,
) -> String {
    let transcript = if transcript.trim().is_empty() {
        "(No dialogue lines were captured for this talk.)"
    } else {
        transcript
    };
    let slots = name_job_slot_lines(learned_name, learned_job, asked_name, asked_job);
    format!(
        "{VOICE}\n\n\
         My conversation with someone who appeared as \"{appears_as}\" just ended.\n\
         {slots}\
         Transcript of this talk:\n\
         ---\n\
         {transcript}\n\
         ---\n\n\
         Identity: treat a personal name as known only if the structured Name \
         field above is filled, or the transcript shows I learned it. \
         \"You see…\" lines are appearance/role, not a name. \
         Job is known only from the Job field or an explicit Job reply.\n\n\
         In first person, briefly cover what I learned, one thing I still wish \
         I had asked, and any hunch linking people or places (wrong guesses ok). \
         Name concrete unfinished subjects when you can (e.g. the stables, a key, \
         a rumour) so a later greeting can recall them. \
         Where to go next only if the talk suggested it.\n\
         Reply in a few short sentences. Under ~75 words."
    )
}

fn name_job_slot_lines(
    learned_name: Option<&str>,
    learned_job: Option<&str>,
    asked_name: bool,
    asked_job: bool,
) -> String {
    let name_line = match learned_name {
        Some(n) => format!("Name (they said): {n}"),
        None if asked_name => {
            "Name: asked; no personal name (refused or unclear)".into()
        }
        None => "Name: not asked".into(),
    };
    let job_line = match learned_job {
        Some(j) => format!("Job (they said): {j}"),
        None if asked_job => "Job: asked, but no clear reply was captured".into(),
        None => "Job: not asked".into(),
    };
    format!("{name_line}\n{job_line}\n")
}

pub fn remember_transcript(
    appears_as: &str,
    _npc_id: i32,
    transcript: &str,
    learned_name: Option<&str>,
    learned_job: Option<&str>,
    asked_name: bool,
    asked_job: bool,
) -> String {
    let slots = name_job_slot_lines(learned_name, learned_job, asked_name, asked_job);
    format!(
        "Ultima VII conversation (appeared as: {appears_as}).\n\
         U7 baseline topics (Name | Job | Bye):\n\
         {slots}\
         Use a personal name only if Name above is filled or the transcript \
         shows it was revealed in dialogue; otherwise keep the role label.\n\
         Use Job only if Job above is filled or the transcript shows an \
         explicit Job reply.\n\
         This is spoken testimony — treat it as what was said, not as proven fact.\n\
         Prefer storing concrete subjects raised and any subjects left hanging \
         (questions considered but not asked), keyed to this person, so a later \
         re-meeting can recall unfinished topics.\n\
         When someone states a personal name for another character \
         (e.g. \"their father is called Aldo\"), store that naming explicitly: \
         the named person, the relation, and who said it. Later questions like \
         \"who is N's child?\" must be answerable from that testimony.\n\
         Do not store or invent engine identifiers.\n\
         Transcript:\n{transcript}"
    )
}

pub fn sign_thought_prompt(
    script: &str,
    text_latin: &str,
    text_runes: &str,
    ambient: Option<&Ambient>,
) -> String {
    let rune_note = if script == "runic" && !text_runes.is_empty() {
        format!("Runic glyphs (phonetic Unicode stand-in): {text_runes}\n")
    } else {
        String::new()
    };
    let style = match script {
        "runic" => "traditional Britannian runes",
        "latin" => "the newer Latin alphabet some people use now",
        "serpentine" => "serpentine script (harder for me to read)",
        other => other,
    };
    format!(
        "{VOICE}\n\n\
         I have just read a sign (script: {style}).\n\
         {ambient}\
         Latin reading: {text_latin}\n\
         {rune_note}\
         In first person: if runic, open with the sense of the text \
         (\"This runic sign says…\"). If Latin, you may briefly note that \
         the lettering is that newer alphabet some people use now — keep it \
         light, not alarmed. Then one short guess at where I might be. \
         Do not invent new factions from lettering style alone.\n\
         Under ~45 words."
    , ambient = ambient_line(ambient))
}

pub fn death_thought_prompt(ambient: Option<&Ambient>) -> String {
    format!(
        "{VOICE}\n\n\
         I have just fallen in battle (or otherwise died) in Britannia.\n\
         {ambient}\
         In Ultima VII I do not stay dead: someone often carries me to the \
         poorhouse in Paws (Black Gate), or the monks' hourglass returns me \
         (Serpent Isle). Death itself does not strip levels; costly rescue \
         magic like Kal Lor is a different matter.\n\
         Briefly: what happened, how I feel about trying again, one practical \
         tip for the next attempt.\n\
         Under ~50 words."
    , ambient = ambient_line(ambient))
}

pub fn location_muse_prompt(cue: &str, ambient: Option<&Ambient>) -> String {
    format!(
        "{VOICE}\n\n\
         A moment of travel or place-change:\n\
         {cue}\n\
         {ambient}\
         Guess where I might be — town, wilds, dungeon, coast — from this cue \
         and my memories. If nearby glance-names are listed, use them lightly \
         (they are peripheral vision, not close examines). Wrong guesses are fine \
         (\"I think this might be…\").\n\
         One or two short sentences. Under ~40 words."
    , ambient = ambient_line(ambient))
}

/// Written books / scrolls the Avatar has *looked at* (not lived experience).
pub fn remember_book(title: Option<&str>, text: &str) -> String {
    let title_line = title
        .map(|t| format!("Title (as printed): \"{t}\".\n"))
        .unwrap_or_default();
    format!(
        "Epistemic frame: the Avatar has just READ this document in Ultima VII. \
         That is a glance / study event - not something the Avatar did in the world, \
         and not automatic confirmation that every name in the text is a real person, \
         group, or place.\n\
         {title_line}\
         How to store this:\n\
         - Prefer ONE document / written-work memory: the Avatar read a document \
         (title if known) that says ...\n\
         - If this is a guest register / ledger / inn book for a named establishment \
         (e.g. \"The Honorable Hound\"), also store that named place as a real \
         pub/inn in the present town and that the listed people visited it. The \
         register is evidence about that place, not a substitute that erases the place.\n\
         - When dialogue already named someone who runs or works at that same \
         establishment, keep them linked (employee/innkeeper of that place) — do \
         not let the document overwrite the living place with \"only a book title\".\n\
         - Preserve concrete reference material when it is clearly instructional \
         (weapon/armour tables, maps, spells, history claimed as fact).\n\
         - Do NOT invent participation: reading a bill, charter, rose brotherhood tract, \
         or joke scroll does not mean living townsfolk \"engage in\" or \"are members of\" \
         organisations named only on the page.\n\
         - Do NOT create membership / devotion edges from people the Avatar already met \
         (e.g. Finnigan, Iolo, Spark) to organisations that appear only in this text \
         unless the dialogue has separately said so.\n\
         - Do NOT explode propaganda, allegory, or dense name-lists into a full ontology. \
         For registers, names + \"visited this inn\" is enough; no need for a node per line.\n\
         - Treat documents as written testimony (authority=written, epistemic=read), \
         weaker than conversation the Avatar heard and weaker than deeds they did.\n\n\
         Full text:\n{text}"
    )
}

/// Plaque / street sign the Avatar has just read (one distinct marker).
pub fn remember_sign(
    gump: &str,
    script: &str,
    text_latin: &str,
    text_raw: &str,
    text_runes: &str,
    ambient_suffix: &str,
) -> String {
    let script_note = match script {
        "latin" => "the newer Latin alphabet some people use now (not traditional runes)",
        "runic" => "traditional Britannian runes",
        "serpentine" => "serpentine script",
        other => other,
    };
    format!(
        "The Avatar read a distinct plaque or street sign. The readable text on \
         THIS marker was:\n\
         \"{text_latin}\"\n\
         How to store this:\n\
         - Do NOT merge successive signs into a single \"Ultima VII sign\" mega-node, \
         and do NOT pile unicode_runes / raw_usecode_encoding from other plaques onto it.\n\
         - If the text names a town, street, or landmark (e.g. PAWS, STRAND, Heroes Way), \
         prefer ONE geographic location entity for that place (normalise case: Paws). \
         The plaque is evidence the Avatar read at / about that place — not a separate \
         sibling entity typed MARKER or PLACE_LABEL that duplicates the location.\n\
         - Only keep a distinct \"sign\" node when the plaque is unique art/lore that is \
         not simply a place name (e.g. long motto).\n\
         Script appearance for this marker only: {script_note} (gump={gump}).\n\
         Optional engine metadata for this marker only (debug; not world ontology): \
         raw_usecode={text_raw}; phonetic_runes={text_runes} \
         (phonetic Unicode stand-in, not Exult pixel-identical).\n\
         {ambient_suffix}"
    )
}

/// Single-click glance name ("Injured Man", "a dog", "gold").
pub fn remember_examine(
    name: &str,
    quantity: i32,
    _npc_id: i32,
    place_hint: Option<&str>,
) -> String {
    let qty = if quantity > 1 {
        format!(" About {quantity} of them are in view.")
    } else {
        String::new()
    };
    let place = place_hint
        .map(|p| {
            format!(
                " This glance is in or near \"{p}\" (recent place cue from a sign or marker \
                 the Avatar read while exploring). Tie the sighting to that named place \
                 (e.g. observed at / is_at \"{p}\"), not to an abstract \"current_situation\" node."
            )
        })
        .unwrap_or_else(|| {
            " Tie the sighting to the named town or street of this Ultima VII visit when known \
             (e.g. Trinsic), not to a prior Quest and not to an abstract \"current_situation\" node."
                .into()
        });
    format!(
        "Epistemic frame: the Avatar has just LOOKED AT something in view \
         (Ultima VII single-click examine). The name shown was \"{name}\".{qty}{place}\n\
         How to store this:\n\
         - Prefer third person: the Avatar observed {name} at that place.\n\
         - Prefer durable edges such as: the Avatar / person observed → {name}; \
         {name} is_at → the named place. Do not invent is_nearby → current_situation \
         (chat over-weights that edge and then omits other observed animals).\n\
         - If {name} is an animal (horse, dog, cat, ...), store it as an animal sighting \
         at that place so questions about animals there can retrieve it. Normalise \
         singular/plural to one entity (dog not a separate dogs node).\n\
         - Labels like \"Injured Man\" describe what/who was seen - not the Avatar's \
         own condition or identity, and never the assistant's identity.\n\
         - This is not a conversation; do not invent dialogue, owners, riders, or prey.\n\
         - Do not invent travel or walking attributes (heading, stride, gait) for what was seen."
    )
}

/// Contents glimpsed when opening a world container (not taken / not owned).
pub fn remember_container(
    container: &str,
    items: &[(String, i32)],
    empty: bool,
    place_hint: Option<&str>,
) -> String {
    let place = place_hint
        .map(|p| format!(" The container was in or near \"{p}\"."))
        .unwrap_or_default();
    let contents = if empty || items.is_empty() {
        " It looked empty (nothing notable visible inside).".to_string()
    } else {
        let list: Vec<String> = items
            .iter()
            .map(|(name, qty)| {
                if *qty > 1 {
                    format!("{name} (×{qty})")
                } else {
                    name.clone()
                }
            })
            .collect();
        format!(" Visible contents (top level only): {}.", list.join("; "))
    };
    format!(
        "Epistemic frame: the Avatar has just OPENED a container in Ultima VII and \
         looked at what was inside — they have NOT necessarily taken anything.\n\
         Container name: \"{container}\".{place}{contents}\n\
         How to store this:\n\
         - Prefer third person: the Avatar looked inside {container} and saw … \
         (or saw that it was empty).\n\
         - This is a sighting / inventory glance, not ownership. Do NOT invent that \
         the Avatar took, stole, bought, or now owns these items.\n\
         - Prefer edges like: item was_seen_in / is_inside {container}; Avatar \
         observed the contents. Keep place via is_at / near the place cue when known.\n\
         - Nested bags listed as items are closed bags; do not invent their insides \
         until the Avatar opens them separately.\n\
         - Useful later for \"where did I see X?\" questions — preserve item names \
         as searchable facts."
    )
}

/// Kept for reference / one-off tools; live ingest uses [`remember_situation`].
#[allow(dead_code)]
pub fn remember_sextant(
    latitude: i32,
    latitude_hemi: &str,
    longitude: i32,
    longitude_hemi: &str,
) -> String {
    format!(
        "The Avatar glanced at a cloth-map sextant reading outdoors: roughly \
         {latitude}°{latitude_hemi}, {longitude}°{longitude_hemi} on the Britannian \
         map grid (same grid Finnigan's copy-protection questions use; not engine tiles).\n\
         How to store this:\n\
         - Prefer one soft location fix the Avatar would remember after a map glance \
         (\"around {latitude}{latitude_hemi}, {longitude}{longitude_hemi}\"), not a \
         dense GIS node with many attributes.\n\
         - Do NOT invent ships, rooms, gates, or back ways at these coordinates unless \
         another memory independently places them there.\n\
         - Optionally link to towns or signs already known; leave uncertain."
    )
}

/// Unified situation snapshot: what was seen, how far moved, optional sextant.
/// Glance-first so retrieval prefers place/scene over pedometry.
pub fn remember_situation(
    trigger: &str,
    seen: &[String],
    mode: &str,
    steps: u32,
    distance: u32,
    heading: &str,
    via: Option<&str>,
    sextant: Option<&crate::events::SextantFix>,
    // Dead-reckoning since the last map glance (sidecar-maintained).
    since_sextant: Option<&str>,
    ambient_suffix: &str,
) -> String {
    let seen_line = if seen.is_empty() {
        "Seen nearby: (nothing notable at a glance).".to_string()
    } else {
        format!("Seen nearby: {}.", seen.join(", "))
    };
    let moved_line = if mode == "teleport" {
        let via_bit = via.unwrap_or("teleport");
        format!(
            "Moved: sudden relocation via {via_bit} (~{distance} tiles) - not ordinary walking."
        )
    } else if distance == 0 && heading == "none" && steps == 0 {
        "Moved: little or no travel this stretch (standing / brief pause).".to_string()
    } else {
        let conveyance = match mode {
            "ship" => "by ship",
            "cart" => "by cart",
            "carpet" => "on a magic carpet",
            _ => "on foot",
        };
        format!(
            "Moved: about {steps} steps {heading} {conveyance} ({distance} net tiles after \
             opposite directions cancelled)."
        )
    };
    let sextant_line = sextant
        .map(|s| {
            format!(
                "Sextant (cloth map, this stretch): {}°{}, {}°{}. This resets the Avatar's \
                 feel of distance from the map — stand at this reading now.",
                s.latitude, s.latitude_hemi, s.longitude, s.longitude_hemi
            )
        })
        .unwrap_or_default();
    let why = match trigger {
        "talk" | "talk_end" => {
            "Flush trigger: conversation — treat this as the walk footnote before/after talking."
                .to_string()
        }
        "relocated" | "pre_relocate" => {
            "Flush trigger: sudden relocate — do not blend with ordinary walking stretches."
                .to_string()
        }
        "death" => "Flush trigger: death — last glance before the Avatar fell.".to_string(),
        "sextant" => "Flush trigger: sextant glance bound to this stretch.".to_string(),
        "cadence" => "Flush trigger: walking cadence (~50 steps).".to_string(),
        other => format!("Flush trigger: {other}."),
    };
    let sextant_block = if sextant_line.is_empty() {
        String::new()
    } else {
        format!("{sextant_line}\n")
    };
    let feel_block = since_sextant
        .filter(|s| !s.is_empty())
        .map(|s| format!("{s}\n"))
        .unwrap_or_default();
    format!(
        "Situation snapshot (trigger={trigger}).\n\
         {why}\n\
         {seen_line}\n\
         {moved_line}\n\
         {sextant_block}\
         {feel_block}\
         How to store this:\n\
         - Prefer what was SEEN and any place cues over pedometry.\n\
         - Movement this stretch is a short walk footnote, not a GIS trail.\n\
         - \"Feel since last sextant\" is soft dead-reckoning from the last cloth-map \
         glance (how far the Avatar thinks they are from that reading). Prefer one \
         current soft position relative to that fix; a new sextant reading resets it.\n\
         - Do NOT invent that walls enclose a city unless evidence clearly supports it; \
         hunches about fortress/crenellations are fine if labelled uncertain.\n\
         - Do not invent travel gait attributes for animals or people in the glance list.\n\
         {ambient_suffix}"
    )
}

pub fn remember_time_reading(text: &str, source: &str, hour: i32, minute: i32) -> String {
    let clock = if hour >= 0 && minute >= 0 {
        format!(" (game clock {hour:02}:{minute:02})")
    } else {
        String::new()
    };
    format!(
        "Epistemic frame: the Avatar checked a {source} in Ultima VII and read the time \
         as \"{text}\"{clock}. This is a glance at a clock or sundial — store the time \
         the Avatar observed, not a schedule change.\n\
         How to store this:\n\
         - Prefer a short fact that the Avatar knows the hour (\"it is about {text}\").\n\
         - Do NOT invent appointments, festivals, or NPC schedules from the reading alone.\n\
         - Sundials only work in daylight; if the source is a sundial at night, treat any \
         odd bark as the Avatar noticing that, not as a precise hour."
    )
}

pub fn time_reading_thought_prompt(
    text: &str,
    source: &str,
    ambient: Option<&Ambient>,
) -> String {
    let amb = ambient
        .map(|a| format!(" {}", a.describe()))
        .unwrap_or_default();
    format!(
        "{VOICE}\n\n\
         I just checked a {source}. It reads: \"{text}\".{amb}\n\
         One short first-person line about noticing the hour — no quest inventing."
    )
}

pub fn sextant_thought_prompt(
    latitude: i32,
    latitude_hemi: &str,
    longitude: i32,
    longitude_hemi: &str,
    ambient: Option<&Ambient>,
) -> String {
    format!(
        "{VOICE}\n\n\
         I have just checked my sextant against the cloth map of Britannia.\n\
         Reading: latitude {latitude}°{latitude_hemi}, longitude {longitude}°{longitude_hemi}.\n\
         {ambient}\
         Say what the sextant told me, then briefly guess where I am from places \
         I already know. Wrong guesses are fine if labelled.\n\
         Under ~50 words."
    , ambient = ambient_line(ambient))
}

pub fn companion_thought_prompt(appears_as: &str, joined: bool, ambient: Option<&Ambient>) -> String {
    let action = if joined {
        "joined my party again — or for the first time in this visit"
    } else {
        "left my party"
    };
    format!(
        "{VOICE}\n\n\
         {appears_as} has just {action}.\n\
         {ambient}\
         React briefly. If they are someone I know from earlier Quests \
         (Iolo, Shamino, Dupre, Jaana, and the rest), let that colour the thought.\n\
         Under ~35 words."
    , ambient = ambient_line(ambient))
}

pub fn knocked_out_thought_prompt(
    appears_as: &str,
    is_avatar: bool,
    ambient: Option<&Ambient>,
) -> String {
    if is_avatar {
        format!(
            "{VOICE}\n\n\
             I have been knocked down in combat (HP at or below zero, not dead). \
             I will lie here until it is safe to rise.\n\
             {ambient}\
             One short first-person line — pain, breath, waiting. No resurrection.\n\
             Under ~35 words."
        , ambient = ambient_line(ambient))
    } else {
        format!(
            "{VOICE}\n\n\
             {appears_as} has been knocked down in combat (not dead — they may rise \
             if left alone).\n\
             {ambient}\
             Brief concern or call to them. Under ~35 words."
        , ambient = ambient_line(ambient))
    }
}

pub fn companion_died_thought_prompt(appears_as: &str, ambient: Option<&Ambient>) -> String {
    format!(
        "{VOICE}\n\n\
         {appears_as} has died in combat — truly fallen, not merely knocked out.\n\
         {ambient}\
         Grief or resolve in one short first-person line. Under ~40 words."
    , ambient = ambient_line(ambient))
}

pub fn relocated_thought_prompt(
    via: &str,
    in_view: &[String],
    ambient: Option<&Ambient>,
) -> String {
    let glance = if in_view.is_empty() {
        "I cannot yet make out much around me.".into()
    } else {
        format!(
            "At a glance I notice: {}.",
            in_view.join(", ")
        )
    };
    format!(
        "{VOICE}\n\n\
         I have been suddenly relocated ({via}) — not ordinary walking. \
         Moongate, teleport, jail transport, or similar.\n\
         {glance}\n\
         {ambient}\
         One short first-person reaction: disorientation, then a hunch about where I am. \
         Do not invent town names not supported by the glance or my memories.\n\
         Under ~40 words."
    , ambient = ambient_line(ambient))
}

/// Soft atmosphere line when weather changes mid-travel (not a location guess).
pub fn weather_road_prompt(from_weather: &str, to_weather: &str, ambient: Option<&Ambient>) -> String {
    format!(
        "{VOICE}\n\n\
         I am on the road. The weather has shifted from {from_weather} to {to_weather}.\n\
         {ambient}\
         Notice the atmosphere only — rain, snow, clearing skies. Do not guess \
         which town I am near.\n\
         One or two short sentences. Under ~35 words."
    , ambient = ambient_line(ambient))
}

/// Ask Spectron to enumerate everyone the Avatar has spoken with (roster fill).
/// Reply must use the marked blocks so the sidecar can parse them.
pub fn npc_roster_list_prompt() -> String {
    format!(
        "{VOICE}\n\n\
         List every person I have spoken with in this visit to Britannia \
         (this Context only — not prior Quests unless I re-met them here).\n\
         For each person, use this exact block format and nothing else between blocks:\n\
         ===NPC===\n\
         appeared_as: <role or face label, e.g. a paladin or Spark>\n\
         name: <personal name if I learned it, else unknown>\n\
         job: <job if I learned it, else unknown>\n\
         knows: <one or two short sentences of what I know about them>\n\
         ===END===\n\n\
         Include people whose names I never learned (use their appearance label). \
         Do not invent people I have not met. Do not mention engine identifiers. \
         No markdown, no preamble, no closing essay — only the blocks."
    )
}

/// After a talk ends: what do I know about this person now?
pub fn npc_dossier_prompt(
    appears_as: &str,
    learned_name: Option<&str>,
    learned_job: Option<&str>,
    asked_name: bool,
    asked_job: bool,
    transcript: &str,
) -> String {
    let slots = name_job_slot_lines(learned_name, learned_job, asked_name, asked_job);
    let who = learned_name.unwrap_or(appears_as);
    let transcript = if transcript.trim().is_empty() {
        "(No dialogue lines were captured for this talk.)"
    } else {
        transcript
    };
    let name_rule = if learned_name.is_some() {
        format!(
            "I already know their personal name is {}. Do NOT say I wish I had asked \
             their name, and do NOT claim I barely know them on that basis.",
            learned_name.unwrap()
        )
    } else if asked_name {
        "I asked their name but they refused or gave no personal name \
         (e.g. \"not important\"). In the name field write unknown — not the refusal quote."
            .into()
    } else {
        "I have not learned a personal name. In the name field write unknown."
            .into()
    };
    let job_rule = if learned_job.is_some() {
        format!(
            "I already know their job/role (short form: {}). Do NOT say I wish I had \
             asked their job.",
            learned_job.unwrap()
        )
    } else if asked_job {
        "I asked about their job but got no clear answer. job field: unknown.".into()
    } else {
        "I have not learned their job. job field: unknown.".into()
    };
    format!(
        "{VOICE}\n\n\
         I just finished speaking with someone who appeared as \"{appears_as}\" \
         (refer to them as {who} when a personal name is known).\n\
         Structured slots from this talk:\n\
         {slots}\
         {name_rule}\n\
         {job_rule}\n\
         Transcript of this talk:\n\
         ---\n\
         {transcript}\n\
         ---\n\
         Pronouns: if the transcript uses he/him/his or she/her, match that gender \
         in \"knows\". Do not default to they/them when the text is clearly gendered.\n\
         In the name field put only a short personal name (e.g. Finnigan), never a \
         full quote like \"My name is Finnigan.\" Refusal lines are unknown.\n\
         In the job field put a short role (e.g. Mayor of Trinsic), not a long quote.\n\
         Reply with exactly one block:\n\
         ===NPC===\n\
         appeared_as: {appears_as}\n\
         name: <short personal name, or unknown>\n\
         job: <short job/role, or unknown>\n\
         knows: <two or three short sentences: who they are to me and what I learned; \
         only mention unfinished questions for topics I truly did not cover>\n\
         ===END===\n\n\
         Do not invent facts. Do not mention engine identifiers. No markdown."
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn name_job_slots_optional() {
        let none = remember_transcript("a guard", 1, "Avatar chooses: bye", None, None, false, false);
        assert!(none.contains("Name: not asked"));
        assert!(none.contains("Job: not asked"));

        let asked = remember_transcript(
            "a guard",
            1,
            "Avatar chooses: name\nI am Sparks.",
            Some("I am Sparks."),
            None,
            true,
            false,
        );
        assert!(asked.contains("Name (they said): I am Sparks."));
        assert!(asked.contains("Job: not asked"));
    }
}
