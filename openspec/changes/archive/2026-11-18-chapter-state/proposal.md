# Proposal: chapter-state

## Why

Chapter-level state (read / skip) lives as `host/skip/<id>` / `host/read/<id>` string rows in
`plugin_state`. The key carries no manga identity, the state is stringly-typed, there is no bulk
operation, and chapter identities themselves (number/title/source URL) are re-parsed from cached
JSON envelopes on every render. The user wants bulk read management on the detail page (mark all
read, clear all, mark a range such as 1..N) and a durable per-chapter row to hang state on.

## What changes

- **New store tables**
  - `chapters(plugin, slug, chapter_id)` PK — identity row per chapter: `number`, `title`, `url`,
    `flags` bitfield, `updated_at`. Upserted on every successful chapters parse.
  - `chapter_pages(chapter_id, idx)` — source image URL per page, replaced on every pages parse.
- **Flags bitfield on `chapters.flags`** (house pattern: `saved_manga.status_flags`):
  `bit0 = read`, `bit1 = hidden`, `bit2+` reserved.
- **Bulk flags API** `POST /chapter/flags` — set/clear bits for `{ids[]}` or `all`, one transaction.
- **Detail page controls** — mark-all-read / clear-all in the list header; per-row "mark previous
  read" with confirm; check glyph lit by the `read` bit.
- **Reader completion auto-read** — reaching the last page sets `read` bit server-side.
- **Skip/read consumers switch** to the bitfield (`/chapter/state`, reader window skip filter,
  `saved_manga.read_ch` rollup); legacy `host/*` keys get a one-shot best-effort backfill
  (unique chapter_id match only) and are then dormant.

## Impact

- Affected specs: `chapter-store` (new), `chapter-list-controls` (modified)
- Source: `src/store/*` (entities + 4 methods), `src/http/http_api_state.cpp`,
  `src/http/http_api_reader.cpp`, `src/http/api_ctx.hpp`, `src/http/http_api_content.cpp`,
  `templates/detail_script.mustache` / `detail.mustache`
- Non-goals: no image/blob paths in DB (filesystem stays source of truth for cache files); no
  hidden-row UI (bit reserved, shipped dark); `api_cache` envelope layer unchanged.
