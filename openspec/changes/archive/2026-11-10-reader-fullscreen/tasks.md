# Tasks

## 1. Reader shell

- [x] 1.1 `templates/reader.mustache`: render a self-contained page (doctype/head/html/body, Tailwind CDN + Alpine + htmx statics as top.mustache does) without the app `Library | Search` header and without `<main class="p-4">`; the image fills `h-screen`. `/read/<plugin>/<slug>` route stays in `src/http/http_pages.cpp` (template content swap only).
- [x] 1.2 Remove the permanent toolbar; introduce `navShown` (default false) in the Alpine component; keep `spacer`, error/loading states, `:dir`.

## 2. Tap-zone + toggle behavior

- [x] 2.1 Center-third tap zone (new click target over the image center, active in all modes) toggles `navShown`; left/right zones keep `step()` unchanged (RTL-aware) and stay active in both nav states; zones stop at the center third so toggles never page.
- [x] 2.2 When nav hidden: `p N / M` dim indicator, `pointer-events-none`, bottom-center; `keys()` arrow-key paging unchanged (form-control guard intact).

## 3. Top bar (shown only while `navShown`)

- [x] 3.1 Fixed top overlay: back control (`/view/manga/<plugin>/<slug>`), chapter `<select>` (all chapters, from the existing `/api/chapters` fetch, navigates via `chUrl(n)`), page `<select>` (`1..N`, sets `page` through the shared jump helper + `persist()`).

## 4. Bottom bar (shown only while `navShown`)

- [x] 4.1 Fixed bottom overlay: existing mode/direction/zoom selectors relocated; page-jump row: numbered buttons `1..N` while N ≤ 24 (current page bright, `firstOf`/`viewOf` mapping, click = `goto_page(n)` + `persist()`); beyond that, one big current-page/jump control that focuses the top bar page select.
- [x] 4.2 Extract the shared `goto_page(n)` helper from `step()` (computes view index, sets `page`, `persist()`, `preload()`), reused by keys/tap/buttons/selects.

## 5. Chapter boundary navigation

- [x] 5.1 `plugins_dev/lua/mangadex/main.lua`: chapters output gains `pages = tonumber(attrs.pages) or 0` (already present in MangaDex `/feed` attributes; no extra request).
- [x] 5.2 `templates/reader.mustache`: extend `step()` — when the target index leaves the views range: prev-boundary tap (first page, prev zone) navigates to `chUrl(prev_ch, prevChapter.pages)` (`&page=<N>` when nonzero, else page 1); next-boundary tap on the last chapter's last view shows the Done overlay instead of navigating. Mode/direction/zoom cross chapters via query params (`&mode=&dir=&zoom=`) appended to chapter-crossing targets.
- [x] 5.3 Add boundary detection: boundary condition computed from `cur()` and `views.length`; reused by keyboard arrows and tap zones.

## 6. Done overlay

- [x] 6.1 Full-screen overlay (shown when `done===true`): centered "Done 🎉", link/button to `/view/manga/<plugin>/<slug>`; hidden otherwise; no nav chrome, no tap zones when active (it swallows center taps); `Esc` hides it.
- [x] 6.2 Trigger: `step(+1)`/forward arrow when on last view and `next_ch` is falsy; also when the chapter list ends before the last view (edge: next_ch=0 with more views — not possible here, next_ch nil only at list end).

## 7. Zoom persistence

- [x] 7.1 Store: additive `zoom` column on `reader_positions` (PRAGMA-guarded `ALTER TABLE ADD COLUMN zoom TEXT DEFAULT ''` for existing DBs + CREATE TABLE for fresh DBs); `ReaderPosition.zoom`; `position_set`/`position_get`/`position_last` map it.
- [x] 7.2 `position_api.hpp` merge: `zoom` absent on the request keeps the stored value (mirror mode/direction); `test_position.cpp` extended.
- [x] 7.3 Routes: `GET /api/position/<plugin>/<slug>` returns `zoom`; `POST /reader/position` accepts `zoom`; http_pages resume seed passes `resume_zoom` and query `zoom=` override honored.
- [x] 7.4 Reader template: zoom seeded from `{{resume_zoom}}`/query, `save()` posts `zoom`, zoom change fires the debounced persist; crossing chapters appends `zoom` to `chUrl` targets.

## 8. Verification

- [x] 8.1 `ctest` unchanged green; build zero warnings.
- [x] 8.2 Live smoke (Chrome via blueprint): open reader full-screen (no app header); center tap shows/hides nav; back returns to detail; chapter select switches chapter; page select jumps; ≤24-page chapter renders numbered buttons with current highlighted; >24-page chapter falls back to big jump control; indicator visible when hidden; arrow keys still page; resume/persist unchanged.
