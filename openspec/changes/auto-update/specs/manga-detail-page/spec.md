# Spec Delta

## MODIFIED Requirements

### Requirement: Forced refresh endpoint
`POST /api/refresh/<plugin>/<slug>` SHALL call the plugin's detail and chapters methods, SHALL overwrite both cache entries only after BOTH envelopes validate, SHALL return `{"ok":true}` on success, and SHALL return HTTP 502 with a refresh-failed error without touching the cache on plugin failure. Regardless of outcome, it SHALL stamp the `update_attempts` tracker for `(plugin, slug)` with the observed result (`ok` | `error`) so the auto-update scheduler treats a manual refresh as a fresh attempt and skips the title for the configured interval.

#### Scenario: Successful refresh overwrites cache
- **WHEN** both plugin envelopes validate
- **THEN** both cache entries are replaced and the endpoint returns ok
- **AND** `update_attempts(plugin, slug)` records `outcome = ok` with the current timestamp

#### Scenario: Failed refresh leaves cache intact
- **WHEN** either plugin call fails or an envelope fails validation
- **THEN** the endpoint returns 502 naming the failed method
- **AND** the existing cache entries are unchanged
- **AND** `update_attempts(plugin, slug)` records `outcome = error` with the failure reason

#### Scenario: Manual refresh exempts title from auto-update
- **WHEN** the user clicks the detail-page refresh button and the call completes (either outcome)
- **THEN** the auto-update scheduler does not select that title again until its next interval elapses
