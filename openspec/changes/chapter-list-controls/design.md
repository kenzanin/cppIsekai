# Design

## Goals / Non-Goals
**Goals:** skip and read toggles persist across restarts; completion derives from real reading progress, not manual bookkeeping only; the list paginates with zero server API growth beyond two thin routes.
**Non-Goals:** server-side pagination/windowing (the chapters payload is already whole in the client); reader UI changes; per-manga read batching or analytics.

## Decisions
- **D1 Client-side pagination.** The page already holds the full chapter array (`/api/chapters`); slice it in Alpine (page size 20, prev/next + `n/m`). Why: no API change, instant, list shrinks with the same data on refresh.
- **D2 Read progress lives in plugin_state as `host/read/<chapter_id>` = highest page reached.** Mirrors the reserved `host/skip/` namespace: per-chapter key, survives restart, no schema change. Completion = stored value >= the chapter's `pages`, compared client-side where the total lives.
- **D3 Completion auto-derives from the existing position POST.** `/reader/position` already fires on every view change (debounced); its handler bumps `host/read/<ch>` to `max(stored, page)`. No new reader traffic, no second source of truth.
- **D4 One batched state fetch.** `POST /chapter/state {ids:[chapterId...]}` -> `{<id>: {skip, read}}` paints eye, check and counter in a single round trip; the page owns the id list already. Absent keys mean "not skipped" / `0`. Writes: skip reuses `POST /chapter/<id>/skip`; read gets `POST /chapter/<id>/read {page}` (`0` clears) with optimistic UI.

## Risks
- Non-numeric stored values parse as 0 (defensive `atoi`-style read) — stale/garbage keys degrade to "unread", never throw.
- `host/` key collision: `read/` vs `skip/` prefixes are distinct; chapter ids are the same namespace the skip route already uses.
