set_project("cppIsekai")
set_version("0.1.0")

set_languages("c23", "c++23")
set_toolchains("clang")
add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_config("mode", "release") -- daemon runs from this build
set_policy("package.requires_lock", true) -- lock resolved versions (xmake-requires.lock); bump via the pins below

-- Opt-in sanitizers (never on by default). Debug modes only: the release binary
-- is the daemon, so instrumenting it in place would swap the running build for
-- an ASan one -- release is warn-and-skip.
--   xmake f -m debug --sanitize=address,undefined -y && xmake -y && xmake test
option("sanitize")
    set_default("none")
    set_values("none", "address", "address,undefined", "undefined", "thread")
    set_description("Instrument every target with -fsanitize=<value> (debug modes only)")
option_end()

local SAN = get_config("sanitize") or "none"
if SAN ~= "none" then
    if is_mode("release") then
        print("[sanitize] IGNORED in release mode (that binary is the daemon) -- use `xmake f -m debug --sanitize=%s`",
              SAN)
    else
        add_cxflags("-fsanitize=" .. SAN, "-fno-omit-frame-pointer", {force = true})
        add_cflags("-fsanitize=" .. SAN, "-fno-omit-frame-pointer", {force = true})
        add_ldflags("-fsanitize=" .. SAN, {force = true})
        add_defines("CPPISEKAI_SANITIZE=" .. SAN)
        print("[sanitize] %s (mode %s)", SAN, get_config("mode"))
    end
end

local TP = "third_party"
local CURL_SO = "$(projectdir)/" .. TP .. "/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0"
local CURL_LIB = "$(projectdir)/" .. TP .. "/curl-impersonate/lib"

-- shared source groups
local STORE_SRC = {"src/store/store_core.cpp", "src/store/store_search.cpp", "src/store/store_api.cpp",
                   "src/store/store_maintenance.cpp", "src/store/store_enrichment.cpp", "src/store/store_dedup.cpp",
                   "src/store/store_saved.cpp", "src/store/store_chapters.cpp"}
local CACHE_SRC = {"src/cache/cache.cpp", "src/cache/cache_img.cpp"}
local ENRICH_SRC = {"src/http/enrich_chosen.cpp", "src/http/enrich_detail.cpp"}
local PLUGIN_SRC = {"src/plugin/plugin_core.cpp", "src/plugin/plugin_load.cpp", "src/plugin/plugin_call.cpp",
                    "src/plugin/lua/bridge.cpp", "src/plugin/lua/lua_runtime.cpp", "src/plugin/http_client.cpp",
                    "src/plugin/wasm/wasm_rt.cpp", "src/plugin/wasm/wasm_load.cpp", "src/plugin/wasm/wasm_imports.cpp",
                    "src/plugin/js/js_rt.cpp", "src/plugin/js/js_bindings.cpp", "src/plugin/htmltext_re2.cpp",
                    "src/plugin/htmltext_html.cpp", "src/browser/chrome.cpp"}
local FETCH_SRC = {"src/fetch/fetch.cpp", "src/fetch/fetch_curl.cpp"}

-- system shared libs via pkg-config; the rest pinned from xrepo (locked in xmake-requires.lock)
add_requires("pkgconfig::vips", {system = true})

-- xrepo pins; formerly vendored/system. wasm3 + curl-impersonate remain vendored.
-- sol2 is header-only: tell its recipe not to pull a Lua dep (we pin xrepo lua 5.4.8 ourselves),
-- because xmake resolves bare "lua" to xrepo's lua 5.5 which sol2 3.5.0's compat header rejects
-- (LUA_ERRGCMM undeclared; 5.1-5.4 only).
add_requires("sol2 3.5.0", {configs = {includes_lua = false}})
-- lua 5.4.8 (not 5.5.x): latest sol2 in xmake-repo is 3.5.0, incompatible with 5.5 otherwise.
add_requires("lua 5.4.8")
add_requires("glaze 9.1.0", "thread-pool 5.1.0", "readerwriterqueue 1.0.7", "lexbor 3.0.0",
    "libzippp 7.1", "quickjs-ng 0.15.1", "simpleini 4.27", "sqlite_orm 1.9.1",
    "spdlog 1.17.0", "zlib 1.3.2", "sqlite3 3.54.0+100", "utf8proc 2.11.3", "re2 2025.11.05",
    "hash-library 2021.09.29", "pugixml 1.16", "backward-cpp")

