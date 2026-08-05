/// Lived memory of Britannia from Ultima IV–VI, embedded for one-click ingest.
///
/// Keep these in sync with `docs/prior-memory/*.md` (same files via `include_str!`).
/// Prose is third person (“the Avatar did…”) so Surrealist chat can answer
/// *about* that person without mirroring “I/you are the Avatar”. The sidecar
/// Avatar-mind still uses first person only in `/chat` templates (`prompts.rs`).

pub struct PriorMemoryDoc {
    pub era: &'static str,
    pub title: &'static str,
    pub body: &'static str,
}

/// Identity binding for Surrealist / Context chat: the *user* is the Avatar.
/// Assistants must not claim that role or answer as “I, the Avatar”.
pub const AVATAR_IDENTITY: &str = "\
Speaker rule for this Context: the human you are talking to is the Avatar. \
You are an assistant (Spectron / Surrealist), not the Avatar. Address them as \
you / your when helpful. Do not say \"I am the Avatar\" or narrate their deeds \
in the first person as if they were yours.\n\
\n\
Documents labelled ultima-iv, ultima-v, and ultima-vi are third-person records \
of the Avatar's lived memories from those Quests (Virtues, companions such as \
Iolo and Shamino, Lord British, towns and hardships of those years). When they \
ask what you know about them, summarise in second person (you / your) or neutral \
third person about the Avatar — never as \"I did…\" for the Avatar's past.\n\
\n\
In their present visit (Ultima VII / Black Gate) they are learning what has \
changed. Do not invent later-age factions until play has shown them in this age.\n\
\n\
Game events with epistemic=read, epistemic=examined, or authority=spoken are \
things the Avatar has read, glanced at, or heard; treat them as the Avatar's \
experiences when answering, still without speaking as the Avatar.";

pub const PRIOR_MEMORIES: &[PriorMemoryDoc] = &[
    PriorMemoryDoc {
        era: "ultima-iv",
        title: "Ultima IV — Quest of the Avatar",
        body: include_str!("../docs/prior-memory/ultima-iv.md"),
    },
    PriorMemoryDoc {
        era: "ultima-v",
        title: "Ultima V — War of the Shadows",
        body: include_str!("../docs/prior-memory/ultima-v.md"),
    },
    PriorMemoryDoc {
        era: "ultima-vi",
        title: "Ultima VI — False Prophet",
        body: include_str!("../docs/prior-memory/ultima-vi.md"),
    },
];
