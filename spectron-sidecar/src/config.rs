use anyhow::{bail, Context, Result};
use std::env;
use std::net::SocketAddr;

/// Runtime config for the sidecar. All values come from environment variables
/// so Exult never needs to know Spectron credentials.
#[derive(Debug, Clone)]
pub struct Config {
    pub listen: SocketAddr,
    pub spectron_base_url: String,
    pub spectron_context_id: String,
    pub spectron_api_key: String,
    /// When true, skip real HTTP to Spectron and invent replies (useful offline).
    pub dry_run: bool,
    /// Write Exult→sidecar→Spectron JSONL under `handoff/sessions/` for engineers.
    pub handoff: bool,
}

impl Config {
    pub fn from_env() -> Result<Self> {
        let listen: SocketAddr = env::var("SPECTRON_SIDECAR_LISTEN")
            .unwrap_or_else(|_| "127.0.0.1:8765".into())
            .parse()
            .context("SPECTRON_SIDECAR_LISTEN must be host:port")?;

        let dry_run = env_flag("SPECTRON_SIDECAR_DRY_RUN");
        // Opt-in local JSONL capture under handoff/sessions/.
        let handoff = env_flag("SPECTRON_SIDECAR_HANDOFF");
        let spectron_base_url = env::var("SPECTRON_BASE_URL")
            .unwrap_or_else(|_| "http://127.0.0.1:9090".into())
            .trim()
            .trim_end_matches('/')
            .to_string();
        let spectron_context_id = env::var("SPECTRON_CONTEXT_ID")
            .unwrap_or_else(|_| "exult".into())
            .trim()
            .to_string();
        let spectron_api_key = env::var("SPECTRON_API_KEY")
            .unwrap_or_default()
            .trim()
            .trim_matches(|c| c == '"' || c == '\'')
            .to_string();

        if !dry_run && spectron_api_key.is_empty() {
            bail!(
                "SPECTRON_API_KEY is required unless SPECTRON_SIDECAR_DRY_RUN=1. \
                 Export a Spectron Context API key, or run dry-run for the egui demo."
            );
        }

        Ok(Self {
            listen,
            spectron_base_url,
            spectron_context_id,
            spectron_api_key,
            dry_run,
            handoff,
        })
    }
}

fn env_flag(name: &str) -> bool {
    matches!(
        env::var(name).ok().as_deref(),
        Some("1" | "true" | "TRUE" | "yes" | "YES")
    )
}
