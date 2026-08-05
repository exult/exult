mod app;
mod books;
mod config;
mod copy_protect;
mod events;
mod npc_roster;
mod prior_memory;
mod prompts;
mod runes;
mod server;
mod session_log;
mod sextant_feel;
mod spectron;
mod talk_slots;
mod theme;

use std::sync::Arc;

use anyhow::{Context, Result};
use tokio::sync::mpsc;
use tracing_subscriber::EnvFilter;

use crate::app::SidecarApp;
use crate::config::Config;
use crate::server::ServerState;
use crate::session_log::SessionLog;
use crate::spectron::SpectronClient;

fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::from_default_env().add_directive("info".parse()?))
        .init();

    let cfg = Config::from_env()?;
    tracing::info!(
        listen = %cfg.listen,
        spectron = %cfg.spectron_base_url,
        context = %cfg.spectron_context_id,
        dry_run = cfg.dry_run,
        handoff = cfg.handoff,
        "starting spectron-sidecar"
    );

    let session = SessionLog::maybe_start(
        cfg.handoff,
        &cfg.spectron_base_url,
        &cfg.spectron_context_id,
        &cfg.listen.to_string(),
        cfg.dry_run,
    )
    .map(Arc::new);

    // Keep the runtime alive for the whole process (eframe owns the UI thread).
    let runtime = Box::leak(Box::new(
        tokio::runtime::Builder::new_multi_thread()
            .enable_all()
            .build()
            .context("tokio runtime")?,
    ));
    let handle = runtime.handle().clone();

    let (events_tx, events_rx) = mpsc::unbounded_channel();
    let listen = cfg.listen;
    let server_tx = events_tx.clone();
    let server_session = session.clone();
    handle.spawn(async move {
        let app = server::router(ServerState {
            events: server_tx,
            session: server_session,
        });
        let listener = match tokio::net::TcpListener::bind(listen).await {
            Ok(l) => l,
            Err(e) => {
                tracing::error!("bind {listen}: {e}");
                return;
            }
        };
        tracing::info!("event server on http://{listen}");
        if let Err(e) = axum::serve(listener, app).await {
            tracing::error!("event server stopped: {e}");
        }
    });

    let client = SpectronClient::new(&cfg, session);
    let native_options = eframe::NativeOptions {
        viewport: eframe::egui::ViewportBuilder::default()
            .with_inner_size([1280.0, 720.0])
            .with_title("Exult Spectron sidecar"),
        ..Default::default()
    };

    let listen_addr = cfg.listen.to_string();
    eframe::run_native(
        "Exult Spectron sidecar",
        native_options,
        Box::new(move |cc| {
            Ok(Box::new(SidecarApp::new(
                cc,
                events_rx,
                events_tx,
                client,
                handle,
                listen_addr,
            )))
        }),
    )
    .map_err(|e| anyhow::anyhow!("eframe: {e}"))?;

    Ok(())
}
