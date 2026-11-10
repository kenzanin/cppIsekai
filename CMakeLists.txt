cmake_minimum_required(VERSION 3.20)
project(
  cppIsekai
  VERSION 0.1.0
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
set(CMAKE_C_COMPILER_LAUNCHER ccache)

# mold: 5-10x faster links than GNU ld when available; zero cost when not.
find_program(MOLD_LINKER mold)
if(MOLD_LINKER)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=mold")
endif()

# PCH for the daemon's two heavy header clusters: stdlib + glaze/json so
# template-heavy TUs (cache/http_api/plugin) stop paying the preprocess price per-file.
function(_cppisekai_pch target)
  target_precompile_headers(${target} PRIVATE
    <memory> <vector> <string> <optional> <map> <unordered_map> <algorithm> <utility> <filesystem> <mutex> <span> <cstring> <cstdint> <ctime> <array> <variant>
    <string_view> <stdexcept> <functional> <chrono> <fstream> <sstream> <cstddef> <cctype>
    <spdlog/spdlog.h> <spdlog/sinks/stdout_color_sinks.h>)
endfunction()

# vendor-heavy PCH only for the daemon target: test targets lack glaze/BS include dirs
function(_cppisekai_pch_vendor target)
  _cppisekai_pch(${target})
  target_precompile_headers(${target} PRIVATE <glaze/glaze.hpp> <BS_thread_pool.hpp>)
endfunction()


# --- system shared libs ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips)
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua5.4)
find_package(spdlog REQUIRED) # system shared 1.17.0; links fmt 12.2.0
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip) # system shared (pacman libzip); CBZ export

set(TP ${PROJECT_SOURCE_DIR}/third_party)

