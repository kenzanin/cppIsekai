# Design: curl-impersonate-families

## Context

The fetch gate (`src/fetch/`) talks to one curl family per request. Today:
- `chrome_api()` — linked `libcurl-impersonate-chrome.so` (BoringSSL), serves all non-plain requests.
- `firefox_api()` — `nullptr`; `ff*`/`firefox*`/`tor*` targets fall through to `chrome_api()` with
  impersonation skipped (silent degradation, dishonest).
- `system_api()` — `dlopen("libcurl.so.4")` (OpenSSL), used only when `req.plain == true`.

Conflict: loading chrome (BoringSSL) and firefox (NSS) libcurl in one process segfaults at
`curl_multi_perform` (two SSL global inits). Prefixing exported symbols (`chrome_*`/`ff_*`) does
not fix this — the collision is in SSL global initialization, not symbol names. This is a
vendored binary constraint, not a naming problem.

## Goals / Non-Goals

- Goals: honest family selection (ff/tor → system OpenSSL, not chrome-pretending); system
  libcurl as a fallback engine when impersonation repeatedly fails TLS; no new vendor deps;
  zero-warning build; 17/17 tests green.
- Non-Goals: loading both TLS stacks in-process; prefixing vendored symbols; building
  curl-impersonate from source; per-domain header policy.

## Decisions

**D1. Family mapping: ff/tor → system OpenSSL, not chrome.**
`family_of()` already splits on `ff`/`firefox` prefix. `sync()` routes Firefox-family profiles
to `system_api()` (OpenSSL, no impersonation) instead of `chrome_api()`. The `is_known_target()`
list gains `ff153`, `ff154`, `ff155`, `firefox153`, `firefox154`, `firefox155`, `tor145` as
known targets served by the system family.

**D2. System libcurl as fallback engine in `sync()`.**
After the impersonate lib fails `CURLE_SSL_CONNECT_ERROR` on both fresh-handle attempts,
`sync()` retries once on `system_api()` (OpenSSL) before returning the error. This covers the
ponytail observation that impersonate builds intermittently drop the handshake against big LBs.
Plain-text HTTP and non-SSL failures are not retried on system libcurl (they are not TLS
issues).

**D3. No symbol prefixing on vendored libs.**
The `chrome_*`/`ff_*` prefix idea is rejected: it does not resolve the BoringSSL-vs-NSS global
init collision, and vendored binaries are not rebuilt. Families are separated at runtime by
choosing which `CurlApi` table to call, not by symbol names.

**D4. `req.plain` keeps precedence.**
If `req.plain == true`, system OpenSSL is used first (as today); impersonation is skipped.
The fallback chain only applies to impersonated requests.

## Risks / Trade-offs

- [Firefox profile does not impersonate] → ff/tor requests use stock OpenSSL TLS; sites that
  fingerprint TLS will see a non-browser client. Documented; honest degradation beats silent
  chrome fallback.
- [System libcurl dlopen may fail on minimal hosts] → `system_api()` returns nullptr; the
  fallback is skipped and the original impersonate error is returned (no crash).
- [Two libcurl instances need independent SSL init] → `system_api()` calls `curl_global_init`
  on its own handle before use; already proven in the image-cache path.

## Migration Plan

1. Update `is_known_target()` + family mapping in `fetch.hpp`/`fetch_curl.cpp`.
2. Route Firefox-family profiles to `system_api()` in `sync()`.
3. Add system-libcurl fallback after double `SSL_CONNECT_ERROR` in `sync()`.
4. Rebuild, zero-warning gate, full test suite, daemon restart, manual ff/ff-fallback verification.
