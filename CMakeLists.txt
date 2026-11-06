cmake_minimum_required(VERSION 3.20)
project(cppIsekai VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- system shared libs ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips)
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua5.4)
find_package(spdlog REQUIRED)  # system shared 1.17.0; links fmt 12.2.0

set(TP ${PROJECT_SOURCE_DIR}/third_party)

# --- vendored static: wasm3 (C) & libsql sqlite amalgamation ---
# ponytail: in-tree static amalgamation; switch to system-shared libsql once pacman/AUR libsql lands
file(GLOB WASM3_SRC CONFIGURE_DEPENDS ${TP}/wasm3/src/*.c)
add_library(wasm3 STATIC ${WASM3_SRC})
target_include_directories(wasm3 PUBLIC ${TP}/wasm3/src)

add_library(sqlite3 STATIC ${TP}/libsql/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${TP}/libsql/sqlite)

# --- app ---
add_executable(cppIsekai
    src/main.cpp
    src/store.cpp
    src/http.cpp
)
target_include_directories(cppIsekai PRIVATE
    ${TP}/sol2/include
    ${TP}/thread_pool/include
    ${TP}/readerwriterqueue/include
    ${TP}/glaze
    ${TP}/simpleini
    ${TP}/sqlite_orm/include
)
target_link_libraries(cppIsekai PRIVATE
    wasm3
    sqlite3
    PkgConfig::VIPS
    PkgConfig::LUA
    ${TP}/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0
    spdlog::spdlog
)
target_compile_options(cppIsekai PRIVATE -Wall -Wextra)
set_target_properties(cppIsekai PROPERTIES
    BUILD_RPATH "${TP}/curl-impersonate/lib"
)

# --- tests (doctest, system package) ---
# ponytail: single test binary; split per-module when the suite outgrows one file
add_executable(test_store tests/test_store.cpp src/store.cpp)
target_include_directories(test_store PRIVATE src ${TP}/sqlite_orm/include)
target_link_libraries(test_store PRIVATE sqlite3)
target_compile_options(test_store PRIVATE -Wall -Wextra)
enable_testing()
add_test(NAME store COMMAND test_store)

# ponytail: no CPack/install yet -- add when packaging matters

find_program(CLANG_TIDY clang-tidy)
find_program(CLANG_FORMAT clang-format)
# optional targets: cmake --build build --target tidy / format
add_custom_target(tidy
    COMMAND ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR} src/main.cpp src/store.cpp src/http.cpp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_custom_target(format
    COMMAND ${CLANG_FORMAT} -i src/*.cpp src/*.hpp tests/*.cpp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
