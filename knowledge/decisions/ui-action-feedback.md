---
type: Decision
title: Shared toast + LED action-feedback contract (feedback.mustache; no new lib/route)
description: "One shared feedback partial (templates/feedback.mustache, included from top layout + reader_head) renders toasts and LED state for every server-calling action; no new JS library or static route; local-only toggles excluded; golden fixtures relocked in two steps."
tags: [ui, templates, toast, feedback, golden-fixtures, decision]
generated: { by: agent/mcp, at: "2026-11-21T06:46:28Z" }
status: stable
---

# Decision: shared toast + LED action feedback

## Contract
- Single shared partial `templates/feedback.mustache` holds the toast markup and LED (in-flight/done/error indicator) states; it is included once from the top layout and from `reader_head` so every page (reader included) gets identical feedback UI.
- Every server-calling action (save, skip, config POST, mark-read, unhide, CBZ export, ...) surfaces a toast on success and on failure. No action hits the server silently.
- No new JS library and no new static-file route: feedback is mustache + inline JS served by the existing template pipeline, same as the rest of the UI.
- Local-only toggles (dim/blur, tri-state badge filters, reader prefs that never POST) are excluded — they already give instant visual state and must not fire toasts.

## Verification
- Golden template fixtures follow the two-step relock: regenerate with `CPPISEKAI_UPDATE_GOLDEN=1`, then re-run the suite to confirm the locked output (never commit a blind regen).

## Rejected alternatives
- Per-page duplicated toast markup (drift); a toast JS library (new dependency for ~20 lines); a `/static/feedback.js` route (route surface for one snippet).

# Related Concepts
- [host.text.clean: lexbor entity fixpoint + RE2/utf8proc numeric sweep](host-text-entity-fixpoint.md): Both are shared-core contracts: ui-action-feedback keeps action feedback in one template partial exactly as host-text-entity-fixpoint keeps text cleanup in one host core; single-source-of-truth over per-site copies.
