set_project("cppIsekai")
set_version("0.1.0")

set_languages("c23", "c++23")
add_rules("mode.debug", "mode.release")
set_config("mode", "release") -- daemon runs from this build; cmake parity is unoptimized

local TP = "third_party"
local CURL_SO = "$(projectdir)/" .. TP .. "/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0"
local CURL_LIB = "$(projectdir)/" .. TP .. "/curl-impersonate/lib"

-- shared source groups (mirror CMakeLists)
local STORE_SRC = {"src/store/store_core.cpp", "src/store/store_search.cpp", "src/store/store_api.cpp",
                   "src/store/store_enrichment.cpp"}
local CACHE_SRC = {"src/cache/cache.cpp", "src/cache/cache_img.cpp"}
local PLUGIN_SRC = {"src/plugin/plugin_core.cpp", "src/plugin/plugin_load.cpp", "src/plugin/plugin_call.cpp",
                    "src/plugin/lua/bridge.cpp", "src/plugin/lua/lua_runtime.cpp", "src/plugin/http_client.cpp",
                    "src/plugin/wasm/wasm_rt.cpp", "src/plugin/wasm/wasm_load.cpp", "src/plugin/wasm/wasm_imports.cpp",
                    "src/plugin/js/js_rt.cpp", "src/plugin/js/js_bindings.cpp", "src/plugin/htmltext_re2.cpp",
                    "src/plugin/htmltext_html.cpp"}
local FETCH_SRC = {"src/fetch/fetch.cpp", "src/fetch/fetch_curl.cpp"}

