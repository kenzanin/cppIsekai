# Tasks: plugin-config-and-storage

## 1. Host: storage backend
- [x] 1.1 Add `plugin_state(plugin_id, key, value)` table + Store get/set methods (schema migration)

## 2. Host: host-tool bindings
- [x] 2.1 Extend `http.fetch` with `method`, `body`, `headers` options (custom request, headers merged after built-ins)
- [x] 2.2 Add `config.get(key)` — parse plugin-folder `config.json` once per load, cache, nil on missing
- [x] 2.3 Add `storage.get`/`storage.set` bindings wired to Store plugin_state

## 3. Mangadex plugin
- [x] 3.1 Add `config.example.json` (+ template for users; data/ is untracked so no extra .gitignore rule needed)
- [x] 3.2 Auth logic in `main.lua`: login (password flow), token caching in storage with `expires_at`, refresh flow, 401 retry-once, Authorization header only on api.mangadex.org

## 4. Verification
- [x] 4.1 Unauth path: rebuild, run host, search/detail/chapters behave as before (no auth attempts in logs)
- [x] 4.2 Auth path: with real personal-client `config.json`, login succeeds (3 tokens in DB), forced-expiry refresh ok, garbage-refresh -> full re-login ok; 401-retry branch present (public MD endpoints tolerate any bearer, no natural 401 today)
- [x] 4.3 Storage: set → host restart → get returns value (proved by fresh-connection reopen in test_store; sqlite file is the restart boundary)
