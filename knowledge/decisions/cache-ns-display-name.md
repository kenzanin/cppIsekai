---
type: Decision
title: "Cache namespace = plugin display name, not plugin id"
description: "img/<ns> is keyed by the slug of the plugin display name (id only as fallback) via an injected NsResolver; id-keyed trees are cut over once at boot, and the img pool lost its context-free sha fallback entirely."
tags: [cache, cache-layout, namespace, plugin-identity, cutover]
generated: { by: agent/cli, at: "2026-11-14T11:44:06Z" }
status: stable
---

- `cache::cache_ns(plugin_id, name)` (`src/cache/slug.hpp`) = `slug_base(name)`, id fallback when blank/unsluggable. Every `img/` path goes through `Cache::ns(plugin)`; the cache holds no Store/registry dependency (resolver injected from `store.plugins()` at boot in `src/main.cpp`).
- Motivation: the id is a folder detail -- re-installing a plugin under another id or folder threw away its whole cached library. The display name is what stays stable, and slugged it is fs-safe.
- Accepted costs: two plugins with the same display name share one directory; renaming the display name starts a fresh cache (like renaming the id did); no migration table exists.
- Cutover: first boot *with a resolver* renames `img/` to `.legacy-img` (swept next boot) and recreates it; `.cache-ns-name` keeps it to one boot. No resolver = id IS the namespace, tree untouched (keeps pre-seeded `img/` fixtures in tests/test_cache.cpp honest).
- Outside path builders `/api/cbz-ready` and `/export/cbz` moved to `Cache::ns(plugin)` -- previously they concatenated the raw id.
- Context-free sha fallback DELETED (design D5): `img_store`/`img_ensure` refuse without a placement path, `path_for` yields "" for the img pool, the worker drops a ctx-less nudge, and `/cache/img?url=` with no cover/page context is a 404 that fetches nothing. The `api/<sha>.json` pool keeps sha naming. Trigger for the cleanup: 10 loose `img/<sha1>.webp` page files written by the two ctx-less nudges (`http_api_state.cpp` skip rerun + `http_api_reader.cpp` unknown-chapter branch) -- unreachable by reader/CBZ/probe, dead weight for LRU only.
- Change: openspec/changes/cache-ns-display-name (delta cache-layout: namespace layout, no-context refusal, cutover, no migration). Tests: test_slug, test_cache, test_http_img, test_worker green.

# Related Concepts
- [cache-layout-v3: per-plugin layout, global transcode, host cover ratio](../architecture/cache-layout-v3.md): the namespace replaces the per-plugin directory component described there, and drops its context-free sha fallback
