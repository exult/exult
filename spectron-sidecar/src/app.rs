use std::collections::VecDeque;
use std::sync::{Arc, Mutex};

use eframe::egui;
use tokio::runtime::Handle;
use tokio::sync::mpsc;

use crate::events::{Ambient, GameEvent, SextantFix};
use crate::prompts;
use crate::sextant_feel::SextantFeel;
use crate::spectron::{RememberOpts, SpectronClient};
use crate::theme::{self, CREST, STONE, STONE_DEEP, STONE_LIGHT, WOOD};

const CHRONICLE_CAP: usize = 200;
const MIND_CAP: usize = 80;
const ACTIVITY_CAP: usize = 40;
/// Soft-muse on location every N travel batches (force-flush / event-flush stretches).
const LOCATION_MUSE_EVERY_N_TRAVELS: u32 = 4;

fn chronicle_bullet_colour(line: &str) -> egui::Color32 {
    if line.starts_with("Looked at ") {
        theme::MOSS
    } else if line.starts_with("Travelled") || line.starts_with("Situation") {
        theme::WOOD_LIGHT
    } else if line.starts_with("Read ") {
        theme::CREST
    } else if line.starts_with("Began speaking")
        || line.starts_with("Parted from")
        || line.starts_with("To ")
    {
        theme::RUG_BLUE_LIGHT
    } else {
        // Spoken lines ("shopkeeper: ...") and rarer events.
        theme::RUG_BLUE_LIGHT
    }
}

#[derive(Clone, Copy)]
enum ThoughtKind {
    Greeting,
    Parting,
    Passing,
}

impl ThoughtKind {
    fn label(self) -> &'static str {
        match self {
            Self::Greeting => "As I greet them",
            Self::Parting => "As we part",
            Self::Passing => "Passing thought",
        }
    }

    fn bullet(self) -> egui::Color32 {
        match self {
            Self::Greeting => theme::RUG_BLUE_LIGHT,
            Self::Parting => theme::GOLD_DIM,
            Self::Passing => theme::MOSS,
        }
    }

    fn waiting(self) -> &'static str {
        match self {
            Self::Greeting => "Listening within...",
            Self::Parting => "Gathering my thoughts...",
            Self::Passing => "Listening within...",
        }
    }
}

#[derive(Clone)]
struct MindEntry {
    id: u64,
    kind: ThoughtKind,
    body: String,
}

#[derive(Clone, Default)]
struct SharedUi {
    status: String,
    busy: bool,
    /// Mind feed (newest first) - greetings, partings, and passing thoughts together.
    mind: VecDeque<MindEntry>,
    mind_seq: u64,
    /// Best-effort place cue from recent signs (so examines can say "in/near Trinsic").
    last_place_hint: Option<String>,
    /// Front-and-centre event stream (truncated JSON, newest first).
    chronicle: VecDeque<String>,
    /// Spectron activity (remember / chat status), quieter sidebar strip.
    activity: VecDeque<String>,
    active_npc: Option<(i32, String)>,
    transcript: String,
    /// After Avatar chooses Name / Job, capture the next NPC line(s) into these Options.
    expect_name_reply: bool,
    expect_job_reply: bool,
    /// Avatar opened the Name topic this talk (U7 baseline menu).
    asked_name: bool,
    /// Avatar opened the Job topic this talk.
    asked_job: bool,
    /// Personal name they gave if Avatar asked Name (None if not asked or no reply yet).
    learned_name: Option<String>,
    /// Job / role they claimed if Avatar asked Job.
    learned_job: Option<String>,
    /// People the Avatar has met — filled by Spectron + local stubs on parting.
    npc_roster: Vec<crate::npc_roster::NpcCard>,
    /// Copy-protection question fingerprints already prompted this talk (dedupe).
    copy_protect_prompted: std::collections::HashSet<String>,
    travel_batches: u32,
    /// Last outdoor weather seen on a travel batch (for silence-on-the-road flips).
    last_outdoor_weather: Option<String>,
    /// Soft dead-reckoning from the last cloth-map sextant glance.
    sextant_feel: SextantFeel,
}

pub struct SidecarApp {
    ui: Arc<Mutex<SharedUi>>,
    events_rx: mpsc::UnboundedReceiver<GameEvent>,
    events_tx: mpsc::UnboundedSender<GameEvent>,
    client: SpectronClient,
    rt: Handle,
    listen_addr: String,
    /// Collapsible panels — only Thoughts starts open so the window is quieter.
    show_people: bool,
    show_chronicle: bool,
    show_thoughts: bool,
    show_demo: bool,
}

impl SidecarApp {
    pub fn new(
        cc: &eframe::CreationContext<'_>,
        events_rx: mpsc::UnboundedReceiver<GameEvent>,
        events_tx: mpsc::UnboundedSender<GameEvent>,
        client: SpectronClient,
        rt: Handle,
        listen_addr: String,
    ) -> Self {
        cc.egui_ctx.set_pixels_per_point(1.25);
        theme::install_fonts(&cc.egui_ctx);
        theme::apply_visuals(&cc.egui_ctx);

        let ui = Arc::new(Mutex::new(SharedUi {
            status: format!("Listening on http://{listen_addr}, events at /event"),
            ..Default::default()
        }));
        Self {
            ui,
            events_rx,
            events_tx,
            client,
            rt,
            listen_addr,
            show_people: false,
            show_chronicle: false,
            show_thoughts: true,
            show_demo: false,
        }
    }

