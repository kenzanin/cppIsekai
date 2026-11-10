# Tasks

## 1. Backend (read state)

- [ ] 1.1 `src/http/http_api.cpp`: `kReadPrefix "host/read/"`; `POST /chapter/<string>/read` body `{page}` -> `state_set(kStateHost, kReadPrefix+id, page)` (`0` clears); hook in `POST /reader/position` after `position_set`: `state_set(..., max(stored, page))` for the chapter.
- [ ] 1.2 `POST /chapter/state` body `{ids:[...]}` -> `{<id>: {skip, read}}` from `state_get` per id (absent key = fields omitted).

## 2. Detail page

- [ ] 2.1 `templates/detail.mustache` row: `[eye svg toggle] [ch+title -> /read link] [date] [read/total] [double-check svg toggle] [cbz]`; client-side pager (20/page, prev/next + n/m, hidden on single page).
- [ ] 2.2 Wire: one `POST /chapter/state` boot fetch paints skip/read; toggles POST then reconcile locally.

## 3. Verification

- [ ] 3.1 build + `ctest` green (no new targets; route logic covered by live smoke); `openspec validate --changes` green; live smoke: pager, eye persists, check lit at done + auto via reader, counter, no console errors.
