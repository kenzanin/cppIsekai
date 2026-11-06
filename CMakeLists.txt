cmake_minimum_required(VERSION 3.20)
project(cppIsekai VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- system shared libs ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips)
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua5.4)

set(TP ${PROJECT_SOURCE_DIR}/third_party)

# --- vendored static: wasm3 (C) & libsql sqlite amalgamation ---
# ponytail: in-tree static amalgamation; switch to system-shared libsql once pacman/AUR libsql lands
file(GLOB WASM3_SRC CONFIGURE_DEPENDS ${TP}/wasm3/src/*.c)
add_library(wasm3 STATIC ${WASM3_SRC})
target_include_directories(wasm3 PUBLIC ${TP}/wasm3/src)

add_library(sqlite3 STATIC ${TP}/libsql/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${TP}/libsql/sqlite)

# --- app ---
add_executable(cppIsekai src/main.cpp)
target_include_directories(cppIsekai PRIVATE
    ${TP}/sol2/include
    ${TP}/thread_pool/include
    ${TP}/readerwriterqueue/include
    ${TP}/glaze
)
target_link_libraries(cppIsekai PRIVATE
    wasm3
    sqlite3
    PkgConfig::VIPS
    PkgConfig::LUA
    ${TP}/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0
)
target_compile_options(cppIsekai PRIVATE -Wall -Wextra)
set_target_properties(cppIsekai PROPERTIES
    BUILD_RPATH "${TP}/curl-impersonate/lib"
)

# ponytail: no tests/CPack/install yet -- add when the first module needs them
