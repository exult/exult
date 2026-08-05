use crate::config::Config;
use crate::session_log::SessionLog;
use anyhow::{bail, Context, Result};
use reqwest::multipart::{Form, Part};
use reqwest::Client;
use serde::Deserialize;
use serde_json::json;
use std::sync::Arc;
use std::time::Duration;

#[derive(Clone)]
pub struct SpectronClient {
    http: Client,
    base_url: String,
    context_id: String,
    api_key: String,
    dry_run: bool,
    session: Option<Arc<SessionLog>>,
}

#[derive(Debug, Clone, Default)]
pub struct RememberOpts {
    pub labels: Vec<String>,
    pub memory_category: Option<&'static str>,
    /// Spectron `infer` mode: `full` (default), `none`, `preview`, …
    pub infer: Option<&'static str>,
}

#[derive(Debug, Deserialize)]
struct ChatResponse {
    reply: String,
}

impl SpectronClient {
    pub fn new(cfg: &Config, session: Option<Arc<SessionLog>>) -> Self {
        let http = Client::builder()
            // Prior-memory docs + infer=full need headroom; staging gateways
            // also sit behind a slow extraction path.
            .timeout(Duration::from_secs(180))
            .connect_timeout(Duration::from_secs(20))
            .build()
            .unwrap_or_else(|_| Client::new());
        Self {
            http,
            base_url: cfg.spectron_base_url.clone(),
            context_id: cfg.spectron_context_id.clone(),
            api_key: cfg.spectron_api_key.clone(),
            dry_run: cfg.dry_run,
            session,
        }
    }

    pub async fn chat(&self, message: &str) -> Result<String> {
        if self.dry_run {
            let reply = format!(
                "[dry-run] Spectron would answer:\n\n{}",
                truncate(message, 400)
            );
            if let Some(s) = &self.session {
                s.chat_out(message, Some(&reply), true, None);
            }
            return Ok(reply);
        }

        let url = format!("{}/api/v1/{}/chat", self.base_url, self.context_id);
        let body = json!({
            "message": message,
            "stream": false,
        });

        let response = self
            .http
            .post(&url)
            .header("Authorization", format!("Bearer {}", self.api_key))
            .header("api-version", "1")
            .json(&body)
            .send()
            .await
            .with_context(|| format!("POST {url}"))?;

        let status = response.status();
        let headers: Vec<(String, String)> = response
            .headers()
            .iter()
            .filter_map(|(k, v)| {
                let name = k.as_str();
                // Skip auth-like headers if any ever appear on the response.
                if name.eq_ignore_ascii_case("set-cookie") {
                    return None;
                }
                v.to_str().ok().map(|v| (name.to_string(), v.to_string()))
            })
            .collect();
        let text = response.text().await.unwrap_or_default();
        if !status.is_success() {
            if let Some(s) = &self.session {
                s.chat_out(message, None, false, Some(&text));
                s.write(serde_json::json!({
                    "t": std::time::SystemTime::now()
                        .duration_since(std::time::UNIX_EPOCH)
                        .map(|d| d.as_millis())
                        .unwrap_or(0),
                    "kind": "spectron_http_error",
                    "route": "chat",
                    "status": status.as_u16(),
                    "body": text,
                    "headers": headers,
                }));
            }
            bail!("Spectron chat failed ({status}): {text}");
        }

        let parsed: ChatResponse =
            serde_json::from_str(&text).with_context(|| format!("chat JSON: {text}"))?;
        let reply = strip_citation_markers(&parsed.reply);
        if let Some(s) = &self.session {
            s.chat_out(message, Some(&reply), true, None);
        }
        Ok(reply)
    }

    pub async fn remember_text_with(&self, text: &str, opts: RememberOpts) -> Result<()> {
        let infer = opts.infer.unwrap_or("full");
        if self.dry_run {
            tracing::info!(
                "[dry-run] would POST /facts ({} chars, category={:?}, labels={:?}, infer={infer})",
                text.len(),
                opts.memory_category,
                opts.labels,
            );
            if let Some(s) = &self.session {
                s.facts_out(text, &opts.labels, infer, opts.memory_category, true, None);
            }
            return Ok(());
        }

        let url = format!("{}/api/v1/{}/facts", self.base_url, self.context_id);
        let mut body = json!({
            "text": text,
            "infer": infer,
            "labels": opts.labels,
        });
        if let Some(cat) = opts.memory_category {
            body["memory_category"] = json!(cat);
        }

        let response = self
            .http
            .post(&url)
            .header("Authorization", format!("Bearer {}", self.api_key))
            .header("api-version", "1")
            .json(&body)
            .send()
            .await
            .with_context(|| format!("POST {url}"))?;

        let status = response.status();
        if !status.is_success() {
            let err = response.text().await.unwrap_or_default();
            if let Some(s) = &self.session {
                s.facts_out(
                    text,
                    &opts.labels,
                    infer,
                    opts.memory_category,
                    false,
                    Some(&err),
                );
            }
            bail!("Spectron facts failed ({status}): {err}");
        }
        if let Some(s) = &self.session {
            s.facts_out(text, &opts.labels, infer, opts.memory_category, true, None);
        }
        Ok(())
    }

