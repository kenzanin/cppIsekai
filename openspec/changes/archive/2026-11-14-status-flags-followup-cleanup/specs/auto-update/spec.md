# Spec Delta: auto-update

## MODIFIED Requirements

### Requirement: Update scope is chapter lists only
An auto-update attempt SHALL call only the plugin's `chapters` method (not `detail`, not covers). It SHALL NOT write `saved_manga` rows directly; any stat refresh flows through the shared refresh path's existing logic. The shared refresh path in chapters-only mode SHALL still advance `total_ch` and recompute `status_flags` bit0 (is_new) for a saved entry whose chapter list grew beyond the stored `total_ch`, using the same single-writer store path the detail-page refresh uses — so the background scheduler keeps the New badge correct without fetching `detail`.

#### Scenario: Metadata not refetched
- **WHEN** an auto-update runs for a title
- **THEN** only the chapters envelope is requested; the title's stored name/cover/synopsis are unchanged by the auto-update itself

#### Scenario: New badge advances from chapter growth
- **WHEN** an auto-update fetches a saved entry's chapter list and the count is greater than the stored `total_ch`
- **THEN** `total_ch` is raised and `status_flags` bit0 is set, without any `detail` call

#### Scenario: No growth leaves the badge alone
- **WHEN** an auto-update fetches a saved entry's chapter list and the count equals the stored `total_ch`
- **THEN** bit0 is unchanged (no spurious New badge)

#### Scenario: Auto-update preserves dim state
- **WHEN** an auto-update runs for an entry whose dim bit is set
- **THEN** bit6 remains set after the stat update
