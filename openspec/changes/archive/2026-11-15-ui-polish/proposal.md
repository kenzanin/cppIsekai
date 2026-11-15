# Proposal: ui-polish

## Why

The library grid gained status category bits (ongoing/hiatus/canceled/completed) and a
per-manga dim bit, but the grid UI only exposes New/Done/Complete. A title classified
`hiatus` or `canceled` is not filterable from the grid at all, and dim — a per-entry toggle
that now lives on `saved_manga.status_flags` bit6 — can only be set from the detail page.
Both are gaps between what the store models and what the user can reach.

Accessibility is the third gap, and it is measurable rather than a matter of taste: across
`templates/` there are 10 `aria-label` occurrences, in `reader_top.mustache`,
`reader_bottom.mustache` and `detail.mustache`. `grid_body.mustache` has none, so its
icon-only controls (badge filters, per-card plugin button, dim toggle) are unnamed to a
screen reader.

## What Changes

- **Grid category filter (U1).** Add category chips alongside the existing New/Done/Complete
  controls, driven by the stored category bits. A title whose category mask is 0 keeps
  rendering a neutral chip from the raw `status` string, so an unrecognised plugin status
  never renders as a blank badge.
- **Dim from the grid (U5).** Expose the dim toggle on the grid card so the state is
  reachable without opening the detail page. The grid must reflect the returned bit rather
  than assume the toggle succeeded.
- **Accessible names (U3).** Give every icon-only control in the grid an `aria-label`, and
  keep the focus ring visible against the dark zinc surfaces.

## What does not change

Badge semantics stay derived from `status_flags`; nothing in the UI recomputes New or Done
from `total_ch`/`read_ch`. The store and its API are untouched — every field these controls
need is already returned.

## Capabilities

### New Capabilities
_(none)_

### Modified Capabilities
- `library-save`: adds the grid category filter and the grid-side dim toggle as stated
  requirements.
- `manga-detail-page`: adds accessible names for icon-only controls as a stated requirement.

## Dependencies

`openspec/changes/status-flags-followup-cleanup` must be archived before this change is
archived. The main `library-save` spec still lists the dropped `has_update` column because
that change is complete but not yet archived; these deltas are written as additions so the
two do not have to be merged in a particular order, but the archive pass must not leave
`has_update` behind.

## Impact

- `templates/grid_body.mustache` — category chips, dim control, `aria-label`s.
- `templates/detail.mustache`, `templates/reader_*.mustache` — `aria-label` gaps only.
- `tests/fixtures/golden/*.html` — re-lock, reviewed diff limited to the intended markup.
- `tests/test_templates.cpp` — lock the new controls.

## Out of scope

- Restyling the grid. Chips reuse the existing badge classes.
- Settings page provenance (already shipped: `GET /config` returns `source`, and the
  `settings-page` spec already requires the winning layer per knob).
- Reader polish (mode memory, loading states, per-page error tiles) and the detail chapter
  list pager — both remain in `references/rec-ui-polish` unstarted.
