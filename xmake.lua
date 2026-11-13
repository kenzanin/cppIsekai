set_project("cppIsekai")
set_version("0.1.0")

set_languages("c23", "c++23")
add_rules("mode.debug", "mode.release")
set_config("mode", "release") -- daemon runs from this build
set_policy("package.requires_lock", true) -- lock resolved versions (xmake-requires.lock); bump via the pins below

local TP = "third_party"
local CURL_SO = "$(projectdir)/" .. TP .. "/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0"
local CURL_LIB = "$(projectdir)/" .. TP .. "/curl-impersonate/lib"

-- shared source groups
local STORE_SRC = {"src/store/store_core.cpp", "src/store/store_search.cpp", "src/store/store_api.cpp",
                   "src/store/store_enrichment.cpp"}
local CACHE_SRC = {"src/cache/cache.cpp", "src/cache/cache_img.cpp"}
local PLUGIN_SRC = {"src/plugin/plugin_core.cpp", "src/plugin/plugin_load.cpp", "src/plugin/plugin_call.cpp",
                    "src/plugin/lua/bridge.cpp", "src/plugin/lua/lua_runtime.cpp", "src/plugin/http_client.cpp",
                    "src/plugin/wasm/wasm_rt.cpp", "src/plugin/wasm/wasm_load.cpp", "src/plugin/wasm/wasm_imports.cpp",
                    "src/plugin/js/js_rt.cpp", "src/plugin/js/js_bindings.cpp", "src/plugin/htmltext_re2.cpp",
                    "src/plugin/htmltext_html.cpp"}
local FETCH_SRC = {"src/fetch/fetch.cpp", "src/fetch/fetch_curl.cpp"}

-- system shared libs via pkg-config; the rest pinned from xrepo (locked in xmake-requires.lock)
add_requires("pkgconfig::vips", {system = true})
add_requires("pkgconfig::lua54", {system = true})