    /// Panel title row with a close control (re-open from the top bar toggles).
    fn paint_panel_title(ui: &mut egui::Ui, title: &str, open: &mut bool) {
        ui.horizontal(|ui| {
            theme::paint_shadowed_label(ui, title, 20.0);
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui
                    .small_button("−")
                    .on_hover_text("Close panel")
                    .clicked()
                {
                    *open = false;
                }
            });
        });
    }

    fn paint_panel_toggle(ui: &mut egui::Ui, label: &str, open: &mut bool) {
        // ASCII markers — Fondamento lacks fancy chevron glyphs.
        let mark = if *open { "[-]" } else { "[+]" };
        if ui
            .selectable_label(*open, format!("{mark} {label}"))
            .on_hover_text(if *open {
                "Click to close"
            } else {
                "Click to open"
            })
            .clicked()
        {
            *open = !*open;
        }
    }

    fn push_chronicle(ui: &mut SharedUi, line: impl Into<String>) {
        ui.chronicle.push_front(line.into());
        while ui.chronicle.len() > CHRONICLE_CAP {
            ui.chronicle.pop_back();
        }
    }

    fn push_mind(ui: &mut SharedUi, kind: ThoughtKind, body: impl Into<String>) -> u64 {
        ui.mind_seq = ui.mind_seq.saturating_add(1);
        let id = ui.mind_seq;
        ui.mind.push_front(MindEntry {
            id,
            kind,
            body: body.into(),
        });
        while ui.mind.len() > MIND_CAP {
            ui.mind.pop_back();
        }
        id
    }

    fn push_activity(ui: &mut SharedUi, line: impl Into<String>) {
        ui.activity.push_front(line.into());
        while ui.activity.len() > ACTIVITY_CAP {
            ui.activity.pop_back();
        }
    }

    fn spawn_remember(&self, body: String, log_ok: String) {
        self.spawn_remember_with(
            body,
            RememberOpts {
                labels: vec!["source=exult".into(), "kind=event".into()],
                infer: None,
                memory_category: None,
            },
            log_ok,
        );
    }

    fn spawn_remember_with(&self, body: String, opts: RememberOpts, log_ok: String) {
        let ui = Arc::clone(&self.ui);
        let client = self.client.clone();
        self.rt.spawn(async move {
            let msg = match client.remember_text_with(&body, opts).await {
                Ok(()) => log_ok,
                Err(e) => format!("Remember error: {e:#}"),
            };
            let mut g = ui.lock().expect("ui lock");
            Self::push_activity(&mut g, msg);
        });
    }

    /// Ingest Ultima IV-VI lived memory into the current Spectron Context.
    fn spawn_prior_britannia_memories(&self) {
        let ui = Arc::clone(&self.ui);
        let client = self.client.clone();
        {
            let mut g = ui.lock().expect("ui lock");
            g.busy = true;
            g.status = "Giving the Avatar memories of prior Britannia...".into();
            Self::push_activity(
                &mut g,
                "-> Ingesting Ultima IV-VI prior memory into Spectron",
            );
        }
        self.rt.spawn(async move {
            let mut ok = 0usize;
            let mut failures: Vec<String> = Vec::new();

            // Bind identity first so Surrealist /chat does not narrate "an individual".
            {
                let labels = vec![
                    "source=exult".into(),
                    "kind=prior_memory".into(),
                    "authority=lived".into(),
                    "era=identity".into(),
                ];
                let id_opts = RememberOpts {
                    labels,
                    infer: Some("full"),
                    memory_category: Some("identity"),
                };
                match client
                    .remember_text_with(crate::prior_memory::AVATAR_IDENTITY, id_opts)
                    .await
                {
                    Ok(()) => {
                        ok += 1;
                        let mut g = ui.lock().expect("ui lock");
                        Self::push_activity(
                            &mut g,
                            "Remembered Avatar identity (you are the Avatar; assistant is not)",
                        );
                    }
                    Err(e) => {
                        // Fall back to infer=none so identity still lands if extraction fails.
                        let labels = vec![
                            "source=exult".into(),
                            "kind=prior_memory".into(),
                            "authority=lived".into(),
                            "era=identity".into(),
                        ];
                        let id_opts = RememberOpts {
                            labels,
                            infer: Some("none"),
                            memory_category: Some("identity"),
                        };
                        match client
                            .remember_text_with(crate::prior_memory::AVATAR_IDENTITY, id_opts)
                            .await
                        {
                            Ok(()) => {
                                ok += 1;
                                let mut g = ui.lock().expect("ui lock");
                                Self::push_activity(
                                    &mut g,
                                    "Remembered Avatar identity via infer=none",
                                );
                            }
                            Err(e2) => {
                                failures.push(format!("identity: {e:#} / {e2:#}"));
                                let mut g = ui.lock().expect("ui lock");
                                Self::push_activity(
                                    &mut g,
                                    format!("Prior memory identity error: {e2:#}"),
                                );
                            }
                        }
                    }
                }
            }

            for doc in crate::prior_memory::PRIOR_MEMORIES {
                let labels = vec![
                    "source=exult".into(),
                    "kind=prior_memory".into(),
                    "authority=lived".into(),
                    format!("era={}", doc.era),
                ];
                let filename = format!("{}.md", doc.era);
                // Prefer document upload (async extraction). Fall back to a
                // literal /facts write so the prose is still searchable.
                let result = match client
                    .upload_markdown_document(&filename, doc.title, doc.body, &labels)
                    .await
                {
                    Ok(resp) => {
                        Ok(format!("Uploaded prior memory document: {} ({resp})", doc.title))
                    }
                    Err(upload_err) => {
                        tracing::warn!(error = %upload_err, era = doc.era, "document upload failed; trying /facts");
                        let opts = RememberOpts {
                            labels: labels.clone(),
                            infer: Some("none"),
                            memory_category: Some("knowledge"),
                        };
                        match client.remember_text_with(doc.body, opts).await {
                            Ok(()) => Ok(format!(
                                "Remembered prior memory via /facts (infer=none): {}",
                                doc.title
                            )),
                            Err(facts_err) => Err(format!(
                                "{}: upload={upload_err:#}; facts={facts_err:#}",
                                doc.era
                            )),
                        }
                    }
                };
                match result {
                    Ok(msg) => {
                        ok += 1;
                        let mut g = ui.lock().expect("ui lock");
                        Self::push_activity(&mut g, msg);
                    }
                    Err(e) => {
                        failures.push(e.clone());
                        let mut g = ui.lock().expect("ui lock");
                        Self::push_activity(&mut g, format!("Prior memory error: {e}"));
                    }
                }
            }
            let mut g = ui.lock().expect("ui lock");
            g.busy = false;
            if failures.is_empty() {
                g.status = format!(
                    "Avatar now holds memories of prior Britannia ({ok} writes).",
                );
                Self::push_mind(
                    &mut g,
                    ThoughtKind::Passing,
                    "I settle for a moment and let older Quests rise again - \
                     Virtues, companions, towns I walked before. Whatever waits \
                     in this age will find that ground already under my feet.",
                );
            } else {
                let first = failures.first().map(|s| s.as_str()).unwrap_or("unknown error");
                let short: String = first.chars().take(220).collect();
                g.status = format!(
                    "Prior memory partial: {ok} ok, {} failed. First error: {short}",
                    failures.len()
                );
            }
        });
    }

    fn ambient_suffix(ambient: &Option<Ambient>) -> String {
        ambient
            .as_ref()
            .map(|a| format!(" {}", a.describe()))
            .unwrap_or_default()
    }

    /// Track outdoor weather across travel batches. Returns `Some((from, to))` on a flip.
    fn note_outdoor_weather_flip(
        ui: &mut SharedUi,
        ambient: Option<&Ambient>,
    ) -> Option<(String, String)> {
        let Some(a) = ambient else {
            return None;
        };
        if a.setting != "outdoors" {
            return None;
        }
        let to = a.weather.clone();
        let flip = match ui.last_outdoor_weather.as_ref() {
            Some(from) if from != &to => Some((from.clone(), to.clone())),
            _ => None,
        };
        ui.last_outdoor_weather = Some(to);
        flip
    }

    /// Remember a place name from a sign so later examines can attach locality.
    fn note_place_hint(ui: &mut SharedUi, text_latin: &str) {
        let line = text_latin
            .lines()
            .map(str::trim)
            .find(|l| !l.is_empty())
            .unwrap_or("");
        if line.is_empty() || line.chars().count() > 48 {
            return;
        }
        ui.last_place_hint = Some(line.to_string());
    }

    /// Append a first-person thought to the mind feed (newest first).
    fn spawn_chat(
        &self,
        prompt: String,
        kind: ThoughtKind,
        status: String,
        log_ok: String,
    ) {
        let ui = Arc::clone(&self.ui);
        let client = self.client.clone();
        let entry_id = {
            let mut g = ui.lock().expect("ui lock");
            g.busy = true;
            g.status = status.clone();
            let id = Self::push_mind(&mut g, kind, kind.waiting());
            Self::push_activity(&mut g, format!("-> {log_ok}"));
            id
        };
        self.rt.spawn(async move {
            let reply = match client.chat(&prompt).await {
                Ok(r) => r,
                Err(e) => format!("Spectron error: {e:#}"),
            };
            let mut g = ui.lock().expect("ui lock");
            if let Some(entry) = g.mind.iter_mut().find(|e| e.id == entry_id) {
                entry.body = reply;
            } else {
                Self::push_mind(&mut g, kind, reply);
            }
            g.busy = false;
            g.status = status;
            Self::push_activity(&mut g, format!("<- {log_ok}"));
        });
    }

    fn spawn_greeting(&self, npc_id: i32, appears_as: String, ambient: Option<Ambient>) {
        // U7 often re-fires show_npc_face mid-talk (party whispers like Iolo, or the
        // same shopkeeper face again). Clearing here used to wipe Name/Job + the
        // whole transcript, so Bye remembered an empty conversation.
        let prev = {
            let g = self.ui.lock().expect("ui lock");
            g.active_npc.clone()
        };
        if let Some((prev_id, prev_as)) = prev {
            if prev_id == npc_id {
                return;
            }
            self.spawn_parting(prev_id, prev_as);
        }
        {
            let mut g = self.ui.lock().expect("ui lock");
            g.active_npc = Some((npc_id, appears_as.clone()));
            g.transcript.clear();
            g.copy_protect_prompted.clear();
            g.expect_name_reply = false;
            g.expect_job_reply = false;
            g.asked_name = false;
            g.asked_job = false;
            g.learned_name = None;
            g.learned_job = None;
        }
        let prompt = prompts::briefing_prompt(&appears_as, npc_id, ambient.as_ref());
        self.spawn_chat(
            prompt,
            ThoughtKind::Greeting,
            format!("Speaking with {appears_as}"),
            format!("greeting {appears_as}"),
        );
    }

    /// Persist an open conversation if one is in progress.
    ///
    /// Black Gate rarely emits `talk_end` (that intrinsic is SI-oriented). Bribes
    /// and other talks often end on a non-Bye choice, then combat or face-clear.
    /// Call this before combat / death so the transcript reaches Spectron.
    fn flush_open_talk(&self) {
        let pending = {
            let g = self.ui.lock().expect("ui lock");
            g.active_npc.clone()
        };
        if let Some((npc_id, appears_as)) = pending {
            self.spawn_parting(npc_id, appears_as);
        }
    }

    /// Unified situation snapshot → `/facts` (glance-first) + optional thoughts.
    fn ingest_situation(
        &self,
        trigger: &str,
        seen: &[String],
        mode: &str,
        steps: u32,
        distance: u32,
        heading: &str,
        via: Option<&str>,
        sextant: Option<&SextantFix>,
        ambient: Option<&Ambient>,
    ) {
        let since_sextant_line = {
            let mut g = self.ui.lock().expect("ui lock");
            if trigger == "relocated" || trigger == "pre_relocate" {
                // Jump invalidates the walk-feel from the last map reading.
                g.sextant_feel.clear();
            } else if let Some(sx) = sextant {
                // Checking the map resets feel to "I am at this reading."
                g.sextant_feel.reset_to_fix(sx.clone());
            } else if mode != "teleport" {
                // Ordinary stretch: accumulate steps since the last fix.
                g.sextant_feel.apply_stretch(heading, distance);
            }
            // After a fresh sextant, describe "still at the reading"; after walks,
            // describe the net offset (e.g. 10 steps E of 30°S, 60°E).
            g.sextant_feel.describe()
        };

        let amb = ambient.cloned();
        let body = prompts::remember_situation(
            trigger,
            seen,
            mode,
            steps,
            distance,
            heading,
            via,
            sextant,
            since_sextant_line.as_deref(),
            &Self::ambient_suffix(&amb),
        );
        let mut labels = vec![
            "source=exult".into(),
            "kind=situation".into(),
            format!("trigger={trigger}"),
            format!("mode={mode}"),
            format!("heading={heading}"),
            format!("net_tiles={distance}"),
            format!("steps={steps}"),
        ];
        if !seen.is_empty() {
            labels.push("has_seen=1".into());
            labels.push("authority=seen".into());
        }
        if sextant.is_some() {
            labels.push("has_sextant=1".into());
            labels.push("authority=written".into());
        }
        if since_sextant_line.is_some() {
            labels.push("has_sextant_feel=1".into());
        }
        if trigger == "relocated" {
            labels.push("kind=relocated".into());
        }
        self.spawn_remember_with(
            body,
            RememberOpts {
                labels,
                infer: None,
                memory_category: Some("context"),
            },
            format!("Remembered situation ({trigger}) {heading}"),
        );

        let (batch, weather_flip) = {
            let mut g = self.ui.lock().expect("ui lock");
            g.travel_batches = g.travel_batches.saturating_add(1);
            let batch = g.travel_batches;
            let flip = Self::note_outdoor_weather_flip(&mut g, ambient);
            (batch, flip)
        };
        if let Some((from, to)) = weather_flip {
            let prompt = prompts::weather_road_prompt(&from, &to, ambient);
            self.spawn_chat(
                prompt,
                ThoughtKind::Passing,
                format!("Weather turning ({from} -> {to})..."),
                format!("silence on the road ({from}->{to})"),
            );
        }

        if trigger == "relocated" {
            let prompt = prompts::relocated_thought_prompt(
                via.unwrap_or("teleport"),
                seen,
                ambient,
            );
            self.spawn_chat(
                prompt,
                ThoughtKind::Passing,
                format!("Suddenly elsewhere ({})...", via.unwrap_or("teleport")),
                format!("relocated via {}", via.unwrap_or("teleport")),
            );
            return;
        }

        if let Some(sx) = sextant {
            let label = format!(
                "{}{}, {}{}",
                sx.latitude, sx.latitude_hemi, sx.longitude, sx.longitude_hemi
            );
            let prompt = prompts::sextant_thought_prompt(
                sx.latitude,
                &sx.latitude_hemi,
                sx.longitude,
                &sx.longitude_hemi,
                ambient,
            );
            self.spawn_chat(
                prompt,
                ThoughtKind::Passing,
                format!("Sextant reading {label}..."),
                format!("sextant thought: {label}"),
            );
        }

        if trigger == "cadence" && batch.is_multiple_of(LOCATION_MUSE_EVERY_N_TRAVELS) {
            let glance_cue = if seen.is_empty() {
                String::new()
            } else {
                format!(" Things nearby at a glance: {}.", seen.join(", "))
            };
            let feel_cue = since_sextant_line
                .as_deref()
                .map(|s| format!(" {s}"))
                .unwrap_or_default();
            let conveyance = match mode {
                "ship" => "by ship",
                "cart" => "by cart",
                "carpet" => "on a magic carpet",
                _ => "on foot",
            };
            let cue = format!(
                "I have been travelling {conveyance} for a while, lately heading {heading} \
                 ({distance} net tiles in the last stretch).{glance_cue}{feel_cue}"
            );
            let prompt = prompts::location_muse_prompt(&cue, ambient);
            self.spawn_chat(
                prompt,
                ThoughtKind::Passing,
                format!("Musing on the road ({heading})..."),
                format!("travel location muse #{batch}"),
            );
        }
    }

    fn spawn_parting(&self, npc_id: i32, appears_as: String) {
        let (transcript, learned_name, learned_job, asked_name, asked_job) = {
            let mut g = self.ui.lock().expect("ui lock");
            // Already flushed (Bye then talk_end, or combat then death).
            if g.active_npc.is_none() {
                return;
            }
            g.active_npc = None;
            let t = std::mem::take(&mut g.transcript);
            let name = g.learned_name.take();
            let job = g.learned_job.take();
            let asked_name = g.asked_name;
            let asked_job = g.asked_job;
            g.expect_name_reply = false;
            g.expect_job_reply = false;
            g.asked_name = false;
            g.asked_job = false;
            g.copy_protect_prompted.clear();
            (t, name, job, asked_name, asked_job)
        };
        if transcript.trim().is_empty() {
            return;
        }
        let remember = prompts::remember_transcript(
            &appears_as,
            npc_id,
            &transcript,
            learned_name.as_deref(),
            learned_job.as_deref(),
            asked_name,
            asked_job,
        );
        let mut labels = vec![
            "source=exult".into(),
            "kind=conversation".into(),
            "authority=spoken".into(),
            format!("npc_id={npc_id}"),
        ];
        if asked_name {
            labels.push("asked_name=1".into());
        }
        if asked_job {
            labels.push("asked_job=1".into());
        }
        if learned_name.is_some() {
            labels.push("has_name=1".into());
        }
        if learned_job.is_some() {
            labels.push("has_job=1".into());
        }
        let who = learned_name
            .as_deref()
            .unwrap_or(appears_as.as_str());
        // Local stub so the People panel updates before Spectron replies.
        {
            let mut g = self.ui.lock().expect("ui lock");
            crate::npc_roster::upsert(
                &mut g.npc_roster,
                crate::npc_roster::NpcCard {
                    npc_id,
                    appears_as: appears_as.clone(),
                    name: learned_name.clone(),
                    job: learned_job.clone(),
                    summary: String::new(),
                },
            );
        }
        self.spawn_remember_with(
            remember,
            RememberOpts {
                labels,
                infer: None,
                memory_category: Some("context"),
            },
            format!("Remembered talk with {who}"),
        );
        let prompt = prompts::farewell_prompt(
            &appears_as,
            npc_id,
            &transcript,
            learned_name.as_deref(),
            learned_job.as_deref(),
            asked_name,
            asked_job,
        );
        self.spawn_chat(
            prompt,
            ThoughtKind::Parting,
            format!("Parting from {who}"),
            format!("parting {who}"),
        );
        self.spawn_npc_dossier_refresh(
            npc_id,
            appears_as,
            learned_name,
            learned_job,
            asked_name,
            asked_job,
            transcript,
        );
    }

    /// Ask Spectron what the Avatar knows about this person after a talk; update People.
    fn spawn_npc_dossier_refresh(
        &self,
        npc_id: i32,
        appears_as: String,
        learned_name: Option<String>,
        learned_job: Option<String>,
        asked_name: bool,
        asked_job: bool,
        transcript: String,
    ) {
        // Trivial Bye-only tails (after a face re-open wiped the real talk) are noise.
        let substantive = asked_name
            || asked_job
            || learned_name.is_some()
            || learned_job.is_some()
            || transcript.lines().count() >= 3;
        if !substantive {
            let mut g = self.ui.lock().expect("ui lock");
            Self::push_activity(
                &mut g,
                format!("Skipped thin dossier for {appears_as} (no Name/Job / short talk)"),
            );
            return;
        }
        let prompt = prompts::npc_dossier_prompt(
            &appears_as,
            learned_name.as_deref(),
            learned_job.as_deref(),
            asked_name,
            asked_job,
            &transcript,
        );
        let ui = Arc::clone(&self.ui);
        let client = self.client.clone();
        let who = learned_name
            .clone()
            .unwrap_or_else(|| appears_as.clone());
        self.rt.spawn(async move {
            {
                let mut g = ui.lock().expect("ui lock");
                Self::push_activity(&mut g, format!("-> Spectron dossier: {who}"));
            }
            match client.chat(&prompt).await {
                Ok(reply) => {
                    let cards = crate::npc_roster::parse_roster_reply(&reply);
                    let mut g = ui.lock().expect("ui lock");
                    if cards.is_empty() {
                        // Keep stub; store raw reply as summary so the test is visible.
                        crate::npc_roster::upsert(
                            &mut g.npc_roster,
                            crate::npc_roster::NpcCard {
                                npc_id,
                                appears_as,
                                name: learned_name,
                                job: learned_job,
                                summary: reply.trim().chars().take(400).collect(),
                            },
                        );
                        Self::push_activity(
                            &mut g,
                            format!("Dossier for {who}: unparsed reply (stored as summary)"),
                        );
                    } else {
                        for mut card in cards {
                            if card.npc_id <= 0 {
                                card.npc_id = npc_id;
                            }
                            if card.appears_as.is_empty() {
                                card.appears_as = appears_as.clone();
                            }
                            crate::npc_roster::upsert(&mut g.npc_roster, card);
                        }
                        Self::push_activity(&mut g, format!("Updated People entry: {who}"));
                    }
                }
                Err(e) => {
                    let mut g = ui.lock().expect("ui lock");
                    Self::push_activity(&mut g, format!("Dossier error ({who}): {e:#}"));
                }
            }
        });
    }

    /// One-shot: ask Spectron to list everyone spoken with; populate People panel.
    fn spawn_npc_roster_from_spectron(&self) {
        let ui = Arc::clone(&self.ui);
        let client = self.client.clone();
        {
            let mut g = ui.lock().expect("ui lock");
            g.busy = true;
            g.status = "Asking Spectron who I have met...".into();
            Self::push_activity(&mut g, "-> Spectron NPC list (full roster)");
        }
        let prompt = prompts::npc_roster_list_prompt();
        self.rt.spawn(async move {
            let result = client.chat(&prompt).await;
            let mut g = ui.lock().expect("ui lock");
            g.busy = false;
            match result {
                Ok(reply) => {
                    let cards = crate::npc_roster::parse_roster_reply(&reply);
                    if cards.is_empty() {
                        g.status = "Spectron NPC list: no parseable blocks (see activity)".into();
                        Self::push_activity(
                            &mut g,
                            format!(
                                "NPC list unparsed ({} chars). Reply starts: {}",
                                reply.len(),
                                reply.chars().take(180).collect::<String>()
                            ),
                        );
                    } else {
                        for card in cards {
                            crate::npc_roster::upsert(&mut g.npc_roster, card);
                        }
                        let n = g.npc_roster.len();
                        g.status = format!("People list updated from Spectron ({n} entries).");
                        Self::push_activity(&mut g, format!("Merged Spectron roster → {n} people"));
                    }
                }
                Err(e) => {
                    g.status = format!("NPC list error: {e:#}");
                    Self::push_activity(&mut g, format!("NPC list error: {e:#}"));
                }
            }
        });
    }

    fn handle_event(&self, event: GameEvent) {
        let event = match event {
            GameEvent::SignRead {
                gump,
                script,
                text_raw,
                text_latin,
                text_runes,
                ambient,
            } => {
                let text_runes = if text_runes.is_empty() {
                    if text_raw.is_empty() {
                        crate::runes::latin_to_unicode(&text_latin)
                    } else {
                        crate::runes::raw_encoding_to_unicode(&text_raw)
                    }
                } else {
                    text_runes
                };
                GameEvent::SignRead {
                    gump,
                    script,
                    text_raw,
                    text_latin,
                    text_runes,
                    ambient,
                }
            }
            other => other,
        };
        {
            let mut g = self.ui.lock().expect("ui lock");
            // One readable line per event; JSON stays on hover (and on the bridge debug log).
            Self::push_chronicle(&mut g, event.chronicle_display());
        }
        match event {
            GameEvent::TalkStart {
                npc_id,
                appears_as,
                ambient,
            } => {
                self.spawn_greeting(npc_id, appears_as, ambient);
            }
            GameEvent::TalkLine { text, .. } => {
                let (appears_as, should_prompt, fingerprint, hit) = {
                    let mut g = self.ui.lock().expect("ui lock");
                    if !g.transcript.is_empty() {
                        g.transcript.push('\n');
                    }
                    g.transcript.push_str(&text);
                    // Capture Name / Job replies (first NPC line after that menu choice).
                    let trimmed = text.trim();
                    if !trimmed.is_empty() {
                        if g.expect_name_reply {
                            if g.learned_name.is_none() {
                                // Refusal ("not important") → None; still counted as asked.
                                g.learned_name =
                                    crate::talk_slots::normalize_personal_name(trimmed);
                            }
                            g.expect_name_reply = false;
                        } else if g.expect_job_reply {
                            if g.learned_job.is_none() {
                                g.learned_job = crate::talk_slots::normalize_job(trimmed);
                            }
                            g.expect_job_reply = false;
                        }
                    }
                    let appears_as = g
                        .active_npc
                        .as_ref()
                        .map(|(_, a)| a.clone())
                        .unwrap_or_else(|| "someone".into());
                    if let Some((fingerprint, hit)) = crate::copy_protect::match_question(&text) {
                        if g.copy_protect_prompted.insert(fingerprint.to_string()) {
                            (appears_as, true, fingerprint.to_string(), Some(hit))
                        } else {
                            (appears_as, false, String::new(), None)
                        }
                    } else {
                        (appears_as, false, String::new(), None)
                    }
                };
                if should_prompt {
                    if let Some(hit) = hit {
                        let prompt = prompts::copy_protect_thought_prompt(
                            &appears_as,
                            text.trim(),
                            hit.answer,
                            hit.source,
                        );
                        self.spawn_chat(
                            prompt,
                            ThoughtKind::Passing,
                            format!("Copy-protection recall -> {}", hit.answer),
                            format!("copy-protect {fingerprint} -> {}", hit.answer),
                        );
                    }
                }
            }
            GameEvent::TalkChoice {
                npc_id,
                appears_as,
                choice,
            } => {
                {
                    let mut g = self.ui.lock().expect("ui lock");
                    if !g.transcript.is_empty() {
                        g.transcript.push('\n');
                    }
                    g.transcript
                        .push_str(&format!("Avatar chooses: {choice}"));
                    // U7 always offers Name | Job | Bye; record which basics were asked.
                    if Self::is_name_choice(&choice) {
                        g.asked_name = true;
                        g.expect_name_reply = true;
                        g.expect_job_reply = false;
                    } else if Self::is_job_choice(&choice) {
                        g.asked_job = true;
                        g.expect_job_reply = true;
                        g.expect_name_reply = false;
                    } else {
                        g.expect_name_reply = false;
                        g.expect_job_reply = false;
                    }
                }
                if choice.eq_ignore_ascii_case("bye")
                    || choice.eq_ignore_ascii_case("goodbye")
                    || choice.eq_ignore_ascii_case("farewell")
                {
                    self.spawn_parting(npc_id, appears_as);
                }
            }
            GameEvent::TalkEnd {
                npc_id, appears_as, ..
            } => {
                let should = {
                    let g = self.ui.lock().expect("ui lock");
                    g.active_npc.is_some()
                };
                if should {
                    self.spawn_parting(npc_id, appears_as);
                }
            }
            GameEvent::BookRead { title, text } => {
                let cleaned = crate::books::normalize_book_text(&text);
                let resolved = crate::books::resolve_title(title.as_deref(), &cleaned);
                let label = resolved
                    .clone()
                    .unwrap_or_else(|| "untitled book".into());
                let body = prompts::remember_book(resolved.as_deref(), &cleaned);
                self.spawn_remember_with(
                    body,
                    RememberOpts {
                        labels: vec![
                            "source=exult".into(),
                            "kind=book".into(),
                            "authority=written".into(),
                            "epistemic=read".into(),
                        ],
                        infer: None,
                        memory_category: Some("knowledge"),
                    },
                    format!("Remembered book (read, not lived): {label}"),
                );
            }
            GameEvent::SignRead {
                gump,
                script,
                text_raw,
                text_latin,
                text_runes,
                ambient,
            } => {
                {
                    let mut g = self.ui.lock().expect("ui lock");
                    Self::note_place_hint(&mut g, &text_latin);
                }
                let ambient_sfx = Self::ambient_suffix(&ambient);
                let body = prompts::remember_sign(
                    &gump,
                    &script,
                    &text_latin,
                    &text_raw,
                    &text_runes,
                    &ambient_sfx,
                );
                self.spawn_remember_with(
                    body,
                    RememberOpts {
                        labels: vec![
                            "source=exult".into(),
                            "kind=sign".into(),
                            "authority=written".into(),
                            "epistemic=read".into(),
                            format!("sign={text_latin}"),
                        ],
                        infer: None,
                        memory_category: Some("knowledge"),
                    },
                    format!("Remembered sign [{script}]: {text_latin}"),
                );
                let prompt = prompts::sign_thought_prompt(
                    &script,
                    &text_latin,
                    &text_runes,
                    ambient.as_ref(),
                );
                self.spawn_chat(
                    prompt,
                    ThoughtKind::Passing,
                    format!("Reading a {script} sign"),
                    format!("sign thought [{script}]"),
                );
            }
            GameEvent::Examine {
                text,
                quantity,
                npc_id,
                ..
            } => {
                let name = text.trim();
                if name.is_empty() {
                    return;
                }
                let place = {
                    let g = self.ui.lock().expect("ui lock");
                    g.last_place_hint.clone()
                };
                let body =
                    prompts::remember_examine(name, quantity, npc_id, place.as_deref());
                let mut labels = vec![
                    "source=exult".into(),
                    "kind=examine".into(),
                    "authority=seen".into(),
                    "epistemic=examined".into(),
                ];
                if npc_id > 0 {
                    labels.push(format!("npc_id={npc_id}"));
                }
                if let Some(p) = place.as_deref() {
                    labels.push(format!("place={p}"));
                }
                self.spawn_remember_with(
                    body,
                    RememberOpts {
                        labels,
                        infer: None,
                        memory_category: Some("knowledge"),
                    },
                    format!("Remembered examine: {name}"),
                );
            }
            GameEvent::ContainerOpened {
                container,
                empty,
                items,
                ..
            } => {
                let place = {
                    let g = self.ui.lock().expect("ui lock");
                    g.last_place_hint.clone()
                };
                let item_pairs: Vec<(String, i32)> = items
                    .iter()
                    .map(|i| (i.name.clone(), i.quantity.max(1)))
                    .collect();
                let body = prompts::remember_container(
                    &container,
                    &item_pairs,
                    empty,
                    place.as_deref(),
                );
                let mut labels = vec![
                    "source=exult".into(),
                    "kind=container_seen".into(),
                    "authority=seen".into(),
                    "epistemic=examined".into(),
                    format!("container={container}"),
                ];
                if let Some(p) = place.as_deref() {
                    labels.push(format!("place={p}"));
                }
                for (name, _) in item_pairs.iter().take(16) {
                    labels.push(format!("item={name}"));
                }
                let log = if empty || items.is_empty() {
                    format!("Remembered empty container: {container}")
                } else {
                    format!(
                        "Remembered contents of {container} ({} kinds)",
                        items.len()
                    )
                };
                self.spawn_remember_with(
                    body,
                    RememberOpts {
                        labels,
                        infer: None,
                        memory_category: Some("knowledge"),
                    },
                    log,
                );
            }
            GameEvent::CombatStart { ambient } => {
                self.flush_open_talk();
                let body = format!(
                    "The Avatar entered combat mode.{}",
                    Self::ambient_suffix(&ambient)
                );
                self.spawn_remember(body, "Remembered combat start".into());
            }
            GameEvent::CombatEnd { ambient } => {
                self.flush_open_talk();
                let body = format!(
                    "The Avatar left combat mode.{}",
                    Self::ambient_suffix(&ambient)
                );
                self.spawn_remember(body, "Remembered combat end".into());
            }
            GameEvent::SleepStart { ambient } => {
                let body = format!(
                    "The Avatar went to sleep.{}",
                    Self::ambient_suffix(&ambient)
                );
                self.spawn_remember(body, "Remembered sleep start".into());
            }
            GameEvent::SleepEnd { ambient } => {
                let body = format!("The Avatar woke up.{}", Self::ambient_suffix(&ambient));
                self.spawn_remember(body, "Remembered sleep end".into());
            }
            GameEvent::DungeonEnter { ambient } => {
                let body = format!(
                    "The Avatar entered a dungeon (underground).{}",
                    Self::ambient_suffix(&ambient)
                );
                self.spawn_remember(body, "Remembered dungeon enter".into());
                let prompt = prompts::location_muse_prompt(
                    "I have just stepped underground into dungeon lighting.",
                    ambient.as_ref(),
                );
                self.spawn_chat(
                    prompt,
                    ThoughtKind::Passing,
                    "Wondering where I am underground...".into(),
                    "dungeon location muse".into(),
                );
            }
            GameEvent::DungeonLeave { ambient } => {
                let body = format!(
                    "The Avatar returned outdoors from underground.{}",
                    Self::ambient_suffix(&ambient)
                );
                self.spawn_remember(body, "Remembered dungeon leave".into());
            }
            GameEvent::Situation {
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
                self.ingest_situation(
                    &trigger,
                    &seen,
                    &mode,
                    steps,
                    distance,
                    &heading,
                    via.as_deref(),
                    sextant.as_ref(),
                    ambient.as_ref(),
                );
            }
            GameEvent::Travel {
                mode,
                steps,
                distance,
                heading,
                in_view,
                ambient,
                ..
            } => {
                // Legacy demo / older bridge payloads → same situation path.
                self.ingest_situation(
                    "cadence",
                    &in_view,
                    &mode,
                    steps,
                    distance,
                    &heading,
                    None,
                    None,
                    ambient.as_ref(),
                );
            }
            GameEvent::Death { ambient } => {
                self.flush_open_talk();
                let body = format!(
                    "The Avatar has died and will be returned to life \
                     (Paws / monk hourglass depending on game).{}",
                    Self::ambient_suffix(&ambient)
                );
                self.spawn_remember(body, "Remembered death".into());
                let prompt = prompts::death_thought_prompt(ambient.as_ref());
                self.spawn_chat(
                    prompt,
                    ThoughtKind::Passing,
                    "I have fallen...".into(),
                    "death thought".into(),
                );
            }
            GameEvent::SextantReading {
                latitude,
                latitude_hemi,
                longitude,
                longitude_hemi,
                ambient,
            } => {
                let fix = SextantFix {
                    latitude,
                    latitude_hemi,
                    longitude,
                    longitude_hemi,
                };
                self.ingest_situation(
                    "sextant",
                    &[],
                    "foot",
                    0,
                    0,
                    "none",
                    None,
                    Some(&fix),
                    ambient.as_ref(),
                );
            }
            GameEvent::TimeReading {
                text,
                source,
                hour,
                minute,
                ambient,
            } => {
                let body = prompts::remember_time_reading(&text, &source, hour, minute);
                self.spawn_remember_with(
                    body,
                    RememberOpts {
                        labels: vec![
                            "source=exult".into(),
                            "kind=time_reading".into(),
                            "authority=seen".into(),
                            format!("timepiece={source}"),
                        ],
                        infer: None,
                        memory_category: Some("context"),
                    },
                    format!("Remembered time: {text}"),
                );
                let prompt =
                    prompts::time_reading_thought_prompt(&text, &source, ambient.as_ref());
                self.spawn_chat(
                    prompt,
                    ThoughtKind::Passing,
                    format!("Checking the {source}..."),
                    format!("time reading: {text}"),
                );
            }
            GameEvent::CompanionJoin {
                npc_id,
                appears_as,
                ambient,
            } => {
                let body = format!(
                    "Ultima VII: {appears_as} joined the Avatar's party.{}",
                    Self::ambient_suffix(&ambient)
                );
                self.spawn_remember_with(
                    body,
                    RememberOpts {
                        labels: vec![
                            "source=exult".into(),
                            "kind=companion".into(),
                            "authority=lived".into(),
                            format!("npc_id={npc_id}"),
                        ],
                        infer: None,
                        memory_category: Some("context"),
                    },
                    format!("Remembered companion join: {appears_as}"),
                );
                let prompt =
                    prompts::companion_thought_prompt(&appears_as, true, ambient.as_ref());
                self.spawn_chat(
                    prompt,
                    ThoughtKind::Passing,
                    format!("{appears_as} joined..."),
                    format!("companion join thought: {appears_as}"),
                );
            }
            GameEvent::CompanionLeave {
                npc_id,
                appears_as,
                ambient,
            } => {
                let body = format!(
                    "Ultima VII: {appears_as} left the Avatar's party.{}",
                    Self::ambient_suffix(&ambient)
                );
                self.spawn_remember_with(
                    body,
                    RememberOpts {
                        labels: vec![
                            "source=exult".into(),
                            "kind=companion".into(),
                            "authority=lived".into(),
                            format!("npc_id={npc_id}"),
                        ],
                        infer: None,
                        memory_category: Some("context"),
                    },
                    format!("Remembered companion leave: {appears_as}"),
                );
                let prompt =
                    prompts::companion_thought_prompt(&appears_as, false, ambient.as_ref());
                self.spawn_chat(
                    prompt,
                    ThoughtKind::Passing,
                    format!("{appears_as} left..."),
                    format!("companion leave thought: {appears_as}"),
                );
            }
            GameEvent::PartyKnockedOut {
                npc_id,
                appears_as,
                is_avatar,
                ambient,
            } => {
                let body = if is_avatar {
                    format!(
                        "Ultima VII: the Avatar was knocked down in combat (HP ≤ 0, not dead). \
                         They will lie there until safe to rise.{}",
                        Self::ambient_suffix(&ambient)
                    )
                } else {
                    format!(
                        "Ultima VII: companion {appears_as} was knocked down \
                         in combat (HP ≤ 0, not dead) and may rise if left alone.{}",
                        Self::ambient_suffix(&ambient)
                    )
                };
                self.spawn_remember_with(
                    body,
                    RememberOpts {
                        labels: vec![
                            "source=exult".into(),
                            "kind=knocked_out".into(),
                            "authority=lived".into(),
                            format!("npc_id={npc_id}"),
                        ],
                        infer: None,
                        memory_category: Some("context"),
                    },
                    format!("Remembered knock-out: {appears_as}"),
                );
                let prompt = prompts::knocked_out_thought_prompt(
                    &appears_as,
                    is_avatar,
                    ambient.as_ref(),
                );
                self.spawn_chat(
                    prompt,
                    ThoughtKind::Passing,
                    if is_avatar {
                        "I am down...".into()
                    } else {
                        format!("{appears_as} is down...")
                    },
                    format!("knocked out: {appears_as}"),
                );
            }
            GameEvent::CompanionDied {
                npc_id,
                appears_as,
                ambient,
            } => {
                let body = format!(
                    "Ultima VII: companion {appears_as} has died in combat \
                     (true death — body left; not a temporary knock-out).{}",
                    Self::ambient_suffix(&ambient)
                );
                self.spawn_remember_with(
                    body,
                    RememberOpts {
                        labels: vec![
                            "source=exult".into(),
                            "kind=companion_death".into(),
                            "authority=lived".into(),
                            format!("npc_id={npc_id}"),
                        ],
                        infer: None,
                        memory_category: Some("context"),
                    },
                    format!("Remembered companion death: {appears_as}"),
                );
                let prompt =
                    prompts::companion_died_thought_prompt(&appears_as, ambient.as_ref());
                self.spawn_chat(
                    prompt,
                    ThoughtKind::Passing,
                    format!("{appears_as} has fallen..."),
                    format!("companion died: {appears_as}"),
                );
            }
            GameEvent::Relocated {
                via,
                distance,
                in_view,
                ambient,
            } => {
                self.ingest_situation(
                    "relocated",
                    &in_view,
                    "teleport",
                    0,
                    distance,
                    "sudden",
                    Some(&via),
                    None,
                    ambient.as_ref(),
                );
            }
        }
    }

    /// Demo event injectors (narrow Demo panel — stacked, not wrapped).
    fn paint_demo_buttons(&self, ui: &mut egui::Ui) {
        let full = ui.available_width();
        let btn = |ui: &mut egui::Ui, label: &str| {
            ui.add_sized([full, 24.0], egui::Button::new(label))
        };
        if btn(ui, "Talk start: a boy").clicked() {
            let _ = self.events_tx.send(GameEvent::TalkStart {
                npc_id: 2,
                appears_as: "a boy".into(),
                ambient: None,
            });
        }
        if btn(ui, "Line").clicked() {
            let _ = self.events_tx.send(GameEvent::TalkLine {
                npc_id: 2,
                appears_as: "a boy".into(),
                text: "You see a cheerful young boy.".into(),
            });
        }
        if btn(ui, "Say Bye").clicked() {
            let _ = self.events_tx.send(GameEvent::TalkChoice {
                npc_id: 2,
                appears_as: "a boy".into(),
                choice: "Bye".into(),
            });
        }
        if btn(ui, "Fake book").clicked() {
            let _ = self.events_tx.send(GameEvent::BookRead {
                title: Some("The Book of Fellowship".into()),
                text: "The Fellowship welcomes all who seek enlightenment...".into(),
            });
        }
        if btn(ui, "Fake death").clicked() {
            let _ = self.events_tx.send(GameEvent::Death { ambient: None });
        }
        if btn(ui, "Fake sextant").clicked() {
            let _ = self.events_tx.send(GameEvent::SextantReading {
                latitude: 30,
                latitude_hemi: "S".into(),
                longitude: 60,
                longitude_hemi: "E".into(),
                ambient: None,
            });
        }
        if btn(ui, "Fake sign").clicked() {
            let _ = self.events_tx.send(GameEvent::SignRead {
                gump: "woodsign".into(),
                script: "runic".into(),
                text_raw: "trinsic".into(),
                text_latin: "TRINSIC".into(),
                text_runes: crate::runes::latin_to_unicode("TRINSIC"),
                ambient: Some(Ambient {
                    time_of_day: "day".into(),
                    weather: "clear".into(),
                    setting: "outdoors".into(),
                }),
            });
        }
        if btn(ui, "Fake examine").clicked() {
            let _ = self.events_tx.send(GameEvent::Examine {
                text: "lit lamp".into(),
                shape: 526,
                frame: 0,
                quantity: 1,
                npc_id: -1,
            });
        }
        if btn(ui, "Fake chest").clicked() {
            let _ = self.events_tx.send(GameEvent::ContainerOpened {
                container: "chest".into(),
                shape: 800,
                empty: false,
                items: vec![
                    crate::events::ContainerItem {
                        name: "gold".into(),
                        quantity: 12,
                        shape: 644,
                    },
                    crate::events::ContainerItem {
                        name: "torch".into(),
                        quantity: 3,
                        shape: 595,
                    },
                    crate::events::ContainerItem {
                        name: "lockpick".into(),
                        quantity: 1,
                        shape: 627,
                    },
                ],
                ambient: Some(Ambient {
                    time_of_day: "day".into(),
                    weather: "clear".into(),
                    setting: "outdoors".into(),
                }),
            });
        }
        if btn(ui, "Weather flip").clicked() {
            // Two outdoor travel batches: clear -> rain triggers silence-on-the-road.
            let clear = Ambient {
                time_of_day: "day".into(),
                weather: "clear".into(),
                setting: "outdoors".into(),
            };
            let rain = Ambient {
                time_of_day: "dusk".into(),
                weather: "rain".into(),
                setting: "outdoors".into(),
            };
            let _ = self.events_tx.send(GameEvent::Travel {
                mode: "foot".into(),
                steps: 25,
                distance: 30,
                heading: "north".into(),
                short: 10,
                medium: 10,
                long: 5,
                in_view: vec!["road".into(), "tree".into()],
                ambient: Some(clear),
            });
            let _ = self.events_tx.send(GameEvent::Travel {
                mode: "foot".into(),
                steps: 25,
                distance: 28,
                heading: "north".into(),
                short: 8,
                medium: 10,
                long: 7,
                in_view: vec!["road".into(), "tree".into()],
                ambient: Some(rain),
            });
        }
        if btn(ui, "Fake time (watch)").clicked() {
            let _ = self.events_tx.send(GameEvent::TimeReading {
                text: "10 o'clock".into(),
                source: "pocketwatch".into(),
                hour: 10,
                minute: 0,
                ambient: Some(Ambient {
                    time_of_day: "day".into(),
                    weather: "clear".into(),
                    setting: "outdoors".into(),
                }),
            });
        }
        if btn(ui, "Fake travel (ship)").clicked() {
            let _ = self.events_tx.send(GameEvent::Travel {
                mode: "ship".into(),
                steps: 40,
                distance: 36,
                heading: "east".into(),
                short: 0,
                medium: 40,
                long: 0,
                in_view: vec!["ship".into(), "water".into(), "dock".into()],
                ambient: Some(Ambient {
                    time_of_day: "day".into(),
                    weather: "clear".into(),
                    setting: "outdoors".into(),
                }),
            });
        }
        if btn(ui, "Fake knock-out (Avatar)").clicked() {
            let _ = self.events_tx.send(GameEvent::PartyKnockedOut {
                npc_id: 0,
                appears_as: "the Avatar".into(),
                is_avatar: true,
                ambient: Some(Ambient {
                    time_of_day: "day".into(),
                    weather: "clear".into(),
                    setting: "outdoors".into(),
                }),
            });
        }
        if btn(ui, "Fake companion died").clicked() {
            let _ = self.events_tx.send(GameEvent::CompanionDied {
                npc_id: 1,
                appears_as: "Iolo".into(),
                ambient: Some(Ambient {
                    time_of_day: "day".into(),
                    weather: "clear".into(),
                    setting: "outdoors".into(),
                }),
            });
        }
        if btn(ui, "Fake moongate relocate").clicked() {
            let _ = self.events_tx.send(GameEvent::Relocated {
                via: "teleport".into(),
                distance: 240,
                in_view: vec![
                    "moongate".into(),
                    "house".into(),
                    "a townsman".into(),
                ],
                ambient: Some(Ambient {
                    time_of_day: "night".into(),
                    weather: "clear".into(),
                    setting: "outdoors".into(),
                }),
            });
        }
    }
}

