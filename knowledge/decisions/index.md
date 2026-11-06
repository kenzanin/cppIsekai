# Decisions
* [Build system: CMake + C++23, exe-only](build-toolchain-cpp23.md) - Single executable target, C++23, cmake floor 3.20; lib/tests follow once the first module lands.
* [Dependency vendoring: system-shared first, minimal vendoring](third-party-dependencies.md) - Dependencies: crow/asio/libvips/lua54/curl-headers/cli11 from pacman; sol2, BS::thread_pool, readerwriterqueue, glaze, wasm3, libsql amalgamation, curl-impersonate and simpleini vendored minimal; pin hashes in README
