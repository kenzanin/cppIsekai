# mangadex-plugin delta

## ADDED Requirements

### Requirement: Optional authentication
When the plugin folder carries a `config.json` with personal-client credentials
(`client_id`, `client_secret`, `username`, `password`), the plugin SHALL authenticate
against `https://auth.mangadex.org/realms/mangadex/protocol/openid-connect/token` using the
`password` flow — a form-urlencoded POST via `http.fetch` — and keep the resulting
`access_token` and `refresh_token` in `storage`.

#### Scenario: Login with credentials
- **WHEN** valid credentials exist and no token is stored yet
- **THEN** the plugin performs one password-flow POST and stores access/refresh tokens with an `expires_at` unix timestamp

### Requirement: Token refresh
The plugin SHALL refresh the access token via the `refresh_token` flow when the stored
`expires_at` (unix seconds) has passed or the API answers 401 (single retry, then error).

#### Scenario: Refresh on expiry
- **WHEN** the stored access token is past `expires_at`
- **THEN** the next API call first performs one refresh POST and succeeds without surfacing an error to the caller

#### Scenario: 401 mid-session
- **WHEN** an API call returns 401 while the stored token looks unexpired
- **THEN** the plugin refreshes once, retries the request, and only then returns the error shape if it still fails

### Requirement: Auth header scoping and graceful fallback
The plugin SHALL attach `Authorization: Bearer <token>` only to `api.mangadex.org`
requests — never to image/upload domains. Without `config.json`, the plugin SHALL run
unauthenticated exactly as before. Authentication failures SHALL surface as the standard
`{"ok": false, "error": ...}` shape, never a crash or retry loop.

#### Scenario: No credentials
- **WHEN** the plugin folder has no `config.json`
- **THEN** search/detail/chapters behave exactly as the unauthenticated spec requires, with no auth attempts and no errors

#### Scenario: Bad credentials
- **WHEN** the token endpoint rejects the login
- **THEN** the plugin returns the error shape with a message identifying auth failure and does not retry in a loop

#### Scenario: Authenticates and refreshes
- **WHEN** valid credentials exist and the stored access token is past `expires_at`
- **THEN** the next API call first performs one refresh POST and succeeds without surfacing an error to the caller

#### Scenario: 401 mid-session
- **WHEN** an API call returns 401 while the stored token looks unexpired
- **THEN** the plugin refreshes once, retries the request, and only then returns the error shape if it still fails

#### Scenario: No credentials
- **WHEN** the plugin folder has no `config.json`
- **THEN** search/detail/chapters behave exactly as the unauthenticated spec requires, with no auth attempts and no errors

#### Scenario: Bad credentials
- **WHEN** the token endpoint rejects the login
- **THEN** the plugin returns the error shape with a message identifying auth failure and does not retry in a loop