impl eframe::App for SidecarApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        while let Ok(event) = self.events_rx.try_recv() {
            self.handle_event(event);
        }

        theme::apply_visuals(ctx);

        egui::TopBottomPanel::top("top")
            .frame(theme::panel_frame(STONE))
            .show(ctx, |ui| {
                ui.horizontal(|ui| {
                    ui.colored_label(CREST, "*");
                    theme::paint_shadowed_label(ui, "The Avatar's Mind", 26.0);
                    ui.add_space(12.0);
                    let g = self.ui.lock().expect("ui lock");
                    // Prefer status when busy / reporting remember errors; otherwise
                    // the compact listen line (Fondamento lacks a unicode arrow glyph).
                    let listen = format!(
                        "Listening on http://{}, events at /event",
                        self.listen_addr
                    );
                    let line = if g.status == listen || g.status.starts_with("Listening on http://")
                    {
                        listen
                    } else {
                        g.status.clone()
                    };
                    ui.label(theme::gold_muted(line));
                    if g.busy {
                        ui.spinner();
                    }
                });
                ui.add_space(4.0);
                ui.horizontal_wrapped(|ui| {
                    let prior = ui
                        .button("Load Avatar's previous memories")
                        .on_hover_text(
                            "Ingest Ultima IV-VI lived memory into this Spectron Context \
                             (Virtues, companions, towns). Safe to run once per Context; \
                             re-running may duplicate facts.",
                        );
                    if prior.clicked() {
                        self.spawn_prior_britannia_memories();
                    }
                    let roster_btn = ui
                        .button("Ask Spectron: who have I met?")
                        .on_hover_text(
                            "One-shot /chat: list everyone spoken with in this Context. \
                             Fills the People panel. Re-run after more talks to refresh. \
                             Each Bye also updates that person's card.",
                        );
                    if roster_btn.clicked() {
                        self.spawn_npc_roster_from_spectron();
                    }
                    ui.separator();
                    Self::paint_panel_toggle(ui, "People", &mut self.show_people);
                    Self::paint_panel_toggle(ui, "Chronicle", &mut self.show_chronicle);
                    Self::paint_panel_toggle(ui, "Thoughts", &mut self.show_thoughts);
                    Self::paint_panel_toggle(ui, "Demo", &mut self.show_demo);
                });
            });

        // Demo outermost on the right (closed by default).
        egui::SidePanel::right("demo")
            .default_width(250.0)
            .resizable(true)
            .frame(theme::panel_frame(STONE_LIGHT))
            .show_animated(ctx, self.show_demo, |ui| {
                Self::paint_panel_title(ui, "Demo", &mut self.show_demo);
                ui.add_space(4.0);
                ui.label(theme::gold_muted(
                    "Inject fake game events without playing.",
                ));
                ui.add_space(6.0);
                egui::ScrollArea::vertical()
                    .id_salt("demo_panel")
                    .auto_shrink([false, false])
                    .show(ui, |ui| {
                        self.paint_demo_buttons(ui);
                    });
            });

        // People I have met — Spectron dossier test surface (left of chronicle).
        egui::SidePanel::left("people")
            .default_width(280.0)
            .resizable(true)
            .frame(theme::panel_frame(STONE_LIGHT))
            .show_animated(ctx, self.show_people, |ui| {
                let n = {
                    let g = self.ui.lock().expect("ui lock");
                    g.npc_roster.len()
                };
                Self::paint_panel_title(ui, &format!("People ({n})"), &mut self.show_people);
                ui.add_space(4.0);
                ui.label(theme::gold_muted(
                    "Spectron's view of who I have met. Bye refreshes one card; \
                     Ask Spectron rebuilds the list.",
                ));
                ui.add_space(8.0);
                egui::Frame::new()
                    .fill(theme::PARCHMENT)
                    .stroke(egui::Stroke::new(2.0, WOOD))
                    .corner_radius(4.0)
                    .inner_margin(egui::Margin::same(10))
                    .show(ui, |ui| {
                        egui::ScrollArea::vertical()
                            .id_salt("npc_roster")
                            .auto_shrink([false, false])
                            .show(ui, |ui| {
                                let g = self.ui.lock().expect("ui lock");
                                if g.npc_roster.is_empty() {
                                    ui.label(
                                        egui::RichText::new(
                                            "(Empty — speak with someone and say Bye, \
                                             or ask Spectron who you have met.)",
                                        )
                                        .color(theme::INK)
                                        .size(14.0),
                                    );
                                }
                                for card in &g.npc_roster {
                                    ui.add(
                                        egui::Label::new(
                                            egui::RichText::new(
                                                crate::npc_roster::format_card_lines(card),
                                            )
                                            .color(theme::INK)
                                            .size(13.0),
                                        )
                                        .wrap()
                                        .selectable(true),
                                    );
                                    ui.add_space(8.0);
                                    ui.separator();
                                    ui.add_space(6.0);
                                }
                            });
                    });
            });

        // Mind feed sits to the right of the chronicle (newest first, like events).
        egui::SidePanel::right("mind")
            .default_width(380.0)
            .resizable(true)
            .frame(theme::panel_frame(STONE_DEEP))
            .show_animated(ctx, self.show_thoughts, |ui| {
                let mind_n = {
                    let g = self.ui.lock().expect("ui lock");
                    g.mind.len()
                };
                Self::paint_panel_title(
                    ui,
                    &format!("Thoughts ({mind_n})"),
                    &mut self.show_thoughts,
                );
                ui.add_space(4.0);
                ui.label(theme::gold_muted(
                    "Newest first - scroll down for earlier thoughts. Spectron \
                     distills what the Avatar may be thinking as the adventure \
                     continues, from thoughts when talking with people to \
                     passing thoughts on the road, when the weather turns, \
                     and so on.",
                ));
                ui.add_space(8.0);

                egui::Frame::new()
                    .fill(theme::PARCHMENT)
                    .stroke(egui::Stroke::new(2.0, WOOD))
                    .corner_radius(4.0)
                    .inner_margin(egui::Margin::same(12))
                    .show(ui, |ui| {
                        egui::ScrollArea::vertical()
                            .id_salt("mind_feed")
                            .auto_shrink([false, false])
                            .stick_to_bottom(false)
                            .show(ui, |ui| {
                                let g = self.ui.lock().expect("ui lock");
                                if g.mind.is_empty() {
                                    ui.label(
                                        egui::RichText::new(
                                            "(Quiet for now - speak with someone, walk, \
                                             or use the demo tools.)",
                                        )
                                        .color(theme::INK)
                                        .size(15.0),
                                    );
                                }
                                for entry in &g.mind {
                                    ui.horizontal(|ui| {
                                        ui.label(
                                            egui::RichText::new("*")
                                                .color(entry.kind.bullet())
                                                .size(16.0),
                                        );
                                        ui.label(
                                            egui::RichText::new(entry.kind.label())
                                                .color(theme::GOLD_DIM)
                                                .size(13.0)
                                                .family(egui::FontFamily::Name(
                                                    theme::FONT_BODY.into(),
                                                )),
                                        );
                                    });
                                    theme::paint_shadowed_paragraph(ui, &entry.body, 15.0);
                                    ui.add_space(10.0);
                                }
                            });
                    });

                ui.add_space(8.0);
                ui.label(theme::gold_muted("Spectron activity"));
                egui::ScrollArea::vertical()
                    .id_salt("activity")
                    .max_height(90.0)
                    .show(ui, |ui| {
                        let g = self.ui.lock().expect("ui lock");
                        if g.activity.is_empty() {
                            ui.label(theme::gold_muted("(none yet)"));
                        }
                        for line in &g.activity {
                            ui.add(egui::Label::new(theme::gold_muted(line)).selectable(true));
                        }
                    });
            });

        egui::CentralPanel::default()
            .frame(
                egui::Frame::new()
                    .fill(STONE)
                    .inner_margin(egui::Margin::same(12)),
            )
            .show(ctx, |ui| {
                let chronicle_n = {
                    let g = self.ui.lock().expect("ui lock");
                    g.chronicle.len()
                };
                if !self.show_chronicle {
                    // Collapsed: keep the title visible so it can be reopened here too.
                    let open = ui
                        .add(
                            egui::Label::new(
                                egui::RichText::new(format!("[+] Chronicle ({chronicle_n})"))
                                    .color(theme::GOLD)
                                    .size(22.0),
                            )
                            .sense(egui::Sense::click()),
                        )
                        .on_hover_text("Click to open Chronicle");
                    if open.clicked() {
                        self.show_chronicle = true;
                    }
                    ui.add_space(4.0);
                    ui.label(theme::gold_muted(
                        "Closed — open from the top bar, or click the title above.",
                    ));
                    return;
                }

                Self::paint_panel_title(
                    ui,
                    &format!("Chronicle ({chronicle_n})"),
                    &mut self.show_chronicle,
                );
                ui.add_space(4.0);
                ui.label(theme::gold_muted(
                    "Newest first - scroll down for earlier events (talk, travel, books, ...)",
                ));
                ui.add_space(8.0);

                egui::Frame::new()
                    .fill(theme::PARCHMENT)
                    .stroke(egui::Stroke::new(2.0, WOOD))
                    .corner_radius(4.0)
                    .inner_margin(egui::Margin::same(12))
                    .show(ui, |ui| {
                        egui::ScrollArea::vertical()
                            .id_salt("chronicle")
                            .auto_shrink([false, false])
                            .stick_to_bottom(false)
                            .show(ui, |ui| {
                                let g = self.ui.lock().expect("ui lock");
                                if g.chronicle.is_empty() {
                                    ui.label(
                                        egui::RichText::new(
                                            "(No events yet - walk Britannia, or use the demo buttons.)",
                                        )
                                        .color(theme::INK)
                                        .size(15.0),
                                    );
                                }
                                for line in &g.chronicle {
                                    let bullet = chronicle_bullet_colour(line);
                                    ui.horizontal(|ui| {
                                        ui.label(
                                            egui::RichText::new("*")
                                                .color(bullet)
                                                .size(16.0),
                                        );
                                        ui.add(
                                            egui::Label::new(
                                                egui::RichText::new(line)
                                                    .color(theme::INK)
                                                    .size(15.0),
                                            )
                                            .wrap()
                                            .selectable(true),
                                        );
                                    });
                                    ui.add_space(3.0);
                                }
                            });
                    });
            });

        ctx.request_repaint_after(std::time::Duration::from_millis(100));
    }
}

impl SidecarApp {
    /// U7 conversation menu: Name (sometimes "who art thou", etc.).
    fn is_name_choice(choice: &str) -> bool {
        let c = choice.trim();
        c.eq_ignore_ascii_case("name")
            || c.eq_ignore_ascii_case("thy name")
            || c.eq_ignore_ascii_case("your name")
            || c.eq_ignore_ascii_case("who art thou")
            || c.eq_ignore_ascii_case("who are you")
    }

    /// U7 conversation menu: Job.
    fn is_job_choice(choice: &str) -> bool {
        let c = choice.trim();
        c.eq_ignore_ascii_case("job")
            || c.eq_ignore_ascii_case("thy job")
            || c.eq_ignore_ascii_case("your job")
            || c.eq_ignore_ascii_case("what dost thou do")
            || c.eq_ignore_ascii_case("what do you do")
            || c.eq_ignore_ascii_case("occupation")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn name_and_job_menu_choices() {
        assert!(SidecarApp::is_name_choice("Name"));
        assert!(SidecarApp::is_name_choice("name"));
        assert!(SidecarApp::is_job_choice("Job"));
        assert!(!SidecarApp::is_name_choice("bye"));
        assert!(!SidecarApp::is_job_choice("fellowship"));
    }
}
