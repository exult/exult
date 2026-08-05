//! End-to-end pipeline capture for Spectron handoff packs.
//!
//! Each sidecar run that has handoff enabled writes under
//! `handoff/sessions/<stamp>/`:
//! - `meta.json` — Context id, base URL (no API key), env notes
//! - `pipeline.jsonl` — one JSON object per line: Exult event in, facts/chat out
//!
//! Enable with `SPECTRON_SIDECAR_HANDOFF=1` (or unset to leave off). Disable with `0`.

use std::fs::{self, File, OpenOptions};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};

use serde_json::{json, Value};

pub struct SessionLog {
    dir: PathBuf,
    file: Mutex<File>,
}

impl SessionLog {
    /// Start a new session directory under `handoff/sessions/`. Returns `None` if disabled.
    pub fn maybe_start(
        enabled: bool,
        base_url: &str,
        context_id: &str,
        listen: &str,
        dry_run: bool,
    ) -> Option<Self> {
        if !enabled {
            return None;
        }
        let stamp = unix_stamp();
        let dir = PathBuf::from("handoff/sessions").join(&stamp);
        if let Err(e) = fs::create_dir_all(&dir) {
            tracing::warn!(error = %e, path = %dir.display(), "handoff session dir");
            return None;
        }
        let meta = json!({
            "started_unix": stamp,
            "spectron_base_url": base_url,
            "spectron_context_id": context_id,
            "sidecar_listen": listen,
            "dry_run": dry_run,
            "note": "API key intentionally omitted. Pair with Surrealist screenshots / \
                     Playground paste in SESSION.md when assembling the handoff pack.",
        });
        let meta_path = dir.join("meta.json");
        if let Err(e) = fs::write(&meta_path, format!("{meta:#}\n")) {
            tracing::warn!(error = %e, "write handoff meta.json");
        }
        let path = dir.join("pipeline.jsonl");
        let file = match OpenOptions::new().create(true).append(true).open(&path) {
            Ok(f) => f,
            Err(e) => {
                tracing::warn!(error = %e, path = %path.display(), "open pipeline.jsonl");
                return None;
            }
        };
        tracing::info!(
            path = %dir.display(),
            "handoff session capture on (Exult -> sidecar -> Spectron)"
        );
        let log = Self {
            dir,
            file: Mutex::new(file),
        };
        log.write(json!({
            "t": unix_ms(),
            "kind": "session_start",
            "dir": log.dir.display().to_string(),
        }));
        // Convenience pointer for "the latest capture".
        let _ = fs::write(
            Path::new("handoff/sessions/LATEST"),
            format!("{}\n", log.dir.display()),
        );
        Some(log)
    }

    pub fn dir(&self) -> &Path {
        &self.dir
    }

    pub fn write(&self, value: Value) {
        if let Ok(mut f) = self.file.lock() {
            let _ = writeln!(f, "{value}");
            let _ = f.flush();
        }
    }

    pub fn event_in(&self, raw_body: &str, chronicle: &str, summary: &str) {
        let parsed: Value = serde_json::from_str(raw_body).unwrap_or(Value::String(raw_body.into()));
        self.write(json!({
            "t": unix_ms(),
            "kind": "exult_event",
            "summary": summary,
            "chronicle": chronicle,
            "payload": parsed,
        }));
    }

    pub fn facts_out(
        &self,
        text: &str,
        labels: &[String],
        infer: &str,
        memory_category: Option<&str>,
        ok: bool,
        error: Option<&str>,
    ) {
        self.write(json!({
            "t": unix_ms(),
            "kind": "spectron_facts",
            "ok": ok,
            "infer": infer,
            "memory_category": memory_category,
            "labels": labels,
            "text": text,
            "error": error,
        }));
    }

    pub fn chat_out(&self, prompt: &str, reply: Option<&str>, ok: bool, error: Option<&str>) {
        self.write(json!({
            "t": unix_ms(),
            "kind": "spectron_chat",
            "ok": ok,
            "prompt": prompt,
            "reply": reply,
            "error": error,
        }));
    }

    pub fn documents_out(
        &self,
        filename: &str,
        title: &str,
        body_len: usize,
        labels: &[String],
        ok: bool,
        response_or_error: &str,
    ) {
        self.write(json!({
            "t": unix_ms(),
            "kind": "spectron_documents",
            "ok": ok,
            "filename": filename,
            "title": title,
            "body_chars": body_len,
            "labels": labels,
            "response_or_error": response_or_error,
        }));
    }
}

fn unix_stamp() -> String {
    let secs = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0);
    // Local-ish sortable folder name without chrono dep.
    format!("{secs}")
}

fn unix_ms() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis())
        .unwrap_or(0)
}
