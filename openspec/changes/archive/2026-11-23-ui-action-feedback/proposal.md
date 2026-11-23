# Proposal: ui-action-feedback

## Why

Clicking **⟳ Update** on a manga detail page gives zero confirmation: the button shows
`…` while the request runs, then silently returns. Success, failure and "nothing
changed" are indistinguishable. The same is true for save settings, restart, plugin
toggle/reload/cache-clear, library add/remove/pin, chapter read flags and enrichment
actions. A stale red `err` line is often left behind even after a later success.
Users cannot tell whether an operation worked.

## What

Add one shared feedback primitive and wire every user-initiated operation to it:

1. A `toast(kind, message)` helper (success / error / info) with auto-dismiss and a
   persistent LED-style result dot next to the button that triggered it.
2. A small `run(fn, button)` wrapper that sets a pending LED, disables the button,
   clears stale errors, and emits the right toast from a `{ok, error}` result.
3. Wire all mutating actions on detail, settings, plugins, migrate, and reader
   restart pages. Local-only toggles (filters, sort, page nav) keep their existing
   visual active-state — no toast spam.
4. Re-lock template goldens and verify in a real browser that success and failure
   both produce visible feedback.

## Non-goals

- No new JavaScript library — reuse Alpine + Tailwind already vendored.
- No server-side response shape changes; every route already returns `{ok, error}`.
- No visual redesign or layout changes beyond the toast host and a result dot.
- No toast for automatic background work (reader position save, cover prefetch).