    /// Upload a markdown document (preferred for prior-memory notes). Extraction
    /// runs asynchronously in Spectron's document pipeline.
    pub async fn upload_markdown_document(
        &self,
        filename: &str,
        title: &str,
        body: &str,
        labels: &[String],
    ) -> Result<String> {
        if self.dry_run {
            tracing::info!(
                "[dry-run] would POST /documents ({filename}, {} chars, labels={labels:?})",
                body.len()
            );
            if let Some(s) = &self.session {
                s.documents_out(filename, title, body.len(), labels, true, "dry-run");
            }
            return Ok("dry-run-doc".into());
        }

        let url = format!("{}/api/v1/{}/documents", self.base_url, self.context_id);
        let metadata = json!({
            "title": title,
            "source": "exult-prior-memory",
            "mimeType": "text/markdown",
            "labels": labels,
        });
        let file = Part::bytes(body.as_bytes().to_vec())
            .file_name(filename.to_string())
            .mime_str("text/markdown")
            .context("mime for markdown part")?;
        let form = Form::new()
            .text("metadata", metadata.to_string())
            .part("file", file);

        let response = self
            .http
            .post(&url)
            .header("Authorization", format!("Bearer {}", self.api_key))
            .header("api-version", "1")
            .multipart(form)
            .send()
            .await
            .with_context(|| format!("POST {url}"))?;

        let status = response.status();
        let text = response.text().await.unwrap_or_default();
        if !status.is_success() {
            if let Some(s) = &self.session {
                s.documents_out(filename, title, body.len(), labels, false, &text);
            }
            bail!("Spectron documents failed ({status}): {text}");
        }
        if let Some(s) = &self.session {
            s.documents_out(filename, title, body.len(), labels, true, &text);
        }
        Ok(text)
    }
}

fn truncate(s: &str, max: usize) -> String {
    if s.chars().count() <= max {
        s.to_string()
    } else {
        let cut: String = s.chars().take(max).collect();
        format!("{cut}...")
    }
}

/// Spectron grounds `/chat` replies with markers like `[S1]` / bare `S1` that
/// index retrieved sources. Fine for tools; wrong for Avatar-facing thoughts.
fn strip_citation_markers(s: &str) -> String {
    let chars: Vec<char> = s.chars().collect();
    let mut out = String::with_capacity(s.len());
    let mut i = 0;
    while i < chars.len() {
        // Bracketed form: [S12]
        if chars[i] == '[' && i + 2 < chars.len() && chars[i + 1] == 'S' {
            let mut j = i + 2;
            while j < chars.len() && chars[j].is_ascii_digit() {
                j += 1;
            }
            if j > i + 2 && j < chars.len() && chars[j] == ']' {
                trim_trailing_sep(&mut out);
                i = j + 1;
                skip_comma_space(&chars, &mut i);
                ensure_word_break(&mut out, &chars, i);
                continue;
            }
        }
        // Bare form: "… quests S1, S3." (not part of a longer token)
        if chars[i] == 'S'
            && i + 1 < chars.len()
            && chars[i + 1].is_ascii_digit()
            && (i == 0 || !chars[i - 1].is_ascii_alphanumeric())
        {
            let mut j = i + 1;
            while j < chars.len() && chars[j].is_ascii_digit() {
                j += 1;
            }
            if j > i + 1 && (j >= chars.len() || !chars[j].is_ascii_alphanumeric()) {
                trim_trailing_sep(&mut out);
                i = j;
                skip_comma_space(&chars, &mut i);
                ensure_word_break(&mut out, &chars, i);
                continue;
            }
        }
        out.push(chars[i]);
        i += 1;
    }
    tidy_spaces_and_punct(&out)
}

fn trim_trailing_sep(out: &mut String) {
    while out.ends_with(' ') || out.ends_with(',') {
        out.pop();
    }
}

fn skip_comma_space(chars: &[char], i: &mut usize) {
    while *i < chars.len() && (chars[*i] == ',' || chars[*i] == ' ') {
        *i += 1;
    }
}

fn ensure_word_break(out: &mut String, chars: &[char], i: usize) {
    if i >= chars.len() {
        return;
    }
    let next = chars[i];
    if next.is_ascii_alphanumeric() && !out.is_empty() && !out.ends_with(' ') {
        out.push(' ');
    }
}

fn tidy_spaces_and_punct(s: &str) -> String {
    let mut cleaned = String::with_capacity(s.len());
    let mut prev_space = false;
    for ch in s.chars() {
        if ch == ' ' {
            if prev_space {
                continue;
            }
            prev_space = true;
            cleaned.push(ch);
            continue;
        }
        prev_space = false;
        if matches!(ch, ',' | '.' | ';' | '!' | '?') {
            while cleaned.ends_with(' ') {
                cleaned.pop();
            }
        }
        cleaned.push(ch);
    }
    cleaned.trim().to_string()
}

#[cfg(test)]
mod tests {
    use super::strip_citation_markers;

    #[test]
    fn strips_spectron_source_markers() {
        assert_eq!(
            strip_citation_markers("Clear skies yield to rain [S3]."),
            "Clear skies yield to rain."
        );
        assert_eq!(
            strip_citation_markers("Iolo [S1] and Dupre [S12] wait."),
            "Iolo and Dupre wait."
        );
        assert_eq!(
            strip_citation_markers("Keep [Sword] as text."),
            "Keep [Sword] as text."
        );
        assert_eq!(
            strip_citation_markers(
                "participated in various quests S1, S3, S5, S6, S7."
            ),
            "participated in various quests."
        );
    }
}
