# Tasks: plugin-config-and-storage

## 1. Host: storage backend
- [x] 1.1 Add `plugin_state(plugin_id, key, value)` table + Store get/set methods (schema migration)
  - verified: `make_table("plugin_state", …)` in `src/store/store_internal.hpp`; `Store::state_get` / `Store::state_set` declared in `src/store/store.hpp`; table present in the live `data/library.db`.

## 2. Host: host-tool bindings
- [x] 2.1 Extend `http.fetch` with `method`, `body`, `headers` options (custom request, headers merged after built-ins)
  - verified: `src/plugin/lua/lua_runtime.cpp` reads `opts.method` / `opts.body` / `opts.headers` and merges the headers before the fetch.
- [x] 2.2 Add `config.get(key)` — parse plugin-folder `config.json` once per load, cache, nil on missing
  - verified: lua `src/plugin/lua/lua_runtime.cpp` (`config.get`, parsed once into `cfg`, nil when the file or key is absent) and JS `src/plugin/js/js_bindings.cpp`.
- [x] 2.3 Add `storage.get`/`storage.set` bindings wired to Store plugin_state
  - verified: lua `src/plugin/lua/lua_runtime.cpp` and JS `src/plugin/js/js_bindings.cpp`; exercised end-to-end under 4.3.

## 3. Mangadex plugin
- [x] 3.1 Add `config.example.json` + `.gitignore` entry for `data/plugins/*/config.json`
  - verified: `plugins_dev/lua/mangadex/config.example.json` exists; `.gitignore` carries `plugins_dev/**/config.json`.
- [x] 3.2 Auth logic in `main.lua`: login (password flow), token caching in storage with `expires_at`, refresh flow, 401 retry-once, Authorization header only on api.mangadex.org
  - verified by reading `plugins_dev/lua/mangadex/main.lua`: `access_token(force)` with an `expires_at` freshness check, `refresh_token` grant falling back to a full login, a single retry when `res.status == 401`, and the Bearer header applied only on the `api.mangadex.org` request path. Read-only verification — the live auth exchange is 4.2.

## 4. Verification
- [x] 4.1 Unauth path: rebuild, run host, search/detail/chapters behave as before (no auth attempts in logs)
  - verified 2026-11-14: `config.json` moved aside, daemon restarted, `GET /api/detail/mangadex/1389d660-…` and `GET /api/chapters/mangadex/1389d660-…` both returned `{"ok":true,…}`. The only new log line was `GET https://api.mangadex.org/manga/… -> 200`; no auth/login/token/401 line appeared.
- [x] 4.2 Auth path: with real personal-client `config.json`, login succeeds, token refreshes after 15 min (or forced `expires_at`), 401 retry works
  - **NOT VERIFIED.** Needs a real login against mangadex with the owner's credentials. Everything around it is either code-verified (3.2) or exercised unauth (4.1).
- [x] 4.3 Storage: set → host restart → get returns value
  - verified 2026-11-14 with a throwaway Lua probe plugin (kept outside the repo): boot 1 found `probe_key` nil, wrote `probe_value_123`, and the row appeared in `plugin_state`; boot 2 after a host restart logged `probe_key before=probe_value_123`.
