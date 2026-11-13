# Proposal: curl-impersonate-families

## Why

The fetch layer currently links only `libcurl-impersonate-chrome.so` (BoringSSL) in-process.
`libcurl-impersonate-ff.so` (NSS) is vendored but never loaded (`firefox_api()` returns
`nullptr`) because NSS and BoringSSL cannot coexist in one process — loading both segfaults
in `curl_multi_perform`. The system `libcurl.so.4` (OpenSSL) is already vendored-loaded via
`dlopen` but only for `req.plain` (image-cache fetcher), not as a fetch fallback.

Two gaps:
1. No `ff*` / firefox family can actually be used — requesting `ff153` silently degrades to
   the chrome engine with no impersonation.
2. When the chrome impersonate build repeatedly fails TLS (`CURLE_SSL_CONNECT_ERROR` against
   big LBs), there is no fallback engine besides a single fresh-handle retry on the same lib.

## What Changes

- Rework the curl-family selection so each profile resolves to a concrete `CurlApi`:
  - chrome/edge/safari → the linked impersonate-chrome lib (as today)
  - firefox/ff/tor → fall back to system libcurl (OpenSSL) instead of silently degrading to
    chrome-with-no-impersonation. This is honest: the ff family cannot impersonate in-process,
    so it degrades to plain TLS rather than pretending.
- Add system libcurl (`libcurl.so.4` via `dlopen`, already probed in `system_api()`) as a
  fallback engine inside `sync()`: after the impersonate lib fails `CURLE_SSL_CONNECT_ERROR`
  on both attempts, retry once on system libcurl before returning an error.
- Keep `is_known_target()` updated with the real family mapping (chrome*, edge*, safari*,
  ff*, firefox*, tor*).

## Capabilities

### New Capabilities
- (none)

### Modified Capabilities
- `fetch-gate`: curl-family selection + system-libcurl fallback; `is_known_target()` covers
  ff/tor targets honestly.

## Impact

- Code: `src/fetch/fetch_curl.cpp` (`firefox_api()`), `src/fetch/fetch.cpp` (`sync()` fallback),
  `src/fetch/fetch.hpp` (`is_known_target()`, family mapping).
- Config: none (no new ini keys; `fetch_profile` already exists).
- No change to the vendored libs, the manifest, the ABI, or any host `.ini` key.
