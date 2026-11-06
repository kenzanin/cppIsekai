---
type: decision
title: Store migrated to sqlite_orm (Pimpl)
description: "Store on sqlite_orm 1.9.1 behind Pimpl, API unchanged; five handoff-vs-header deltas recorded; on_db deadlock fixed; LIKE escaping preserved and verified"
tags: [database, orm, store]
generated: { by: agent/mcp, at: "2026-11-06T13:50:06Z" }
status: stable
---

Store migrated from raw sqlite3 C API to sqlite_orm v1.9.1 (vendored single header, third_party/sqlite_orm, AGPL-3.0). Pimpl pattern: StoreImpl hides the decltype(make_storage(...)) template type so store.hpp stays ORM-free and compile-light. Public API unchanged (plugins/count/search); structs keep field names titles/plugin so http.cpp and templates untouched.

Handoff-vs-header deltas (handoff code did not compile as written; these are the v1.9.1 truths):
- pragma_t has NO foreign_keys() setter; make_storage enables FK automatically because the schema declares foreign_key().
- count(X) is the aggregate free function count(&Entity::member); count<T>(member) does not exist. Store::count member shadows it -- must qualify sqlite_orm::count inside the class.
- No is_true; 'no filter' expressed as like(col, "%").
- Standalone offset() has an incomplete serializer (linker/type error); use the limit(lim, offset(off)) overload.
- like(...).escape('\') binds char via field_printer<char> which prints the NUMERIC value (ESCAPE 92) -> SQL error 'ESCAPE expression must be a single character'; must pass std::string("\\").
- Handoff renamed public fields title_count/plugin_name; deliberately NOT adopted (breaking http.cpp/templates for nothing).

Also fixed a REAL deadlock on the way: on_db() posted fn() to the db pool without try/catch -- a throwing lambda never called promise::set_value, so the route future blocked forever and ALL routes hung (even /logs via the shared sidebar). Now the worker catches and set_exception, the caller logs via spdlog and rethrows; failure surfaces as Crow 500 instead of a hang.

LIKE escaping preserved (regression-caught and fixed): like_pattern() escapes %, _ and \ and every like pairs .escape(std::string("\\")). Verified end-to-end via page smokes: q=_ -> 0, q=Asura%2 -> 0, q='Scans #3' -> 4, plugin=Reaper -> 12, home -> 12, no 500s. WAL confirmed (PRAGMA journal_mode=wal), 48 seed rows.

# Related Concepts
- [Source layer map and rules (store / log / http)](../references/code-layers.md): ORM migration realizes the code-layer rules: Store remains the sole SQL gate, now via sqlite_orm
- [cppIsekai dependency API surface](../references/dependency-api.md): sqlite_orm entry in the dependency API reference with the escape/limit/pragma caveats
