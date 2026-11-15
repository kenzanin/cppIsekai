# Design: ui-polish

## D1. Category filter reuses the tri-state badge mechanism

**Decision.** Category chips are instances of the existing `toggleBadge(kind)` tri-state
(off -> show -> hide -> off) with keys `ongoing`, `hiatus`, `canceled`, `completed`, rather
than a new filter component.

**Why.** The grid already has one filter model, one `apply()` pass and one client-side page
reset. A second mechanism would need its own reset and pagination interaction, and the two
would drift. The only new thing a category needs is a bit to test, and that bit already
exists.

**Consequence.** `filterState` grows four keys. `matches()` gains a category clause that
tests `status_flags & categoryMask` instead of comparing the `status` string — the string
stays the display source only, per `architecture/status-flags-bitfield`.

## D2. Unknown status renders a neutral chip, never an empty one

**Decision.** When the category mask is 0, the card renders a neutral chip carrying the raw
`status` string; the category chips simply do not match it.

**Why.** A plugin is free to return a status the host does not fold into a bit (the fold
covers ongoing/hiatus/canceled/cancelled/completed only). Rendering nothing would silently
hide a real state; inventing a bit for it would put plugin vocabulary into the bitfield.

## D3. Dim toggle on the card reflects the server bit

**Decision.** The grid toggle posts the dim change and re-renders from the returned
`status_flags`, rather than optimistically flipping local state.

**Why.** Dim now lives on the saved row and the write is a no-op for an unsaved manga
(`saved_dim_update` only touches saved entries). An optimistic flip would show a dimmed card
for a state the server refused to store. The response is the only correct source.

## D4. Accessibility assertions are markup, so goldens can lock them

**Decision.** Assert the presence of `aria-label` on the icon-only controls in
`tests/test_templates.cpp` against the rendered grid, instead of leaving the requirement to
a manual review.

**Why.** U3's gap was found by counting occurrences across templates, which is exactly the
kind of check that rot as soon as a control is added. A golden assertion costs one line per
control and fails at the point of regression.

**Limit.** A golden cannot judge whether the label is *good* or whether the focus ring is
visible on a dark surface. Those stay a browser check and are recorded in `tasks.md`, not
claimed as verified.

## D5. Additions, not modifications, to the specs

**Decision.** Express the new behaviour as ADDED requirements instead of MODIFYING the
existing `Saved entries are listed` requirement.

**Why.** `status-flags-followup-cleanup` is complete but unarchived, so the main
`library-save` spec still describes the dropped `has_update` column. A MODIFIED delta here
would have to reproduce text that another pending delta is about to rewrite. Additions are
independent of merge and archive order.
