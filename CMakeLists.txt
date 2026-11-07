cmake_minimum_required(VERSION 3.20)
project(
  cppIsekai
  VERSION 0.1.0
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- system shared libs ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips)
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua5.4)
find_package(spdlog REQUIRED) # system shared 1.17.0; links fmt 12.2.0

set(TP ${PROJECT_SOURCE_DIR}/third_party)

# --- vendored static: wasm3 (C) & libsql sqlite amalgamation ---
# ponytail: in-tree static amalgamation; switch to system-shared libsql once
# pacman/AUR libsql lands
file(GLOB WASM3_SRC CONFIGURE_DEPENDS ${TP}/wasm3/src/*.c)
add_library(wasm3 STATIC ${WASM3_SRC})
target_include_directories(wasm3 PUBLIC ${TP}/wasm3/src)

add_library(sqlite3 STATIC ${TP}/libsql/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${TP}/libsql/sqlite)

# --- app ---
add_executable(cppIsekai src/main.cpp src/store/store.cpp src/http/http.cpp
                         src/plugin/plugin.cpp src/cache/cache.cpp)
target_include_directories(
  cppIsekai
  PRIVATE src
          ${TP}/sol2/include
          ${TP}/thread_pool/include
          ${TP}/readerwriterqueue/include
          ${TP}/glaze
          ${TP}/simpleini
          ${TP}/sqlite_orm/include)
target_link_libraries(
  cppIsekai
  PRIVATE wasm3 sqlite3 PkgConfig::VIPS PkgConfig::LUA
          ${TP}/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0
          spdlog::spdlog)
target_compile_options(cppIsekai PRIVATE -Wall -Wextra)
set_target_properties(cppIsekai PROPERTIES BUILD_RPATH
                                           "${TP}/curl-impersonate/lib")

# --- tests (doctest, system package) ---
# ponytail: single test binary; split per-module when the suite outgrows one
# file
add_executable(test_store tests/test_store.cpp src/store/store.cpp)
target_include_directories(test_store PRIVATE src ${TP}/sqlite_orm/include)
target_link_libraries(test_store PRIVATE sqlite3)
target_compile_options(test_store PRIVATE -Wall -Wextra)
enable_testing()
add_test(NAME store COMMAND test_store)

# plugin module: loader/sandbox/ABI/host tools; fixtures under tests/fixtures
target_include_directories(cppIsekai PRIVATE ${TP}/glaze ${TP}/sol2/include)
add_executable(test_plugin tests/test_plugin.cpp src/plugin/plugin.cpp
                           src/store/store.cpp)
target_include_directories(
  test_plugin
  PRIVATE src ${TP}/glaze ${TP}/sol2/include ${TP}/thread_pool/include
          ${TP}/sqlite_orm/include ${TP}/readerwriterqueue/include)
target_link_libraries(
  test_plugin
  PRIVATE sqlite3 PkgConfig::LUA
          ${TP}/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0
          spdlog::spdlog)
target_compile_options(test_plugin PRIVATE -Wall -Wextra)
target_compile_definitions(
  test_plugin
  PRIVATE TEST_PLUGIN_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/plugins")
set_target_properties(test_plugin PROPERTIES BUILD_RPATH
                                             "${TP}/curl-impersonate/lib")
add_test(NAME plugin COMMAND test_plugin)

# cache core: disk LRU store; fetcher injected -> network-free tests
add_executable(test_cache tests/test_cache.cpp src/cache/cache.cpp)
target_include_directories(test_cache PRIVATE src)
target_compile_options(test_cache PRIVATE -Wall -Wextra)
add_test(NAME cache COMMAND test_cache)

# /cache/img serve-through: pure route core, crow-free; fetcher injected via the
# cache ctor, so no server, curl or store linkage is needed here.
add_executable(test_http_img tests/test_http_img.cpp src/cache/cache.cpp)
target_include_directories(test_http_img PRIVATE src)
target_compile_options(test_http_img PRIVATE -Wall -Wextra)
add_test(NAME http_img COMMAND test_http_img)

# stale layer (2.2): wrapper around the ABI call; cache + glaze, no server
add_executable(test_stale tests/test_stale.cpp src/cache/cache.cpp)
target_include_directories(test_stale PRIVATE src ${TP}/glaze)
target_compile_options(test_stale PRIVATE -Wall -Wextra)
add_test(NAME stale COMMAND test_stale)

# ponytail: no CPack/install yet -- add when packaging matters

find_program(CLANG_TIDY clang-tidy)
find_program(CLANG_FORMAT clang-format)
# optional targets: cmake --build build --target tidy / format
add_custom_target(
  tidy
  COMMAND ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR} src/main.cpp src/store/store.cpp
          src/http/http.cpp src/plugin/plugin.cpp src/cache/cache.cpp
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(
  format
  COMMAND ${CLANG_FORMAT} -i src/*.cpp src/*/*.cpp src/main.cpp src/*/*.hpp
          tests/*.cpp
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
