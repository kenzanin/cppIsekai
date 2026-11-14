# Tasks: ui-polish

## 1. Grid category filter (U1)

- [ ] 1.1 Add `ongoing`, `hiatus`, `canceled`, `completed` keys to `filterState` in
      `templates/grid_body.mustache`, reusing the existing tri-state `toggleBadge` mechanism
- [ ] 1.2 Extend `matches()` with a category clause testing the stored `status_flags`
      category bits; do not compare the `status` string
- [ ] 1.3 Render a neutral chip from the raw `status` string when the category mask is 0
- [ ] 1.4 Confirm the four chips and the New/Done/Complete chips share one client-side page
      reset and do not fight over pagination

## 2. Grid dim toggle (U5)

- [ ] 2.1 Add the dim control to the saved-entry card, mirroring the detail-page toggle
- [ ] 2.2 Derive the dimmed appearance from the `status_flags` bits returned by the server
      after the toggle; no optimistic flip
- [ ] 2.3 Hide the control for cards that are not saved entries

## 3. Accessible names (U3)

- [ ] 3.1 Enumerate every icon-only control in `grid_body.mustache`, `detail.mustache`,
      `reader_top.mustache`, `reader_bottom.mustache` and list the ones without a name
- [ ] 3.2 Add `aria-label` to each gap; check no control relies on `title` alone
- [ ] 3.3 Verify the focus ring is visible on the zinc surfaces and restore any removed outline

## 4. Verification

- [ ] 4.1 `xmake -y` clean with `-Wall -Wextra`
- [ ] 4.2 Re-lock goldens deliberately (`CPPISEKAI_UPDATE_GOLDEN=1`), then read
      `git diff tests/fixtures/golden/` and confirm the diff is limited to the intended markup
- [ ] 4.3 `xmake test` green
- [ ] 4.4 Lock the new controls and the `aria-label` presence in `tests/test_templates.cpp`
- [ ] 4.5 Browser smoke note (what was clicked, what was seen) for the category filter, the
      grid dim toggle and keyboard focus; this change touches templates, so a passing test
      suite is not evidence about the rendered result

## 5. Spec

- [ ] 5.1 Archive `status-flags-followup-cleanup` first, so the main `library-save` spec no
      longer lists the dropped `has_update` column before this change is archived
