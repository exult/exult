use std::sync::Arc;

use axum::body::Bytes;
use axum::extract::State;
use axum::http::StatusCode;
use axum::routing::{get, post};
use axum::Router;
use tokio::sync::mpsc;

use crate::events::GameEvent;
use crate::session_log::SessionLog;

#[derive(Clone)]
pub struct ServerState {
    pub events: mpsc::UnboundedSender<GameEvent>,
    pub session: Option<Arc<SessionLog>>,
}

pub fn router(state: ServerState) -> Router {
    Router::new()
        .route("/health", get(|| async { "ok" }))
        .route("/event", post(post_event))
        .with_state(Arc::new(state))
}

async fn post_event(State(state): State<Arc<ServerState>>, body: Bytes) -> StatusCode {
    let raw = String::from_utf8_lossy(&body);
    let event: GameEvent = match serde_json::from_slice(&body) {
        Ok(e) => e,
        Err(e) => {
            let preview: String = raw.chars().take(500).collect();
            tracing::warn!("bad /event JSON ({e}): {preview}");
            return StatusCode::BAD_REQUEST;
        }
    };
    tracing::info!("event: {}", event.summary_label());
    // Mirror every accepted POST so we can tell "Exult sent it" from "UI showed it".
    if let Ok(mut out) = std::fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open("/tmp/spectron_sidecar_accepted.log")
    {
        use std::io::Write;
        let _ = writeln!(out, "{}", event.chronicle_json());
    }
    if let Some(session) = &state.session {
        session.event_in(&raw, &event.chronicle_display(), &event.summary_label());
    }
    if state.events.send(event).is_err() {
        return StatusCode::SERVICE_UNAVAILABLE;
    }
    StatusCode::ACCEPTED
}
