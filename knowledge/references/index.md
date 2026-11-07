# References
* [cppIsekai dependency API surface + MangaDex auth resolution](dependency-api.md) - Per-dependency functions and entry points with include paths; header-verified, includes curl_easy_impersonate, SimpleIni GetLongValue and sqlite_orm escape/limit/pragma caveats
* [Source layer map and rules (src/<scope>/, .mustache templates)](code-layers.md) - Layer map of the shipped tree: src grouped by scope (store/http/log), templates .mustache, Store is the only SQL, on_db is the only cross-thread path
* [Test and lint tooling (doctest, clang-tidy, clang-format)](tooling.md) - doctest + ctest + clang-tidy/format targets wired; system packages only; LIKE-% lesson recorded
* [MangaDex auth and API quirks (verified)](mangadex-auth-quirks.md) - Verified MangaDex auth/API behaviors: password-grant-only personal clients, public endpoints tolerating any bearer, no auth headers on CDN, impersonate TLS flakiness, token state in plugin_state.
* [MangaDex search ranking = followedCount desc](mangadex-search-ranking.md) - Search mangadex.org = order[followedCount]=desc di API; relevance enum ada tapi tidak menjamin exact-match first; plugin mengirim followedCount dan menerima title/query/q.
