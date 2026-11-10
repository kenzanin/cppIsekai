# Design

## Context

See proposal.md for motivation. Current state:

- `reader.mustache` renders inside `top.mustache` (app header) + `<main class="p-4">` via the `/read/<plugin>/<slug>` route (`src/http/http_pages.cpp`), with a permanent toolbar (back/mode/dir/zoom/ch-page hint), fixed tap-zone overlays, and `label()` shown in the toolbar.
- Reader JS is one Alpine component (`readerPage`): `pages`/`views`/`cur()`/`step()`/`keys()`/`persist()`; zoom and mode already work; tap zones are `left-0 w-1/3` and `right-0 w-1/3` fixed overlays.
- No store/API changes needed — nav visibility is client state; page/chapter lists derive from existing data (`pages`, `chapters` fetched for chId resolution, `prev_ch`/`next_ch`).

## Goals / Non-Goals

**Goals:**
- Reader owns its full viewport; app chrome disappears on `/read`
- Center-third tap toggles one `navShown` state controlling both bars (all modes)
- Numbered page buttons up to ~24; `select` in top bar for chapter and (beyond fit) page jump

**Non-Goals:**
- New gestures (swipe), gamepad support, fullscreen API (`requestFullscreen`) — browser fullscreen stays out; this is viewport layout, not Fullscreen API
- Nav auto-hide timers — explicitly rejected by user
- Any store/API schema change; bottom-bar persistence of nav state
- Persisting zoom (unchanged, client-only as today)

## Decisions

- **D1 — Reader renders its own shell, not the app shell.** The `/read` route stops including `top.mustache`/`bottom.mustache` and `main` padding; `reader.mustache` provides a self-contained `<doctype>` page with Tailwind/Alpine scripts (shared statics) and the image layer. Why: full-screen must not depend on hide-CSS against app markup; route ownership stays in `http_pages.cpp` with one template swap.
- **D2 — One `navShown` Alpine flag; center tap toggles it.** Left/right tap zones keep `step()` and stay active in both nav states; their zones stop at the center third as today, so a center tap never pages accidentally. When shown, bars sit above the image (`z-30/40`); tapping center toggles back off.
- **D3 — Bars overlay, not reflow.** `position: fixed` top/bottom overlays with semi-opaque zinc backgrounds (consistent with current toolbar); the image stays full-viewport regardless of nav state — no resize on toggle, no scroll jump.
- **D4 — Page select lists `1..N` (display numbers), chapter select lists all chapters from the existing `/api/chapters` fetch** (only names/numbers needed; the fetch already exists for chId resolution). Selecting a chapter uses the existing `chUrl(n)` navigation; selecting a page sets `page=N` via `viewOf`/`firstOf` mapping (same math the numbered buttons use) and calls `persist()` — reusing `step`'s internals through a `goto_page(n)` helper.
- **D5 — Numbered buttons threshold is a constant (≈24).** Render `1..N` buttons while N ≤ 24; else render one big `current / total` jump control (clicking it focuses the top bar page select). Why: fixed-breakpoint keeps CSS trivial; fine-tuning can happen with real long chapters.
- **D6 — Small dim indicator when hidden** (`p N / M`), non-interactive (`pointer-events-none`), bottom-center, opacity ~60%, same position as bottom overlay — no layout shift on toggle.
- **D7 — Keyboard unchanged**: arrow-key paging already exists; `keys()` keeps its select/input guard. Add `Home`/`End` only if trivial? No — keep scope: arrows only, nav visibility does not change their behavior.
- **D8 — Chapter-boundary crossing reuses the view math.** `step()` at a boundary (index below 0 or past the end) navigates via `chUrl(prev_ch|next_ch|pages)` with mode/direction/zoom carried on the URL as query params (`?ch=&page=&mode=&dir=&zoom=` — the resume seed already parses `mode`/`dir`; add `zoom`) in addition to persisting them, so the landing page never flashes the wrong chrome before JS boots. Prev-boundary passes `page=<prev_chapter.pages>` when the chapters response carries a known nonzero `pages` for that chapter (MangaDex `/feed` returns `attributes.pages` today, so this is the normal path), else page 1.
- **D9 — Done overlay is pure client state.** Condition: `mode !== 'webtoon'`? No — all modes; trigger: `step(+1)` (and the Next key/tap) while on the last view and `next_ch` is falsy. Shows a fixed full-screen zinc overlay with `Done 🎉` and a `<a>` back to `/view/manga/<plugin>/<slug>`. No store/API change.
- **D10 — Zoom persists in `reader_positions`.** Additive `zoom` TEXT column (empty = unset) on `reader_positions`; `ReaderPosition::zoom`; `position_set`/`position_get`/`position_last` include it; route's `merge_position` mirrors the `mode`/`direction` absent-means-keep-stored rule for `zoom`; `/api/position` GET returns it; the reader seeds from `{{resume_zoom}}` (template context + query `zoom=`), `save()` posts `zoom`, and `modeChanged`-style persistence fires on zoom change (debounced like page changes). Migration is automatic: fresh column added to the CREATE TABLE for new DBs; existing `data/library.db` needs the column — handled at boot with `ALTER TABLE ... ADD COLUMN zoom TEXT DEFAULT ''` guarded by a `PRAGMA table_info` check (sqlite_orm does not do this automatically).
- **D11 — Wire format compatibility:** `zoom` absent on POST → kept (D10); `zoom` present → overwrite. GET `/api/position/<plugin>/<slug>` includes `zoom` (ignored by old clients).

## Risks / Trade-offs

- [Tailwind CDN + bespoke shell duplication of top.mustache] → reader mustache gains its own head; duplication accepted for 2 routes, refactor into `reader_shell.mustache` partial only if a second full-screen page appears
- [`fixed` overlays on mobile + iOS Safari viewport] → same pattern already used by top overlay; visual smoke on blueprint covers desktop; mobile untested
- [Center tap conflicts with spread-v scroll on touch] → same trade-off as today's tap zones; user accepted in discussion

## Migration Plan

Additive to one route and one template. Rollback = revert commit. No schema/config/cache/plugin changes.
