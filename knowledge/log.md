## 2026-11-06
* **Update**: Linked `references/code-layers.md` to `architecture/home-demo.md` (Layer map shows how the home-demo decision is realized in source (paths updated after restructure)).
* **Update**: Updated concept `references/code-layers.md`.
* **Update**: Linked `references/tooling.md` to `references/code-layers.md` (Test/lint tooling executes the code-layer rules for future modules).
* **Creation**: Documented concept `references/tooling.md` (Test and lint tooling (doctest, clang-tidy, clang-format)).
* **Update**: Linked `references/dependency-api.md` to `decisions/third-party-dependencies.md` (Dependency decisions reference updated with the sqlite_orm entry).
* **Update**: Linked `architecture/store-orm-migration.md` to `references/dependency-api.md` (sqlite_orm entry in the dependency API reference with the escape/limit/pragma caveats).
* **Update**: Linked `architecture/store-orm-migration.md` to `references/code-layers.md` (ORM migration realizes the code-layer rules: Store remains the sole SQL gate, now via sqlite_orm).
* **Creation**: Documented concept `architecture/store-orm-migration.md` (Store migrated to sqlite_orm (Pimpl)).
* **Update**: Linked `references/dependency-api.md` to `decisions/third-party-dependencies.md` (API reference untuk dependensi yang diputuskan di concept ini).
* **Update**: Linked `decisions/third-party-dependencies.md` to `decisions/build-toolchain-cpp23.md` (Dependency choices (system cli11, vendored simpleini, dst) implement the toolchain/build decision).
* **Update**: Linked `references/code-layers.md` to `references/dependency-api.md` (Log bridge uses the verified ReaderWriterQueue/spdlog entry points).
* **Update**: Linked `references/code-layers.md` to `architecture/home-demo.md` (Code layer map shows how the home-demo decision is realized in source).
* **Creation**: Documented concept `references/code-layers.md` (Source layer map and rules (store / log / http)).
* **Update**: Updated concept `architecture/home-demo.md`.
* **Update**: Linked `architecture/home-demo.md` to `architecture/manga-viewer-core.md` (Home demo is the first slice of the core architecture).
* **Creation**: Documented concept `architecture/home-demo.md` (Home demo: library grid, plugin filter, live log).
* **Update**: Linked `architecture/manga-viewer-core.md` to `references/dependency-api.md` (Plugin ABI function names come from the verified dependency API reference).
* **Update**: Linked `architecture/manga-viewer-core.md` to `decisions/third-party-dependencies.md` (Core architecture consumes the vendored dependency set decided here).
* **Creation**: Documented concept `architecture/manga-viewer-core.md` (cppIsekai core architecture: plugin-per-site manga viewer).
* **Update**: Linked `references/dependency-api.md` to `decisions/third-party-dependencies.md` (API reference for the dependencies decided in this concept).
* **Update**: Linked `decisions/third-party-dependencies.md` to `decisions/build-toolchain-cpp23.md` (Dependency vendoring decision executes the toolchain decision).
* **Update**: Updated concept `references/dependency-api.md`.
* **Update**: Updated concept `decisions/third-party-dependencies.md`.
* **Update**: Updated concept `decisions/build-toolchain-cpp23.md`.
* **Update**: Linked `references/dependency-api.md` to `decisions/third-party-dependencies.md` (Daftar fungsi/APA dari dependensi yang diputuskan di concept ini).
* **Creation**: Documented concept `references/dependency-api.md` (API surface dependensi cppIsekai).
* **Update**: Linked `decisions/third-party-dependencies.md` to `decisions/build-toolchain-cpp23.md` (Pilihan vendor/linking dependensi mengeksekusi keputusan toolchain build (C++23, shared-first)).
* **Creation**: Documented concept `decisions/third-party-dependencies.md` (Dependency vendoring: system-shared dulu, vendor minimal).
* **Creation**: Documented concept `decisions/build-toolchain-cpp23.md` (Build system: CMake + C++23, exe-only).
* **Creation**: Initialized OKF v0.2 project memory.
