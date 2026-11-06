# Architecture
* [cppIsekai core architecture: plugin-per-site manga viewer](manga-viewer-core.md) - Plugin-per-site manga viewer: plugins parse HTML themselves, host = crow+mustache + libsql + impersonate proxy w/ disk cache; light sandbox; zero new deps
* [Home demo: library grid, plugin filter, live log](home-demo.md) - Home demo live: partial layouts, htmx grid swaps, Crow built-in /static, PORT env; no mustache inheritance in crow
* [Store migrated to sqlite_orm (Pimpl)](store-orm-migration.md) - Store on sqlite_orm 1.9.1 behind Pimpl, API unchanged; five handoff-vs-header deltas recorded; on_db deadlock fixed; LIKE escaping preserved and verified
