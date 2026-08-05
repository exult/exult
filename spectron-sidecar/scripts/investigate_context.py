#!/usr/bin/env python3
"""Probe a Spectron Context via API. Reads SPECTRON_* env vars. No secrets in output."""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request
from collections import Counter
from typing import Any

BASE = os.environ.get("SPECTRON_BASE_URL", "").rstrip("/")
CTX = os.environ.get("SPECTRON_CONTEXT_ID", "")
KEY = os.environ.get("SPECTRON_API_KEY", "")

EVAL_PROMPTS = [
    "What is the password to leave Trinsic? Who said it?",
    "Who is Christopher's son, and how do you know?",
    "Who works at the Honorable Hound?",
    "Are you the Avatar?",
    'Has anyone ever said "What is your liberty worth?" to me?',
    "Has a guard accosted me recently?",
]

QUERIES = [
    ("liberty (unfiltered)", {"query": "What is your liberty worth?", "k": 10}),
    (
        "guard bribe (conversation label)",
        {
            "query": "guard liberty gold bribe unpleasant trial",
            "k": 12,
            "labels": ["kind=conversation"],
        },
    ),
    (
        "Blackbird password (conversation label)",
        {
            "query": "password Blackbird leave Trinsic Finnigan",
            "k": 10,
            "labels": ["kind=conversation"],
        },
    ),
    (
        "npc_id 258 guard",
        {"query": "npc_id=258 guard liberty worth", "k": 10},
    ),
    (
        "Spark Christopher (conversation label)",
        {
            "query": "Christopher son Spark Finnigan",
            "k": 10,
            "labels": ["kind=conversation"],
        },
    ),
]

REFLECT_PROMPTS = [
    "What conversations have I had with guards in Trinsic, including bribes, liberty, and combat?",
    "Who is Spark and how do I know? Who works at the Honorable Hound?",
]


def api(method: str, path: str, body: dict | None = None) -> tuple[int, Any]:
    url = f"{BASE}/api/v1/{CTX}{path}"
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(
        url,
        data=data,
        method=method,
        headers={
            "Authorization": f"Bearer {KEY}",
            "api-version": "1",
            "Content-Type": "application/json",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=180) as resp:
            raw = resp.read().decode()
            return resp.status, json.loads(raw) if raw else None
    except urllib.error.HTTPError as e:
        raw = e.read().decode()
        try:
            payload = json.loads(raw)
        except json.JSONDecodeError:
            payload = raw
        return e.code, payload


def hit_line(h: dict) -> str:
    text = (h.get("text") or "")[:200].replace("\n", " ")
    score = h.get("score")
    src = h.get("source", "")
    return f"score={score} [{src}] {text}"


def context_stats(ctx: str) -> dict[str, Any]:
    lines = ctx.splitlines()
    sections: Counter[str] = Counter()
    current = "(preamble)"
    for line in lines:
        if line.startswith("## "):
            current = line[3:].strip()
        sections[current] += 1
    attrs_you = 0
    in_you = False
    for line in lines:
        if line.startswith("## You"):
            in_you = True
            continue
        if line.startswith("## ") and in_you:
            break
        if in_you and line.startswith("- "):
            attrs_you += 1
    return {
        "chars": len(ctx),
        "lines": len(lines),
        "top_sections": sections.most_common(8),
        "you_section_attrs": attrs_you,
    }


def main() -> int:
    if not (BASE and CTX and KEY):
        print("Set SPECTRON_BASE_URL, SPECTRON_CONTEXT_ID, SPECTRON_API_KEY", file=sys.stderr)
        return 1

    out: dict[str, Any] = {"context_id": CTX, "base_url": BASE}

    req = urllib.request.Request(
        f"{BASE}/api/v1/health",
        headers={"Authorization": f"Bearer {KEY}", "api-version": "1"},
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            raw = resp.read().decode()
            out["health"] = json.loads(raw) if raw.strip() else {"status": resp.status}
    except urllib.error.HTTPError as e:
        out["health"] = {"error": e.code, "body": e.read().decode()[:200]}

    status, entities = api("GET", "/entities?limit=500")
    out["entities_status"] = status
    ents = entities.get("entities", []) if isinstance(entities, dict) else []
    out["entity_count"] = len(ents)
    by_type = Counter(e.get("entityType", "?") for e in ents)
    out["entity_types"] = dict(by_type.most_common(25))

    keywords = (
        "guard",
        "liberty",
        "blackbird",
        "spark",
        "hound",
        "trinsic",
        "finnigan",
        "combat",
        "cheese",
        "bribe",
    )
    matched = []
    for e in ents:
        blob = json.dumps(e).lower()
        if any(k in blob for k in keywords):
            matched.append(f"{e.get('entityType')}/{e.get('name')}")
    out["entities_matching_keywords"] = sorted(set(matched))[:40]

    # Try fetching a few entities by type/name
    entity_fetches = []
    for etype, name in [
        ("person", "guard"),
        ("concept", "Blackbird"),
    ]:
        st, data = api("GET", f"/entities/{etype}/{name}")
        entity_fetches.append(
            {
                "path": f"{etype}/{name}",
                "status": st,
                "keys": list(data.keys()) if isinstance(data, dict) else str(data)[:200],
                "attribute_count": len(data.get("attributes", []))
                if isinstance(data, dict)
                else 0,
                "relation_count": len(data.get("relations", []))
                if isinstance(data, dict)
                else 0,
            }
        )
    out["entity_fetches"] = entity_fetches

    out["queries"] = {}
    for label, body in QUERIES:
        st, data = api("POST", "/query", body)
        hits = data.get("hits", []) if isinstance(data, dict) else []
        out["queries"][label] = {
            "status": st,
            "tier": data.get("tier") if isinstance(data, dict) else None,
            "query_ms": data.get("queryMs") if isinstance(data, dict) else None,
            "hit_count": len(hits),
            "top_hits": [hit_line(h) for h in hits[:8]],
        }

    out["context_samples"] = {}
    for q in [
        "password Blackbird Trinsic",
        "guard liberty gold bribe",
    ]:
        st, data = api("POST", "/context", {"query": q, "k": 12})
        ctx = data.get("context", "") if isinstance(data, dict) else ""
        out["context_samples"][q] = {
            "status": st,
            "tier": data.get("tier") if isinstance(data, dict) else None,
            "stats": context_stats(ctx),
            "preview": ctx[:2500],
        }

    out["reflect"] = {}
    for prompt in REFLECT_PROMPTS:
        st, data = api("POST", "/reflect", {"query": prompt, "persist": False})
        evidence = data.get("evidence", []) if isinstance(data, dict) else []
        ev_texts = [
            (e if isinstance(e, str) else e.get("text", str(e)))[:180] for e in evidence[:6]
        ]
        out["reflect"][prompt[:60]] = {
            "status": st,
            "trace_id": data.get("traceId") if isinstance(data, dict) else None,
            "reflection": (data.get("reflection") or "")[:800]
            if isinstance(data, dict)
            else "",
            "evidence_count": len(evidence),
            "evidence_sample": ev_texts,
        }

    out["chat_eval"] = {}
    for prompt in EVAL_PROMPTS:
        st, data = api("POST", "/chat", {"message": prompt, "stream": False})
        reply = data.get("reply", "") if isinstance(data, dict) else str(data)
        out["chat_eval"][prompt[:50]] = {
            "status": st,
            "reply": reply[:600],
        }

    json.dump(out, sys.stdout, indent=2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
