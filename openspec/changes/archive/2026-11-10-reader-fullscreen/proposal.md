# Proposal

## Why

The current reader is a regular page inside the app chrome: the app's Library/Search header stays visible, the reader keeps a permanent toolbar, and the page number only exists while chrome is shown. Reading manga wants the image to own the screen, with any navigation chrome appearing on demand — like a real reader, not a form.

## What Changes

- The reader route (`/read/<plugin>/<slug>`) becomes a **full-screen view**: the app's `top.mustache` header (`Library | Search | ...`) and `<main class="p-4">` padding are hidden; the image fills the viewport.
- **Tap-zone model (all modes, zone widths as today):**
  - left/right thirds: previous/next page (RTL-aware), exactly as today
  - center third: toggle top+bottom overlay nav (no auto-hide; stays until center-clicked again)
- **Overlay nav (shown only when toggled, all modes including webtoon):**
  - Top bar: back icon/link to `/view/manga/<plugin>/<slug>`; chapter `<select>` (all chapters); page `<select>` (all pages)
  - Bottom bar: mode/direction/zoom selectors; page-jump row with numbered page buttons
- **Page-jump row:** numbered buttons 1..N in the bottom bar while they fit (compact, roughly up to two dozen); current page highlighted bright; beyond that the complete page list lives in the top bar's `page <select>` and the bottom bar falls back to a single big current-page/jump control.
- **When nav is hidden:** a small, dim `p N / M` indicator stays visible at the bottom; back/detail navigation is not available until nav is toggled.
- **Cross-chapter navigation:** tapping "next" on the last page of a chapter advances to the next chapter's first page; tapping "prev" on the first page advances to the previous chapter's last page (RTL-aware, symmetric with in-chapter taps). The chapter list envelope gains `pages` per chapter (MangaDex `/feed` already returns `attributes.pages`; Lua plugin passes it through) so the prev-boundary landing page is known without loading the chapter.
- **Done state:** at the end of the final chapter (no `next_ch`), tapping "next" shows a full-screen "Done 🎉" overlay with a back-to-detail button instead of failing silently.
- **Mode + direction + zoom persistence:** `zoom` joins `mode`/`direction` in the `reader_positions` store, so reading chrome state (mode, direction, zoom) survives restarts; navigation between chapters retains it too.
- Reader keeps position persistence, resume, prefetch and keyboard navigation.

## Capabilities

### New Capabilities

- `reader-fullscreen`: full-screen reader chrome — tap-zone toggle, overlay top/bottom nav, numbered/jump page navigation

### Modified Capabilities

<!-- reader-page (archived, openspec/specs/reader-page/spec.md) keeps its requirements for modes/resume/pages API; this change redefines the reader's chrome behavior around them -->

## Impact

- `templates/reader.mustache` (nav layout, hidden state, toggle logic, page buttons), `templates/top.mustache` (or reader template renders its own head/shell without the app header) — the reader route stops including the app header; `/read` remains a distinct full-screen surface
- `templates/reader.mustache` JS (`keys()` unchanged, new `navShown` state + page-button rendering + select sync)
- No store, API, plugin or cache changes
- No new dependencies; Tailwind utilities only