-- system shared libs via pkg-config (pacman-installed, same as cmake's pkg_check_modules)
add_requires("pkgconfig::sqlite3", "pkgconfig::vips", "pkgconfig::lua5.4", "pkgconfig::re2", "pkgconfig::spdlog",
             "pkgconfig::fmt", "pkgconfig::libzip", "pkgconfig::zlib", {system = true})

-- add_files needs flat strings; cfg.files may embed the shared groups above
local function flatten(list)
    local out = {}
    for _, v in ipairs(list) do
        if type(v) == "table" then
            for _, w in ipairs(v) do
                table.insert(out, w)
            end
        else
            table.insert(out, v)
        end
    end
    return out
end

-- helper: one binary with -Wall -Wextra; cfg carries the cmake-mapped knobs
local function cxx_bin(name, cfg)
    target(name)
        set_kind("binary")
        set_warnings("all", "extra")
        add_files(flatten(cfg.files))
        if cfg.incs then add_includedirs(cfg.incs) end
        if cfg.pkgs then
            local pkgs = {}
            for _, p in ipairs(cfg.pkgs) do
                table.insert(pkgs, "pkgconfig::" .. p)
            end
            add_packages(table.unpack(pkgs))
        end
        if cfg.deps then add_deps(table.unpack(cfg.deps)) end
        if cfg.defs then add_defines(table.unpack(cfg.defs)) end
        if cfg.ldflags then add_ldflags(table.unpack(cfg.ldflags), {force = true}) end
        if cfg.rpath then add_rpathdirs(cfg.rpath) end
        if cfg.test then add_tests("default") end
        on_load(function(t) -- mold for fast links like the cmake build (import needs script scope)
            import("lib.detect.find_program")
            if find_program("mold") then
                t:add("ldflags", "-fuse-ld=mold", {force = true})
            end
        end)
    target_end()
end

-- --- vendored static libs (C) ---
target("wasm3")
    set_kind("static")
    set_languages("c23")
    add_files(TP .. "/wasm3/src/*.c")
    add_includedirs(TP .. "/wasm3/src", {public = true})
    add_defines("_GNU_SOURCE") -- strict c23 hides POSIX (strnlen); cmake built with gnu23

target("quickjs")
    set_kind("static")
    set_languages("c23")
    add_files(TP .. "/quickjs/*.c")
    add_includedirs(TP .. "/quickjs", {public = true})
    add_defines("_GNU_SOURCE")
    add_syslinks("m", "dl", "pthread")

target("lexbor")
    set_kind("static")
    set_languages("c23")
    add_files(TP .. "/lexbor/source/lexbor/**.c")
    add_includedirs(TP .. "/lexbor/source", {public = true})
    add_defines("_GNU_SOURCE") -- same strict-c23 POSIX guard as wasm3
    set_warnings("none") -- C vendor lib; do not gate on -Wall -Wextra

-- --- daemon ---
cxx_bin("cppIsekai", {
    files = {"src/**.cpp", TP .. "/libzippp/libzippp.cpp"},
    incs = {"src", TP .. "/sol2/include", TP .. "/thread_pool/include", TP .. "/readerwriterqueue/include",
            TP .. "/glaze", TP .. "/simpleini", TP .. "/sqlite_orm/include", TP .. "/libzippp", TP .. "/lexbor/source"},
    deps = {"wasm3", "quickjs", "lexbor"},
    pkgs = {"sqlite3", "vips", "lua5.4", "re2", "spdlog", "fmt", "libzip", "zlib"},
    ldflags = {CURL_SO},
    rpath = {CURL_LIB},
    defs = {'CPPISEKAI_FETCH_LIB_DIR="' .. CURL_LIB .. '"', "CROW_USE_LOCALTIMEZONE", "CROW_ENABLE_COMPRESSION"},
})

-- --- tests (doctest, system package; `xmake test` runs all) ---
cxx_bin("test_store", {
    files = {"tests/test_store.cpp", STORE_SRC},
    incs = {"src", TP .. "/sqlite_orm/include", TP .. "/glaze"},
    pkgs = {"spdlog", "fmt", "sqlite3"},
    test = true,
})

cxx_bin("test_plugin", {
    files = {"tests/test_plugin.cpp", PLUGIN_SRC, STORE_SRC, FETCH_SRC},
    incs = {"src", TP .. "/glaze", TP .. "/sol2/include", TP .. "/thread_pool/include", TP .. "/lexbor/source",
            TP .. "/sqlite_orm/include", TP .. "/readerwriterqueue/include"},
    deps = {"wasm3", "quickjs", "lexbor"},
    pkgs = {"sqlite3", "lua5.4", "re2", "spdlog", "fmt"},
    ldflags = {CURL_SO},
    rpath = {CURL_LIB},
    defs = {'TEST_PLUGIN_FIXTURES="$(projectdir)/tests/fixtures/plugins"'},
    test = true,
})

cxx_bin("test_htmltext", {
    files = {"tests/test_htmltext.cpp", PLUGIN_SRC, STORE_SRC, FETCH_SRC},
    incs = {"src", TP .. "/glaze", TP .. "/sol2/include", TP .. "/thread_pool/include", TP .. "/lexbor/source",
            TP .. "/sqlite_orm/include", TP .. "/readerwriterqueue/include"},
    deps = {"wasm3", "quickjs", "lexbor"},
    pkgs = {"sqlite3", "lua5.4", "re2", "spdlog", "fmt"},
    ldflags = {CURL_SO},
    rpath = {CURL_LIB},
    defs = {'TEST_PLUGIN_FIXTURES="$(projectdir)/tests/fixtures/plugins"'},
    test = true,
})

cxx_bin("test_cache", {
    files = {"tests/test_cache.cpp", CACHE_SRC},
    incs = {"src"},
    pkgs = {"spdlog", "fmt", "vips"},
    test = true,
})

cxx_bin("test_transcode", {
    files = {"tests/test_transcode.cpp"},
    incs = {"src"},
    pkgs = {"vips"},
    test = true,
})

cxx_bin("test_slug", {
    files = {"tests/test_slug.cpp"},
    incs = {"src"},
    test = true,
})

cxx_bin("test_vrf", {
    files = {"tests/test_vrf.cpp"},
    incs = {"src"},
    test = true,
})

cxx_bin("test_config", {
    files = {"tests/test_config.cpp"},
    incs = {"src", TP .. "/simpleini", TP .. "/glaze"},
    test = true,
})

cxx_bin("test_http_img", {
    files = {"tests/test_http_img.cpp", CACHE_SRC},
    incs = {"src"},
    pkgs = {"spdlog", "fmt", "vips"},
    test = true,
})

cxx_bin("test_export", {
    files = {"tests/test_export.cpp", "src/http/export_cbz.cpp", TP .. "/libzippp/libzippp.cpp"},
    incs = {"src/http", TP .. "/libzippp"},
    pkgs = {"libzip"},
    test = true,
})

cxx_bin("test_stale", {
    files = {"tests/test_stale.cpp", STORE_SRC},
    incs = {"src", TP .. "/glaze", TP .. "/sqlite_orm/include"},
    pkgs = {"spdlog", "fmt", "vips", "sqlite3"},
    test = true,
})

cxx_bin("test_http_enrich", {
    files = {"tests/test_http_enrich.cpp", STORE_SRC},
    incs = {"src", TP .. "/glaze", TP .. "/sqlite_orm/include", TP .. "/thread_pool/include", TP .. "/simpleini",
            TP .. "/readerwriterqueue/include"},
    pkgs = {"spdlog", "fmt", "vips", "sqlite3"},
    test = true,
})

cxx_bin("test_pages", {
    files = {"tests/test_pages.cpp", "src/http/pages_api.cpp"},
    incs = {"src", TP .. "/glaze"},
    pkgs = {"spdlog", "fmt"},
    test = true,
})

cxx_bin("test_position", {
    files = {"tests/test_position.cpp"},
    incs = {"src"},
    test = true,
})

cxx_bin("test_window", {
    files = {"tests/test_window.cpp", "src/reader/window.cpp"},
    incs = {"src"},
    test = true,
})

cxx_bin("test_worker", {
    files = {"tests/test_worker.cpp", CACHE_SRC},
    incs = {"src"},
    pkgs = {"spdlog", "fmt", "vips"},
    test = true,
})

cxx_bin("test_templates", {
    files = {"tests/test_templates.cpp"},
    incs = {"src"},
    defs = {'CPPISEKAI_TEMPLATES_DIR="$(projectdir)/templates"',
            'CPPISEKAI_GOLDEN_DIR="$(projectdir)/tests/fixtures/golden"'},
    test = true,
})

-- --- gate tasks: prod code only (src/**), tests/third_party untouched ---
-- tasks run in a fresh lua sandbox: no shared globals, no shell pipes (os.exec passes | to argv).
task("format")
    set_menu {usage = "xmake format", description = "clang-format -i prod sources (src/** only)"}
    on_run(function()
        local files = {}
        table.join2(files, os.files("src/**.cpp"))
        table.join2(files, os.files("src/**.hpp"))
        os.execv("clang-format", table.join({"-i"}, files))
    end)

task("tidy")
    set_menu {usage = "xmake tidy", description = "clang-tidy report on prod sources (src/** only)"}
    on_run(function()
        os.exec("xmake project -k compile_commands")
        for _, f in ipairs(os.files("src/**.cpp")) do
            os.execv("clang-tidy", {"-p", ".", f})
        end
    end)

task("fix")
    set_menu {usage = "xmake fix", description = "clang-tidy -fix -fix-errors on prod sources (src/** only)"}
    on_run(function()
        os.exec("xmake project -k compile_commands")
        for _, f in ipairs(os.files("src/**.cpp")) do
            os.execv("clang-tidy", {"-p", ".", "-fix", "-fix-errors", f})
        end
    end)

task("gate")
    set_menu {usage = "xmake gate", description = "done gate: format + fix + build + test"}
    on_run(function()
        os.exec("xmake format")
        os.exec("xmake fix")
        os.exec("xmake")
        os.exec("xmake test")
    end)
