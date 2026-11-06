# References
* [cppIsekai dependency API surface](dependency-api.md) - Per-dependency functions and entry points with include paths; header-verified, includes curl_easy_impersonate, SimpleIni GetLongValue and sqlite_orm escape/limit/pragma caveats
* [Source layer map and rules (src/<scope>/, .mustache templates)](code-layers.md) - Layer map of the shipped tree: src grouped by scope (store/http/log), templates .mustache, Store is the only SQL, on_db is the only cross-thread path
* [Test and lint tooling (doctest, clang-tidy, clang-format)](tooling.md) - doctest + ctest + clang-tidy/format targets wired; system packages only; LIKE-% lesson recorded
