# Design: ui-action-feedback

## D1 — One partial, no new static asset

Define the primitive in a new mustache partial `templates/feedback.mustache` and include
it from `templates/top.mustache` (9 pages) and `templates/reader_head.mustache` (reader
page). This avoids a new `/static/*.js` route, a new `br_asset` entry and `.br`
regeneration — the script is inline like the existing `top.mustache` dim-blur helper.

## D2 — `toast(kind, message)`

`window.toast(kind, message)` appends a compact fixed-position element to
`#toast-host`. `kind` is `success` (emerald), `error` (red) or `info` (zinc). Toasts
auto-dismiss after 5 s on success/info and 8 s on error; clicking one dismisses it.
Cap the host at 4 visible toasts (drop the oldest) so a burst of chapter toggles does
not cover the page. Existing Tailwind classes only — no new CSS framework.

## D3 — `run(job, btn)` wrapper

`window.runFeedback(job, btn)`:
- sets `btn.dataset.busy = "1"`, disables it, and shows a pending LED (a small dot
  whose class flips to amber while busy);
- clears the component's `err` field when the caller passes `onStart`;
- awaits `job()`, which resolves to `{ok: true, message?}` or `{ok: false, error}`;
- emits the toast, flips the LED to emerald (ok) or red (fail) for 3 s, re-enables the
  button, and returns the result to the caller so page state can still be updated.
- a thrown exception is caught and becomes an error toast — no unhandled rejection.

The wrapper is opt-in per handler: existing `refreshing` / `libraryBusy` flags stay as
the source of truth for button labels; the wrapper adds feedback, not a second state
machine.

## D4 — Surface inventory (wired in this change)

| Page | Operations |
|---|---|
| detail | Update, add/remove library, pin, mark all read/clear, per-chapter read/done/mark-prev, enrichment fetch + force, reset detail/cover, pick/toggle field, add/remove all related, dim+blur |
| settings | Save config, Restart server (success = request accepted, then existing poll takes over), Discard edits (local → info toast) |
| plugins | Enable/disable, clear cache, reload all |
| migrate | Search results, Migrate here |
| reader | Restart server in footer; no toast for auto position save |

Local-only actions (grid filters/sort/pagination, search pager, reader nav/zoom,
enrichment panel open/close) already render their active state; they are explicitly
out of toast scope.

## D5 — Result contract from routes

All touched routes already return JSON `{ok: true}` / `{ok: false, error: "..."}`
or HTTP 200 with `ok` false. The wrapper reads `j.ok` and `j.error`; no server change.
For `/api/refresh` a 502 body also carries `error`, so failure is still observable.

## D6 — Stale error fix

Every wired handler clears `this.err = ''` before the request. Today `refresh()` and
`toggleLibrary()` only set `err` on failure, so an old message survives a later
success — the toast makes the current outcome unambiguous and the stale line is gone.

## D7 — Tests and goldens

- Template goldens for all 9 pages + reader re-lock via the two-step
  `CPPISEKAI_UPDATE_GOLDEN=1` then plain `CPPISEKAI_UPDATE_GOLDEN_ACK=1` run; diff
  reviewed so only the feedback partial / data attributes land.
- `tests/test_gridjs` keeps passing (it extracts the grid script; the feedback partial
  is a separate `<script>` and must not break its extraction).
- Browser verification: click a failing action (e.g. Update with a disabled plugin)
  → red error toast; click a successful one → emerald success toast; LED returns to
  idle and the button re-enables.

## Risks

- Toast overlay on small screens: cap at 4 and anchor bottom-right with pointer-events
  only on the toast itself.
- Double-success toasts from handlers that both toast and set `this.err`: the `err`
  line stays as a detail panel, the toast is the transient signal — acceptable.
