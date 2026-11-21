---
type: Decision
title: "Plugin host promotion: shared host cores, thin runtime marshals"
description: "Host-tool promotion series (D0..D7): per-plugin copies of text/codec/crypto/url/html helpers move into shared host cores under src/plugin, with one thin marshal per runtime (lua/js/wasm) and site adaptation left in the plugin."
tags: [plugin, host-tools, shared-core, lua, js, wasm, parity, architecture]
generated: { by: agent/mcp, at: "2026-11-21T05:30:48Z" }
status: stable
---

# Plugin host promotion: shared host cores, thin runtime marshals

Decision series referenced by code comments as `plugin-host-promotion D0..D7`
(and by later changes such as `text-entity-fixpoint`). Recorded here because the
series had no concept entry; the decisions are already implemented in the tree.

## What was promoted
Repeated per-plugin helpers moved into host cores under `src/plugin/`:

- `host_text.hpp` — normalize_status, chapter_num, date_to_iso/date_epoch,
  json_blob, trim, unescape, strip_html, clean.
- `host_codecs.hpp` / `host_crypto.hpp` / `url_codec.hpp` / `host_xpath.hpp` —
  codecs, crypto, URL codec, XPath.
- `htmltext.hpp` (`htmltext_re2.cpp`, `htmltext_html.cpp`) — RE2 match/find/
  replace/quote/capture and lexbor CSS select/attr.

## Contract
One C++ core per domain; lua/js/wasm each keep a thin marshal that only
marshals arguments and results. Runtime surfaces stay at parity (same helper
name and semantics), and site-specific adaptation stays in the plugin — the
host owns site-agnostic transforms only.

## D0 (typo/synonym tolerance)
Real upstream data carries typos ("ongoin", "onhaiatus") and variant vocabulary
("publish", "finish", "onhold", "notyetpublished"); the host map is the union of
every plugin copy's synonyms.

## Follow-ups
A fleet-wide migration of plugin copies to the host helpers is incremental; each
promotion lands with tests in the host suite and migrates at least one proving
plugin.

# Related Concepts
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): Host promotion is the mechanism behind the plugin system's shared host-tool surface: cores live in src/plugin, runtimes keep thin marshals.
