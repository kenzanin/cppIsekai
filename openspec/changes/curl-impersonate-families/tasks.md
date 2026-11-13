# Tasks: curl-impersonate-families

## 1. Family mapping (honest ff/tor → system OpenSSL)
- [ ] 1.1 Extend `is_known_target()` in `src/fetch/fetch.hpp` with `ff153`, `ff154`, `ff155`, `firefox153`, `firefox154`, `firefox155`, `tor145`
- [ ] 1.2 Route Firefox-family profiles to `system_api()` in `src/fetch/fetch.cpp::sync()` instead of falling through to chrome

## 2. System libcurl fallback engine
- [ ] 2.1 In `src/fetch/fetch.cpp::sync()`, after impersonate lib fails `CURLE_SSL_CONNECT_ERROR` on both attempts, retry once on `system_api()` (OpenSSL) before returning error
- [ ] 2.2 Keep `req.plain == true` precedence (system OpenSSL first, impersonation skipped)

## 3. Verification
- [ ] 3.1 `xmake -y` builds with zero warnings (-Wall -Wextra)
- [ ] 3.2 Full test suite: 17/17 green (`xmake test`)
- [ ] 3.3 Daemon restarted; `ff153`/`firefox155`/`tor145` targets serve via system OpenSSL (not chrome-pretending)
- [ ] 3.4 Fallback path: impersonate `SSL_CONNECT_ERROR` → system libcurl succeeds (manual curl against a TLS site)