# system sqlite (3.54.0): full JSONB (jsonb_extract/jsonb_each/jsonb(...)); libsql fork dropped
file(GLOB WASM3_SRC CONFIGURE_DEPENDS ${TP}/wasm3/src/*.c)
add_library(wasm3 STATIC ${WASM3_SRC})
target_include_directories(wasm3 PUBLIC ${TP}/wasm3/src)

# --- vendored static: quickjs-ng (js plugin runtime, plugin-js-wasm; pin d4187c7 2026-11-07) ---
file(GLOB QUICKJS_SRC CONFIGURE_DEPENDS ${TP}/quickjs/*.c)
add_library(quickjs STATIC ${QUICKJS_SRC})
target_include_directories(quickjs PUBLIC ${TP}/quickjs)
target_compile_definitions(quickjs PRIVATE _GNU_SOURCE)
target_link_libraries(quickjs PUBLIC m ${CMAKE_DL_LIBS} pthread)

pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

# --- app ---
add_executable(cppIsekai src/main.cpp src/store/store_core.cpp src/store/store_search.cpp src/store/store_api.cpp src/http/http.cpp src/http/http_pages.cpp src/http/http_api.cpp src/http/http_api_admin.cpp src/http/http_api_library.cpp src/http/http_api_title.cpp src/http/http_api_reader.cpp src/http/http_api_state.cpp src/http/http_api_content.cpp src/http/pages_api.cpp
                         src/fetch/fetch.cpp src/plugin/plugin_core.cpp src/plugin/plugin_load.cpp src/plugin/plugin_call.cpp src/plugin/lua/bridge.cpp src/plugin/lua/lua_runtime.cpp src/plugin/http_client.cpp src/plugin/wasm/wasm_rt.cpp src/plugin/wasm/wasm_load.cpp src/plugin/js/js_rt.cpp src/plugin/js/js_bindings.cpp src/plugin/htmltext.cpp src/cache/cache.cpp src/cache/cache_img.cpp src/reader/window.cpp
                         ${TP}/libzippp/libzippp.cpp src/http/export_cbz.cpp)
target_include_directories(
  cppIsekai
  PRIVATE src
          ${TP}/sol2/include
          ${TP}/thread_pool/include
          ${TP}/readerwriterqueue/include
          ${TP}/glaze
          ${TP}/simpleini
          ${TP}/sqlite_orm/include
          ${TP}/libzippp)
target_link_libraries(
  cppIsekai
  PRIVATE wasm3 quickjs PkgConfig::SQLITE3 PkgConfig::VIPS PkgConfig::LUA re2 lexbor
          ${TP}/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0
          spdlog::spdlog PkgConfig::LIBZIP)
target_compile_definitions(cppIsekai PRIVATE CPPISEKAI_FETCH_LIB_DIR="${CMAKE_SOURCE_DIR}/third_party/curl-impersonate/lib")
target_compile_options(cppIsekai PRIVATE -Wall -Wextra)
set_target_properties(cppIsekai PROPERTIES BUILD_RPATH
                                           "${TP}/curl-impersonate/lib")
_cppisekai_pch_vendor(cppIsekai)


# --- tests (doctest, system package) ---
# ponytail: single test binary; split per-module when the suite outgrows one
# file
add_executable(test_store tests/test_store.cpp src/store/store_core.cpp src/store/store_search.cpp src/store/store_api.cpp)
target_include_directories(test_store PRIVATE src ${TP}/sqlite_orm/include)
target_link_libraries(test_store PRIVATE PkgConfig::SQLITE3)
target_compile_options(test_store PRIVATE -Wall -Wextra)
enable_testing()
add_test(NAME store COMMAND test_store)

# plugin module: loader/sandbox/ABI/host tools; fixtures under tests/fixtures
# lexbor static lib (vendored source; the html/css/dom/core subtrees only)
file(GLOB_RECURSE LEXBOR_SOURCES ${TP}/lexbor/source/lexbor/*.c)
add_library(lexbor STATIC ${LEXBOR_SOURCES})
target_include_directories(lexbor PUBLIC ${TP}/lexbor/source)
target_compile_options(lexbor PRIVATE -w) # C vendor lib; do not gate on -Wall -Wextra
target_include_directories(cppIsekai PRIVATE ${TP}/glaze ${TP}/sol2/include ${TP}/lexbor/source)
add_executable(test_plugin tests/test_plugin.cpp src/plugin/plugin_core.cpp src/plugin/plugin_load.cpp src/plugin/plugin_call.cpp
                           src/plugin/lua/bridge.cpp src/plugin/lua/lua_runtime.cpp src/plugin/http_client.cpp src/plugin/wasm/wasm_rt.cpp src/plugin/wasm/wasm_load.cpp src/plugin/js/js_rt.cpp src/plugin/js/js_bindings.cpp src/plugin/htmltext.cpp src/store/store_core.cpp src/store/store_search.cpp src/store/store_api.cpp src/fetch/fetch.cpp)
target_include_directories(
  test_plugin
  PRIVATE src ${TP}/glaze ${TP}/sol2/include ${TP}/thread_pool/include
          ${TP}/lexbor/source
          ${TP}/sqlite_orm/include ${TP}/readerwriterqueue/include)
target_link_libraries(
  test_plugin
  PRIVATE wasm3 quickjs PkgConfig::SQLITE3 PkgConfig::LUA re2
          ${TP}/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0
          spdlog::spdlog lexbor)
target_compile_options(test_plugin PRIVATE -Wall -Wextra)
target_compile_definitions(
  test_plugin
  PRIVATE TEST_PLUGIN_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/plugins")
set_target_properties(test_plugin PROPERTIES BUILD_RPATH
                                             "${TP}/curl-impersonate/lib")
add_test(NAME plugin COMMAND test_plugin)
_cppisekai_pch(test_plugin)


# host-helper core + runtime parity (plugin-helper 2.1): htmltext core cases
# direct; lua/js/wasm parity through the real Host on migrated fixtures.
add_executable(test_htmltext tests/test_htmltext.cpp src/plugin/plugin_core.cpp src/plugin/plugin_load.cpp src/plugin/plugin_call.cpp
                             src/plugin/lua/bridge.cpp src/plugin/lua/lua_runtime.cpp src/plugin/http_client.cpp src/plugin/wasm/wasm_rt.cpp src/plugin/wasm/wasm_load.cpp src/plugin/js/js_rt.cpp src/plugin/js/js_bindings.cpp src/plugin/htmltext.cpp src/store/store_core.cpp src/store/store_search.cpp src/store/store_api.cpp src/fetch/fetch.cpp)
target_include_directories(test_htmltext PRIVATE src ${TP}/glaze ${TP}/sol2/include ${TP}/thread_pool/include ${TP}/lexbor/source
                          ${TP}/sqlite_orm/include ${TP}/readerwriterqueue/include)
target_link_libraries(
  test_htmltext
  PRIVATE wasm3 quickjs PkgConfig::SQLITE3 PkgConfig::LUA re2 lexbor
          ${TP}/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0
          spdlog::spdlog)
target_compile_options(test_htmltext PRIVATE -Wall -Wextra)
target_compile_definitions(
  test_htmltext
  PRIVATE TEST_PLUGIN_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/plugins")
set_target_properties(test_htmltext PROPERTIES BUILD_RPATH
                                               "${TP}/curl-impersonate/lib")
add_test(NAME htmltext COMMAND test_htmltext)
_cppisekai_pch(test_htmltext)


# cache core: disk LRU store; fetcher injected -> network-free tests
add_executable(test_cache tests/test_cache.cpp src/cache/cache.cpp src/cache/cache_img.cpp)
target_include_directories(test_cache PRIVATE src)
target_link_libraries(test_cache PRIVATE spdlog::spdlog PkgConfig::VIPS)
target_compile_options(test_cache PRIVATE -Wall -Wextra)
add_test(NAME cache COMMAND test_cache)
_cppisekai_pch(test_cache)


# transcode core (cache-layout-v3 1.2): header-only vips buffer in/out
add_executable(test_transcode tests/test_transcode.cpp)
target_include_directories(test_transcode PRIVATE src)
target_link_libraries(test_transcode PRIVATE PkgConfig::VIPS)
target_compile_options(test_transcode PRIVATE -Wall -Wextra)
add_test(NAME transcode COMMAND test_transcode)

# slug pragma (1.1): header-only title->folder rule + collision suffix; no src sources
add_executable(test_slug tests/test_slug.cpp)
target_include_directories(test_slug PRIVATE src)
target_compile_options(test_slug PRIVATE -Wall -Wextra)
add_test(NAME slug COMMAND test_slug)

# config core (1.1): header-only registry + layered resolve; CLI11 parse, no env leakage
add_executable(test_config tests/test_config.cpp)
target_include_directories(test_config PRIVATE src ${TP}/simpleini ${TP}/glaze)
target_compile_options(test_config PRIVATE -Wall -Wextra)
add_test(NAME config COMMAND test_config)

# /cache/img serve-through: pure route core, crow-free; fetcher injected via the
# cache ctor, so no server, curl or store linkage is needed here.
add_executable(test_http_img tests/test_http_img.cpp src/cache/cache.cpp src/cache/cache_img.cpp)
target_include_directories(test_http_img PRIVATE src)
target_link_libraries(test_http_img PRIVATE spdlog::spdlog PkgConfig::VIPS)
target_compile_options(test_http_img PRIVATE -Wall -Wextra)
add_test(NAME http_img COMMAND test_http_img)
_cppisekai_pch(test_http_img)


# CBZ export core: in-memory zip of the cached tree, crow-free
add_executable(test_export tests/test_export.cpp src/http/export_cbz.cpp ${TP}/libzippp/libzippp.cpp)
target_include_directories(test_export PRIVATE src/http ${TP}/libzippp)
target_link_libraries(test_export PRIVATE PkgConfig::LIBZIP)
target_compile_options(test_export PRIVATE -Wall -Wextra)
add_test(NAME export COMMAND test_export)

# stale layer (2.2): wrapper around the ABI call; cache + glaze, no server
add_executable(test_stale tests/test_stale.cpp src/store/store_core.cpp src/store/store_search.cpp src/store/store_api.cpp)
target_include_directories(test_stale PRIVATE src ${TP}/glaze ${TP}/sqlite_orm/include)
target_link_libraries(test_stale PRIVATE spdlog::spdlog PkgConfig::VIPS PkgConfig::SQLITE3)
target_compile_options(test_stale PRIVATE -Wall -Wextra)
add_test(NAME stale COMMAND test_stale)
_cppisekai_pch(test_stale)


# pages api core (reader-page 1.1): pure builder, envelopes faked; crow
# confined to the TU under test
add_executable(test_pages tests/test_pages.cpp src/http/pages_api.cpp)
target_include_directories(test_pages PRIVATE src ${TP}/glaze)
target_link_libraries(test_pages PRIVATE spdlog::spdlog) # stale.hpp logs now
target_compile_options(test_pages PRIVATE -Wall -Wextra)
add_test(NAME pages COMMAND test_pages)

# position merge (reader-page 2.2): absent fields keep stored; crow-free
add_executable(test_position tests/test_position.cpp)
target_include_directories(test_position PRIVATE src)
target_compile_options(test_position PRIVATE -Wall -Wextra)
add_test(NAME position COMMAND test_position)

# reader window math: pure fn, fakes injected; no server, store or network
add_executable(test_window tests/test_window.cpp src/reader/window.cpp)
target_include_directories(test_window PRIVATE src)
target_compile_options(test_window PRIVATE -Wall -Wextra)
add_test(NAME window COMMAND test_window)

# worker (3.3): delta prefetch on an injected cache; no server, no crow
add_executable(test_worker tests/test_worker.cpp src/cache/cache.cpp src/cache/cache_img.cpp)
target_include_directories(test_worker PRIVATE src)
target_link_libraries(test_worker PRIVATE spdlog::spdlog PkgConfig::VIPS)
target_compile_options(test_worker PRIVATE -Wall -Wextra)
add_test(NAME worker COMMAND test_worker)
_cppisekai_pch(test_worker)


# ponytail: no CPack/install yet -- add when packaging matters

find_program(CLANG_TIDY clang-tidy)
find_program(CLANG_FORMAT clang-format)
# optional targets: cmake --build build --target tidy / format
add_custom_target(
  tidy
  COMMAND ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR} src/main.cpp src/store/store_core.cpp src/store/store_search.cpp src/store/store_api.cpp
          src/http/http.cpp src/plugin/plugin_core.cpp src/plugin/plugin_load.cpp src/plugin/plugin_call.cpp src/cache/cache.cpp src/cache/cache_img.cpp src/reader/window.cpp
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(
  format
  COMMAND ${CLANG_FORMAT} -i src/*.cpp src/*/*.cpp src/main.cpp src/*/*.hpp
          tests/*.cpp
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(
    fix
    COMMAND ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR} -fix -fix-errors src/main.cpp src/store/store_core.cpp src/store/store_search.cpp src/store/store_api.cpp
            src/http/http.cpp src/plugin/plugin_core.cpp src/plugin/plugin_load.cpp src/plugin/plugin_call.cpp src/cache/cache.cpp src/cache/cache_img.cpp src/reader/window.cpp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
