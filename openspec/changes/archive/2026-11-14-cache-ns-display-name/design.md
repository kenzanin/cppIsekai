# Design: cache-ns-display-name

## D1. The namespace is the slugified display name

`cache::cache_ns(plugin_id, name)` returns `slug_base(name)`, falling back to `plugin_id` when
the name is blank or slugifies to nothing (`untitled`). The fallback is not decoration: a
plugin with no display name must still get a directory, and the id is the only other stable
identifier it has.

Slugging reuses the same rules as manga slugs, so a namespace is filesystem-safe by
construction — an `Atsu.moe Reader` plugin lands in `img/atsu-moe-reader/`, never in a
directory with a dot or a space.

**Accepted consequences.**
- Two plugins with the same display name share one cache directory. Titles within it are
  namespaced by manga slug, so the practical risk is a cross-plugin slug collision on the same
  title — the same risk two ids already had within one plugin.
- Renaming a plugin's display name starts a fresh namespace, exactly like renaming its id does
  today. The cache is a cache; the alternative is a persistent mapping table.

## D2. Resolver injected into `Cache`, not looked up inside it

`Cache` gets `using NsResolver = std::function<std::string(const std::string&)>` and
`Cache::ns(plugin)`. It does not include the Store or the plugin registry: `Cache` is
constructed before/independently of them in tests, and a cache that reaches into the plugin
registry to render a path would be a dependency inversion for a string.

With no resolver injected, `ns()` returns the id unchanged, so every existing test and any
caller that never learned about namespaces keeps the old behaviour byte for byte.

`main.cpp` builds the map from `store.plugins()` once at boot and moves it into the lambda. A
reload that renames a plugin is therefore picked up on the next restart, not mid-run — the
namespace must not change under files the cache already indexed.

## D3. Path builders outside `Cache` call `Cache::ns()`

`/api/cbz-ready` and `/export/cbz` compose `data/cache/img/...` themselves. They take the
namespace from `Cache::ns(plugin)` rather than concatenating the request's `plugin` param, so
there is exactly one definition of where a plugin's files live. Those two routes capture
`images` by reference; the routes were previously capture-less.

## D4. One-time cutover, guarded on the resolver

Every id-keyed directory is stale the moment the namespace changes, so the cache cannot serve
it and cannot tell which plugin it belonged to. On the first boot with a resolver injected, the
whole `img/` pool is renamed to `.legacy-img` (a rollback copy the existing `.legacy-` sweep
deletes on the next boot) and `img/` is recreated; `.cache-ns-name` keeps it to one boot.

Guarded on `ns_`: without a resolver the id *is* the namespace, so those pools are live and
must not be touched. Tests that seed an `img/` tree before constructing `Cache` boot without a
resolver and are unaffected, which is also why the guard is a correctness rule and not just a
convenience.

The cutover is a cold image cache, accepted in the proposal. No id → name migration exists to
get wrong: the files are content-addressed by URL, so a re-fetch rebuilds them.

## D5. Delete the context-free sha fallback rather than keep it as the odd path out

The `img/<sha1>.<ext>` fallback let a request with no placement context (neither `cover=1` nor a
complete page context) land bytes directly at the pool root. Nothing could ever read them back:
the reader, the cache-full probe and the CBZ export all address `<ns>/…`, so those files were
dead weight that only LRU could reclaim. Two nudge sites fed it — `/chapter/<id>/skip` (which
had the context in hand and simply did not pass it) and the reader route's "no chapter number"
branch.

Deleting is smaller than fixing: `Cache::img_store` returns nullopt without a placement path,
`Cache::img_ensure` refuses before fetching, `path_for` yields "" for the img pool, and the
worker drops a batch without plugin/slug/chapter. The route then answers 404 for a bare
`?url=`, which costs nothing and says what happened.

The alternative — keep the fallback and only fix the two callers — leaves a second, unreachable
file layout in the codebase for `curl` convenience. Rejected: one layout, one way to name a
file. `api/<sha>.json` keeps its sha naming because that pool is offline-stale storage, not
image placement.
