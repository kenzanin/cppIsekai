# Spec Delta

## ADDED Requirements

### Requirement: Per-call impersonation profile
The host fetch gate SHALL accept a profile identifier on every request; when present it
SHALL override the configured default for that call only, and an unknown profile SHALL
fall back to the configured default with a warning, never a failed request. Changing the
configured default SHALL take effect for subsequent calls without host restart or plugin
reload.

#### Scenario: Per-call override
- **WHEN** a plugin passes `profile: "edge101"` in request options while the configured
  default is `chrome116`
- **THEN** that single request impersonates edge101 and later requests without the option
  use chrome116

#### Scenario: Live default change
- **WHEN** `POST /config` sets `plugin.fetch_profile` to another known target
- **THEN** the next plugin request without an explicit profile uses the new value without
  host restart

### Requirement: Persistent per-plugin cookie jar
The host fetch gate SHALL support a per-plugin cookie jar stored at
`data/plugin-state/<id>.cookies.txt`, enabled per request via options; cookies SHALL
persist across host restarts and SHALL NOT be shared between plugins. When the option is
absent the request SHALL behave exactly as before the feature (no cookie engine).

#### Scenario: Session survives restart
- **WHEN** a plugin with an enabled jar receives session cookies and the host restarts
- **THEN** the next request from that plugin reuses the stored cookies

#### Scenario: Isolation between plugins
- **WHEN** two plugins with separate jars fetch from the same site
- **THEN** neither observes the other's cookies