-- xrepo pins; formerly vendored/system. wasm3 + curl-impersonate remain vendored.
-- sol2 is header-only: tell its recipe not to pull a Lua dep (Arch's system lua54 covers runtime + headers),
-- because xmake resolves bare "lua" to Arch's pacman lua 5.5 which sol2 3.5.0 rejects.
add_requires("sol2 3.5.0", {configs = {includes_lua = false}})
add_requires("glaze 7.9.0", "thread-pool 5.1.0", "readerwriterqueue 1.0.7", "lexbor 3.0.0",
    "libzippp 7.1", "quickjs-ng 0.15.1", "simpleini 4.27", "sqlite_orm 1.9.1",
    "spdlog 1.17.0", "zlib 1.3.2", "sqlite3 3.54.0+100", "utf8proc 2.11.3", "re2 2025.11.05",
    "date 3.0.4", "hash-library 2021.09.29", "pugixml 1.16")

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
        if cfg.reqs then add_packages(table.unpack(cfg.reqs)) end
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

-- Pinned tiny-AES source (same upstream commit as the xmake package on the
-- add_requires line) rebuilt with AES256: the prebuilt package lib is 128-only
-- and host.crypto needs AES-256-GCM. Single-keylen library, hence the define.
target("tiny-aes256")
    set_kind("static")
    set_languages("c23")
    add_files(TP .. "/tiny-aes/aes.c")
    add_includedirs(TP .. "/tiny-aes", {public = true})
    add_defines("AES256=1")

-- --- daemon ---
cxx_bin("cppIsekai", {
    files = {"src/**.cpp"},
    incs = {"src"},
    deps = {"wasm3", "tiny-aes256"},
    pkgs = {"vips", "lua54"},
    reqs = {"glaze", "sol2", "thread-pool", "readerwriterqueue", "lexbor", "libzippp", "quickjs-ng", "simpleini", "sqlite_orm", "spdlog", "zlib", "sqlite3", "utf8proc", "re2", "date", "hash-library", "pugixml"},
    ldflags = {CURL_SO},
    rpath = {CURL_LIB},
    defs = {'CPPISEKAI_FETCH_LIB_DIR="' .. CURL_LIB .. '"', "CROW_USE_LOCALTIMEZONE", "CROW_ENABLE_COMPRESSION"},
})

-- --- tests (doctest, system package; `xmake test` runs all) ---
cxx_bin("test_store", {
    files = {"tests/test_store.cpp", STORE_SRC},
    incs = {"src"},
    reqs = {"glaze", "sqlite_orm", "spdlog", "sqlite3", "utf8proc"},
    test = true,
})

cxx_bin("test_plugin", {
    files = {"tests/test_plugin.cpp", PLUGIN_SRC, STORE_SRC, FETCH_SRC},
    incs = {"src"},
    deps = {"wasm3", "tiny-aes256"},

    pkgs = {"lua54"},
    reqs = {"glaze", "sol2", "thread-pool", "lexbor", "sqlite_orm", "readerwriterqueue", "quickjs-ng", "spdlog", "sqlite3", "utf8proc", "re2", "date", "hash-library", "pugixml"},
    ldflags = {CURL_SO},
    rpath = {CURL_LIB},
    defs = {'TEST_PLUGIN_FIXTURES="$(projectdir)/tests/fixtures/plugins"'},
    test = true,
})

cxx_bin("test_htmltext", {
    files = {"tests/test_htmltext.cpp", PLUGIN_SRC, STORE_SRC, FETCH_SRC},
    incs = {"src"},
    deps = {"wasm3", "tiny-aes256"},

    pkgs = {"lua54"},
    reqs = {"glaze", "sol2", "thread-pool", "lexbor", "sqlite_orm", "readerwriterqueue", "quickjs-ng", "spdlog", "sqlite3", "utf8proc", "re2", "date", "hash-library", "pugixml"},
    ldflags = {CURL_SO},
    rpath = {CURL_LIB},
    defs = {'TEST_PLUGIN_FIXTURES="$(projectdir)/tests/fixtures/plugins"'},
    test = true,
})

cxx_bin("test_cache", {
    files = {"tests/test_cache.cpp", CACHE_SRC},
    incs = {"src"},
    pkgs = {"vips"},
    reqs = {"spdlog"},
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
    incs = {"src"},
    reqs = {"simpleini", "glaze"},
    test = true,
})

cxx_bin("test_http_img", {
    files = {"tests/test_http_img.cpp", CACHE_SRC},
    incs = {"src"},
    pkgs = {"vips"},
    reqs = {"spdlog"},
    test = true,
})

cxx_bin("test_export", {
    files = {"tests/test_export.cpp", "src/http/export_cbz.cpp"},
    incs = {"src/http"},
    reqs = {"libzippp"},
    test = true,
})

cxx_bin("test_stale", {
    files = {"tests/test_stale.cpp", STORE_SRC},
    incs = {"src"},
    pkgs = {"vips"},
    reqs = {"glaze", "sqlite_orm", "spdlog", "sqlite3", "utf8proc"},
    test = true,
})

cxx_bin("test_http_enrich", {
    files = {"tests/test_http_enrich.cpp", STORE_SRC},
    incs = {"src"},
    pkgs = {"vips"},
    reqs = {"glaze", "sqlite_orm", "thread-pool", "simpleini", "readerwriterqueue", "spdlog", "sqlite3", "utf8proc"},
    test = true,
})

cxx_bin("test_pages", {
    files = {"tests/test_pages.cpp", "src/http/pages_api.cpp"},
    incs = {"src"},
    reqs = {"glaze", "spdlog"},
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
    pkgs = {"vips"},
    reqs = {"spdlog"},
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
    set_menu {usage = "xmake tidy", description = "clang-tidy report on prod sources changed vs HEAD"}
    on_run(function()
        os.exec("xmake project -k compile_commands")
        -- ponytail: sweep only sources changed vs HEAD -- a full-tree crawl
        -- burns ~20min re-instantiating vendor templates (sol2/quickjs/wasm3)
        -- inside untouched TUs. Widen by hand for a one-off tree-wide pass.
        local files, seen = {}, {}
        local function collect(text)
            if not text then
                return
            end
            for f in text:gmatch("[^\r\n]+") do
                if f:match("%.cpp$") and not seen[f] then
                    seen[f] = true
                    table.insert(files, path.absolute(f))
                end
            end
        end
        collect(os.iorun("git diff --name-only HEAD -- src/"))
        collect(os.iorun("git ls-files -o --exclude-standard src/"))
        for _, f in ipairs(files) do
            os.execv("clang-tidy", {"-p", ".", f})
        end
    end)

task("fix")
    set_menu {usage = "xmake fix", description = "clang-tidy -fix on prod sources changed vs HEAD"}
    on_run(function()
        os.exec("xmake project -k compile_commands")
        -- ponytail: same changed-only scope as tidy (vendor template TUs
        -- dominate the crawl); widen by hand for a one-off tree-wide pass.
        local files, seen = {}, {}
        local function collect(text)
            if not text then
                return
            end
            for f in text:gmatch("[^\r\n]+") do
                if f:match("%.cpp$") and not seen[f] then
                    seen[f] = true
                    table.insert(files, path.absolute(f))
                end
            end
        end
        collect(os.iorun("git diff --name-only HEAD -- src/"))
        collect(os.iorun("git ls-files -o --exclude-standard src/"))
        for _, f in ipairs(files) do
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