-- crow + cli11: both header-only (static into the binary), pinned via xrepo.
-- crow's SSL build is skipped: the daemon serves plain HTTP.
-- asio is pulled by crow's recipe with no version constraint of its own.
add_requires("asio 1.36.0", "crow 1.3.5", {configs = {ssl = false}})
add_requires("cli11 2.7.2", {configs = {cmake = false}})

-- reproc++ retired with the lightpanda backend; the browser is app-local CDP
-- via vendored cdp-cpp (third_party/cdp-cpp, MIT) driving headless Chrome.

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
        t:add("ldflags", "-rdynamic", {force = true}) -- dynsym utk backward dladdr; jalur def-time add_ldflags menghilangkannya
            end
            -- pkg-config injection for vendored sources needing system GUI libs
            -- (traypp): xmake's pkgconfig:: package cache proved flaky here, the
            -- raw tool is not.
            if cfg.pkgconfig then
                local pc = find_program("pkg-config")
                if pc then
                    local names = table.concat(cfg.pkgconfig, " ")
                    local cf = os.iorun(pc .. " --cflags " .. names)
                    local lf = os.iorun(pc .. " --libs " .. names)
                    if cf and #cf > 0 then
                        local flags = {} -- split shlex-ish on spaces
                        for f in cf:gmatch("%S+") do flags[#flags + 1] = f end
                        t:add("cflags", flags, {force = true})
                        t:add("cxxflags", flags, {force = true})
                    end
                    if lf and #lf > 0 then
                        local libs = {}
                        for f in lf:gmatch("%S+") do libs[#libs + 1] = f end
                        t:add("ldflags", libs, {force = true})
                    end
                end
            end
        end)
    target_end()
end

-- --- vendored static libs (C) ---
-- traypp (vendored Soundux/traypp) compiles straight into the daemon target
-- below: a separate static target does not propagate its pkg-config cflags to
-- its own sources reliably, and the daemon needs the gtk link flags anyway.

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

-- Vendored CDP-for-CPP (MIT): the Chrome DevTools client behind the browser
-- backend. Upstream ships 30 warnings; compiled as-is, outside our gate
-- (same policy as every third_party target).
target("cdp-cpp")
    set_kind("static")
    set_languages("c++23")
    -- upstream's own source list (CMakeLists) + the posix platform pair;
    -- domains/*.cpp and the win32 pair are not part of the shipped lib.
    add_files(TP .. "/cdp-cpp/src/core/Json.cpp",
        TP .. "/cdp-cpp/src/core/Base64.cpp",
        TP .. "/cdp-cpp/src/core/SHA1.cpp",
        TP .. "/cdp-cpp/src/net/HttpClient.cpp",
        TP .. "/cdp-cpp/src/net/WebSocket.cpp",
        TP .. "/cdp-cpp/src/protocol/CDPConnection.cpp",
        TP .. "/cdp-cpp/src/protocol/CDPClient.cpp",
        TP .. "/cdp-cpp/src/browser/QuickStart.cpp",
        TP .. "/cdp-cpp/src/browser/ChromeLauncher_posix.cpp",
        TP .. "/cdp-cpp/src/highlevel/Page.cpp",
        TP .. "/cdp-cpp/src/highlevel/ElementHandle.cpp",
        TP .. "/cdp-cpp/src/highlevel/NetworkInterceptor.cpp",
        TP .. "/cdp-cpp/src/highlevel/Browser.cpp",
        TP .. "/cdp-cpp/src/net/Socket_posix.cpp")
    add_includedirs(TP .. "/cdp-cpp/include", {public = true})
    add_syslinks("pthread")

-- --- daemon ---
cxx_bin("cppIsekai", {
    files = {"src/**.cpp", TP .. "/traypp/tray/src/core/*.cpp", TP .. "/traypp/tray/src/core/linux/*.cpp", TP .. "/traypp/tray/src/components/*.cpp"},
    incs = {"src", TP .. "/traypp/tray/include"},
    deps = {"wasm3", "tiny-aes256", "cdp-cpp"},
    pkgs = {"vips"},
    pkgconfig = {"appindicator3-0.1", "gtk+-3.0"}, -- traypp gtk/appindicator (raw pkg-config; see on_load)
    reqs = {"lua", "crow", "cli11", "asio", "glaze", "sol2", "thread-pool", "readerwriterqueue", "lexbor", "libzippp", "quickjs-ng", "simpleini", "sqlite_orm", "spdlog", "zlib", "sqlite3", "utf8proc", "re2", "hash-library", "pugixml", "backward-cpp"},
    ldflags = {CURL_SO},
    rpath = {CURL_LIB},
    defs = {'CPPISEKAI_FETCH_LIB_DIR="' .. CURL_LIB .. '"', "CROW_USE_LOCALTIMEZONE", "CROW_ENABLE_COMPRESSION"},
})

-- --- tests (doctest, system package; `xmake test` runs all) ---
cxx_bin("test_fetch", {
    files = {"tests/test_fetch.cpp", FETCH_SRC, "src/browser/chrome.cpp"},
    incs = {"src"},
    deps = {"cdp-cpp"},
    reqs = {"glaze", "spdlog"},
    ldflags = {CURL_SO},
    rpath = {CURL_LIB},
    test = true,
})

cxx_bin("test_store", {
    files = {"tests/test_store.cpp", STORE_SRC},
    incs = {"src"},
    reqs = {"glaze", "sqlite_orm", "spdlog", "sqlite3", "utf8proc"},
    test = true,
})

cxx_bin("test_plugin", {
    files = {"tests/test_plugin.cpp", PLUGIN_SRC, STORE_SRC, FETCH_SRC},
    incs = {"src"},
    deps = {"wasm3", "tiny-aes256", "cdp-cpp"},

    reqs = {"lua", "glaze", "sol2", "thread-pool", "lexbor", "sqlite_orm", "readerwriterqueue", "quickjs-ng", "spdlog", "sqlite3", "utf8proc", "re2", "hash-library", "pugixml"},
    ldflags = {CURL_SO},
    rpath = {CURL_LIB},
    defs = {'TEST_PLUGIN_FIXTURES="$(projectdir)/tests/fixtures/plugins"'},
    test = true,
})

cxx_bin("test_htmltext", {
    files = {"tests/test_htmltext.cpp", PLUGIN_SRC, STORE_SRC, FETCH_SRC},
    incs = {"src"},
    deps = {"wasm3", "tiny-aes256", "cdp-cpp"},

    reqs = {"lua", "glaze", "sol2", "thread-pool", "lexbor", "sqlite_orm", "readerwriterqueue", "quickjs-ng", "spdlog", "sqlite3", "utf8proc", "re2", "hash-library", "pugixml"},
    ldflags = {CURL_SO},
    rpath = {CURL_LIB},
    defs = {'TEST_PLUGIN_FIXTURES="$(projectdir)/tests/fixtures/plugins"'},
    test = true,
})

cxx_bin("test_cache", {
    files = {"tests/test_cache.cpp", CACHE_SRC},
    incs = {"src"},
    pkgs = {"vips"},
    reqs = {"spdlog", "hash-library"},
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
    reqs = {"hash-library"},
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
    reqs = {"cli11", "simpleini", "glaze"},
    test = true,
})

cxx_bin("test_http_img", {
    files = {"tests/test_http_img.cpp", CACHE_SRC},
    incs = {"src"},
    pkgs = {"vips"},
    reqs = {"spdlog", "hash-library"},
    test = true,
})

cxx_bin("test_export", {
    files = {"tests/test_export.cpp", "src/http/export_cbz.cpp"},
    incs = {"src", "src/http"},
    reqs = {"libzippp", "hash-library"},
    test = true,
})

cxx_bin("test_stale", {
    files = {"tests/test_stale.cpp", STORE_SRC},
    incs = {"src"},
    pkgs = {"vips"},
    reqs = {"glaze", "sqlite_orm", "spdlog", "sqlite3", "utf8proc", "hash-library", "thread-pool"},
    test = true,
})

cxx_bin("test_http_enrich", {
    files = {"tests/test_http_enrich.cpp", STORE_SRC, ENRICH_SRC},
    incs = {"src"},
    pkgs = {"vips"},
    reqs = {"crow", "glaze", "sqlite_orm", "thread-pool", "simpleini", "readerwriterqueue", "spdlog", "sqlite3", "utf8proc", "hash-library"},
    test = true,
})

cxx_bin("test_pages", {
    files = {"tests/test_pages.cpp", "src/http/pages_api.cpp"},
    incs = {"src"},
    reqs = {"crow", "glaze", "spdlog", "hash-library", "thread-pool"},
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
    reqs = {"spdlog", "hash-library"},
    test = true,
})

cxx_bin("test_saved_stats", {
    files = {"tests/test_saved_stats.cpp", STORE_SRC},
    incs = {"src"},
    pkgs = {"vips"},
    reqs = {"glaze", "sqlite_orm", "thread-pool", "simpleini", "readerwriterqueue", "spdlog", "sqlite3", "utf8proc", "hash-library"},
    test = true,
})

cxx_bin("test_templates", {
    files = {"tests/test_templates.cpp"},
    incs = {"src"},
    defs = {'CPPISEKAI_TEMPLATES_DIR="$(projectdir)/templates"',
            'CPPISEKAI_GOLDEN_DIR="$(projectdir)/tests/fixtures/golden"'},
    reqs = {"crow"},
    test = true,
})

cxx_bin("test_gridjs", {
    files = {"tests/test_gridjs.cpp"},
    defs = {'CPPISEKAI_GRID_SRC="$(projectdir)/templates/grid_body.mustache"'},
    reqs = {"quickjs-ng"},
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

-- Full sanitizer sweep, meant for the release gate (slow: two full rebuilds).
-- Leak/UB first, races second -- the two cannot share one binary -- then the
-- daemon build is restored no matter how the sweep ends.
task("scan")
    set_menu {usage = "xmake scan", description = "release gate: ASan+UBSan suite, TSan suite, restore release"}
    on_run(function()
        -- task sandbox has no global pcall; xmake's try/catch is the idiom here
        local function run(cmd)
            print("[scan] " .. cmd)
            local r = try {
                function ()
                    os.iorun(cmd)
                    return true
                end,
                catch {
                    function (errors)
                        print("[scan] FAILED: " .. cmd)
                        print(errors)
                        return false
                    end
                }
            }
            return r == true
        end
        local ok = true
        -- address+undefined: leaks, lifetime, integer UB, out-of-bounds. ASan
        -- reports are exit-code failures, so a red suite here IS a finding.
        ok = run("xmake f -m debug --sanitize=address,undefined -y") and ok
        ok = run("xmake -y") and ok
        ok = run("xmake test") and ok
        -- thread: data races in the store/plugin worker pools. Keep going even
        -- when the leak pass failed -- one sweep should surface both.
        ok = run("xmake f -m debug --sanitize=thread -y") and ok
        ok = run("xmake -y") and ok
        -- os.execv cannot take a VAR=value prefix (it is parsed as the program
        -- name), so the TSan knobs go through the environment instead. The
        -- suppression list mutes uninstrumented system libs (libvips/glib spawn
        -- their own worker threads); reports inside src/ are still fatal.
        os.setenv("TSAN_OPTIONS",
            "halt_on_error=0 second_deadlock_stack=1 history_size=7 suppressions=" ..
            path.join(os.scriptdir(), "scripts", "tsan.supp"))
        ok = run("xmake test") and ok
        os.setenv("TSAN_OPTIONS", "")
        run("xmake f -m release --sanitize=none -y")
        run("xmake -y")
        if not ok then
            raise("[scan] sanitizer findings above (release build restored)")
        end
        print("[scan] clean: address+undefined and thread suites green; release restored")
    end)

-- --- docs: clang-doc HTML for src/ only --------------------------------------
-- Two quirks make this usable instead of a 15-minute, ~20k-page dump:
--   1. clang-doc cannot merge its own JSON intermediates here (LLVM 23 answers
--      `Reducing 0 infos` when handed a .json input), so the whole doc set must
--      come out of ONE process that parses every TU -- per-TU parallel runs
--      cannot be stitched back together afterwards.
--   2. xmake passes package includes as -I, so clang-doc treats sqlite_orm,
--      crow, asio, boost, std, sol... as project code (1816 pages for a single
--      TU). Rewriting every out-of-project include to -isystem drops them (26
--      pages for the same TU) because clang-doc skips system headers.
task("doc")
    set_menu {usage = "XMAKE_DOC_FORMAT=md xmake doc", description = "clang-doc docs for src/ into docs/api (html default, md opt-in)"}
    on_run(function ()
        import("core.base.json")
        local root = os.projectdir()
        os.exec("xmake project -k compile_commands")
        local db = json.loadfile(path.join(root, "compile_commands.json"))
        local function external(p)
            -- out-of-project include dirs, plus third_party/: vendored cdp-cpp,
            -- traypp and wasm3 have their own targets and would otherwise land
            -- in the docs as project namespaces (cdp alone is 1.3 MB of
            -- generated markdown next to ~500 KB of our own).
            return (p:startswith("/") and not p:startswith(root))
                or p:find("third_party/", 1, true) ~= nil
        end
        -- one entry per TU (release/debug duplicates), src/ only
        local seen = {}
        local entries = {}
        for _, e in ipairs(db) do
            if e.file:startswith("src/") and not seen[e.file] then
                seen[e.file] = true
                table.insert(entries, e)
            end
        end
        if #entries == 0 then
            raise("[doc] no src/ entries in compile_commands.json")
        end
        local files = {}
        for _, e in ipairs(entries) do
            local args = {}
            local i = 1
            while i <= #e.arguments do
                local t = e.arguments[i]
                if t == "-I" and e.arguments[i + 1] then
                    local p = e.arguments[i + 1]
                    table.insert(args, external(p) and "-isystem" or "-I")
                    table.insert(args, p)
                    i = i + 2
                elseif t:startswith("-I") and #t > 2 then
                    local p = t:sub(3)
                    table.insert(args, external(p) and "-isystem" or "-I")
                    table.insert(args, p)
                    i = i + 1
                else
                    table.insert(args, t)
                    i = i + 1
                end
            end
            e.arguments = args
            table.insert(files, e.file)
        end
        local cc = path.join(root, "build", "clangdoc")
        os.tryrm(cc)
        os.mkdir(cc)
        io.writefile(path.join(cc, "compile_commands.json"), json.encode(entries))
        local fmt = os.getenv("XMAKE_DOC_FORMAT") or "html"
        -- the distro llvm package installs only the html templates under
        -- share/clang-doc, so --format=md dies on "cannot open .../md/class-template.mustache".
        -- Provision a private clang-doc prefix with the md set vendored in
        -- scripts/clang-doc/md instead of requiring root to fix /usr/share.
        local exe = "clang-doc"
        local extra = nil
        if fmt == "md" then
            local tool = path.join(cc, "tool")
            local bin = path.join(tool, "bin", "clang-doc")
            local share = path.join(tool, "share", "clang-doc")
            if not os.isfile(bin) then
                os.mkdir(path.join(tool, "bin"))
                os.mkdir(share)
                os.mkdir(path.join(share, "md"))
                os.cp(os.iorunv("sh", {"-c", "command -v clang-doc"}):trim(), bin)
                for _, f in ipairs(os.files("/usr/share/clang-doc/*")) do
                    os.cp(f, share, {force = true})
                end
                for _, f in ipairs(os.files(path.join(root, "scripts", "clang-doc", "md", "*.mustache"))) do
                    os.cp(f, path.join(share, "md"), {force = true})
                end
            end
            exe = bin
            -- a copied binary loses its builtin include dir (it resolves
            -- ../lib/clang/<ver> relative to argv[0]), so point it back.
            local rd = try { function () return os.iorunv("clang", {"-print-resource-dir"}):trim() end }
            if rd and #rd > 0 then
                extra = "-resource-dir=" .. rd
            end
        end
        local outdir = path.join(root, "docs", "api")
        os.tryrm(outdir)
        os.mkdir(outdir)
        local argv = {"-p", cc, "--format=" .. fmt, "--public",
                      "--project-name=cppIsekai", "--output=" .. outdir}
        if extra then
            table.insert(argv, "--extra-arg=" .. extra)
        end
        for _, f in ipairs(files) do
            table.insert(argv, f)
        end
        print("[doc] clang-doc (%s): %d TU(s) -> %s", fmt, #files, outdir)
        os.execv(exe, argv)
        local n = 0
        for _, _ in ipairs(os.files(path.join(outdir, "**", "*.*"))) do
            n = n + 1
        end
        print("[doc] done: %d page(s)", n)
    end)
