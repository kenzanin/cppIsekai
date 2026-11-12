#include "plugin_internal.hpp"
#include "store/store.hpp"

#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

namespace plugin {

std::string err_json(std::string msg) {
    glz::generic ok;
    glz::generic err;
    glz::generic root;
    ok.data = false;
    err.data = std::move(msg);
    auto& obj = root.data.emplace<glz::generic::object_t>();
    obj.insert({"ok", std::move(ok)});
    obj.insert({"error", std::move(err)});
    return dump_json(root);
}

// The wasm env.http_fetch service: moved verbatim from the monolith so both
// load paths (wasm and js) share the same front as the lua host tools.
wasm::HttpFetch make_wasm_fetch(const std::string& id, const Config& conf, const LiveProfile& live_profile) {
    return [id, timeout_ms = conf.fetch_timeout_ms, live = &live_profile,
            proxy = conf.proxy](const std::string& url, const std::string& opts_json) -> std::optional<std::string> {
        long ms = timeout_ms;
        std::string live_default;
        if (auto p = live->load()) {
            live_default = *p;
        }
        std::string profile_name = live_default;
        std::string method = "GET";
        std::string body;
        std::string accept_header;
        std::string ua_header;
        bool use_cookies = false; // 2.2: opts.cookie => per-plugin jar (cookie_jar_path)
        std::vector<std::pair<std::string, std::string>> extra_headers;

        glz::generic o;
        if (auto ec = glz::read_json(o, opts_json); !ec) {
            if (const auto* obj = std::get_if<glz::generic::object_t>(&o.data)) {
                const auto num = [&](const char* key, long& dst) {
                    if (const auto* v = member(obj, key)) {
                        if (const auto* d = std::get_if<double>(&v->data)) {
                            dst = static_cast<long>(*d);
                        }
                    }
                };
                const auto str = [&](const char* key, std::string& dst) {
                    if (const auto* v = member(obj, key)) {
                        if (const auto* s = std::get_if<std::string>(&v->data)) {
                            dst = *s;
                        }
                    }
                };
                num("timeout_ms", ms);
                str("profile", profile_name);
                str("method", method);
                str("body", body);
                str("accept", accept_header);
                str("user_agent", ua_header);
                if (const auto* b = member(obj, "cookie")) {
                    if (const auto* bb = std::get_if<bool>(&b->data)) {
                        use_cookies = *bb;
                    }
                }
                if (const auto* h = member(obj, "headers")) {
                    if (const auto* ho = std::get_if<glz::generic::object_t>(&h->data)) {
                        for (const auto& [k, v] : *ho) {
                            if (const auto* s = std::get_if<std::string>(&v.data)) {
                                extra_headers.emplace_back(k, *s);
                            }
                        }
                    }
                }
            }
        }

        profile_name = checked_profile(std::move(profile_name), live_default);
        const std::string cookie_jar = use_cookies ? cookie_jar_path(id) : std::string{};
        const FetchResult fr =
            fetch_sync(url, ms, profile_name, method, body, accept_header, ua_header, extra_headers, proxy, cookie_jar);
        if (!fr.error.empty()) {
            spdlog::warn("[plugin:{}] fetch {} failed: {}", id, url, fr.error);
            return std::nullopt;
        }
        glz::generic out;
        auto& obj = out.data.emplace<glz::generic::object_t>();
        glz::generic status;
        status.data = static_cast<double>(fr.status);
        obj.insert({"status", std::move(status)});
        glz::generic body_v;
        body_v.data = std::move(fr.body);
        obj.insert({"body", std::move(body_v)});
        glz::generic headers;
        auto& hh = headers.data.emplace<glz::generic::object_t>();
        for (auto&& [k, v] : fr.headers) {
            glz::generic val;
            val.data = v;
            hh.insert({k, std::move(val)});
        }
        obj.insert({"headers", std::move(headers)});
        return dump_json(out);
    };
}

namespace {

/** \brief One scan of a plugin directory. Shared by the scraper scan
 *  (data/plugins, upserts into the Store registry /plugins shows) and the
 *  enrichment scan (data/enrichment, stays out of the registry). Keeps the
 *  two paths byte-identical except for directory, tagging and upsert. */
void scan_dir(const Config& conf, LiveProfile& live_profile, Store& store, const StateGet& sget, const StateSet& sset,
              const std::string& dir, bool is_enrichment, std::map<std::string, std::shared_ptr<Runtime>>& fresh) {
    std::error_code ec;
    if (!fs::exists(fs::path(dir), ec)) {
        spdlog::warn("{} dir '{}' missing; skipped", is_enrichment ? "enrichment" : "plugin", dir);
        return;
    }
    // Enrichment dir is scanned in the same order but after scrapers; duplicate
    // ids across the two dirs are rejected here as duplicates too (first wins).
    const char* kind = is_enrichment ? "enrichment plugin" : "plugin";

    for (const auto& item : fs::directory_iterator(fs::path(dir))) {
        if (!item.is_directory()) {
            continue;
        }
        const std::string folder = item.path().filename().string();

        std::ifstream in(item.path() / "main.json");
        if (!in) {
            spdlog::warn("{} {}: no main.json, skipped", kind, folder);
            continue;
        }
        std::stringstream buf;
        buf << in.rdbuf();
        const std::string text = buf.str();

        Manifest m;
        if (auto e = glz::read<glz::opts{.error_on_unknown_keys = false}>(m, text)) {
            spdlog::warn("{} {}: manifest rejected (missing/invalid fields)", kind, folder);
            continue;
        }

        // Duplicate ids: first in scan order wins.
        Runtime rt;
        rt.info.id = m.id.value_or(folder);
        if (fresh.contains(rt.info.id)) {
            spdlog::warn("{} {}: duplicate id, keeping the first", kind, rt.info.id);
            continue;
        }
        rt.info.name = m.name;
        rt.info.version = m.version;
        rt.info.runtime = m.runtime;
        rt.info.api = m.api;
        rt.info.website = m.website.value_or("");
        const char* default_ext = "lua";
        if (m.runtime == "wasm") {
            default_ext = "wasm";
        } else if (m.runtime == "js") {
            default_ext = "js";
        }
        rt.info.entry = m.entry.value_or(std::string("main.") + default_ext);
        if (m.capabilities) {
            rt.info.capabilities = *m.capabilities;
        }
        if (is_enrichment) {
            // Enrichment sources are not scraper plugins: tag capability so
            // callers can distinguish; harmless if the manifest already set it.
            bool has_cap = false;
            for (const auto& c : rt.info.capabilities) {
                if (c == "enrichment") {
                    has_cap = true;
                    break;
                }
            }
            if (!has_cap) {
                rt.info.capabilities.emplace_back("enrichment");
            }
        }
        rt.info.dir = item.path().string();

        // User config lives beside the manifest, is never read by anyone else,
        // and is optional: absent/invalid -> plugin runs with no config.
        if (std::ifstream cf(fs::path(rt.info.dir) / "config.json"); cf) {
            std::stringstream ss;
            ss << cf.rdbuf();
            if (auto ec = glz::read_json(rt.cfg, std::move(ss).str())) {
                spdlog::warn("{} {}: config.json invalid ({})", kind, rt.info.id, glz::format_error(ec));
            } else {
                rt.has_cfg = true;
            }
        }

        const bool is_lua = m.runtime == "lua";
        const bool is_wasm = m.runtime == "wasm";
        const bool is_js = m.runtime == "js";
        if (!is_lua && !is_wasm && !is_js) {
            spdlog::warn("{} {}: runtime '{}' not loadable (host loads: lua, wasm, js)", kind, rt.info.id, m.runtime);
            continue;
        }
        if (m.api > kHostApi) {
            spdlog::warn("{} {}: manifest api {} > host api {}, skipped", kind, rt.info.id, m.api, kHostApi);
            continue;
        }
        if (!fs::exists(fs::path(rt.info.dir) / rt.info.entry)) {
            spdlog::warn("{} {}: entry {} not found", kind, rt.info.id, rt.info.entry);
            continue;
        }

        if (is_lua) {
            lua_State* L = luaL_newstate();
            if (L == nullptr) {
                spdlog::warn("{} {}: lua state allocation failed", kind, rt.info.id);
                continue;
            }
            rt.lua = {L, &lua_close};
            lua::open_sandbox(L);
            lua::install_host_tools(L, rt.info.id, conf, rt.cfg, rt.has_cfg, sget, sset, rt.info.dir, live_profile);
            // Entry scripts return nothing; run them, check validity explicitly.
            const sol::protected_function_result script =
                sol::state_view(L).safe_script_file((fs::path(rt.info.dir) / rt.info.entry).string());
            if (!script.valid()) {
                const sol::error e = script;
                spdlog::warn("{} {}: entry script failed: {}", kind, rt.info.id, e.what());
                continue;
            }
            // The chunk may leave a nil result on the stack; balance it so the
            // protected-call path below never sees a stray value.
            lua_pop(L, lua_gettop(L));
        } else if (is_js) {
            // js: entry is source text; the seam owns the realm and globals.
            std::ifstream jf(fs::path(rt.info.dir) / rt.info.entry, std::ios::binary);
            if (!jf) {
                spdlog::warn("{} {}: js entry {} unreadable", kind, rt.info.id, rt.info.entry);
                continue;
            }
            std::stringstream jbuf;
            jbuf << jf.rdbuf();
            js::Loaded loaded = js::load(rt.info.id, rt.info.dir, std::move(jbuf).str(),
                                         wasm::Services{sget, sset, make_wasm_fetch(rt.info.id, conf, live_profile)});
            if (!loaded.instance) {
                spdlog::warn("{} {}: js load failed: {}", kind, rt.info.id, loaded.error);
                continue;
            }
            rt.js = std::move(loaded.instance);
        } else {
            // wasm: entry is a prebuilt .wasm image; the seam owns load/link.
            std::ifstream wf(fs::path(rt.info.dir) / rt.info.entry, std::ios::binary);
            if (!wf) {
                spdlog::warn("{} {}: wasm entry {} unreadable", kind, rt.info.id, rt.info.entry);
                continue;
            }
            std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(wf), {}};
            wasm::Services services{sget, sset, make_wasm_fetch(rt.info.id, conf, live_profile)};
            wasm::Loaded loaded = wasm::load(rt.info.id, rt.info.dir, std::move(bytes), std::move(services));
            if (!loaded.instance) {
                spdlog::warn("{} {}: wasm load failed: {}", kind, rt.info.id, loaded.error);
                continue;
            }
            rt.wasm = std::move(loaded.instance);
        }

        std::string caps;
        for (size_t i = 0; i < rt.info.capabilities.size(); ++i) {
            caps += ((i != 0u) ? "," : "") + rt.info.capabilities[i];
        }
        if (is_enrichment) {
            spdlog::info("enrichment loaded: {} v{} (api {}, caps: {})", rt.info.id, rt.info.version, rt.info.api,
                         caps.empty() ? std::string("none") : caps);
        } else {
            store.upsert_plugin(rt.info.id, rt.info.name, rt.info.version, caps, rt.info.runtime);
            spdlog::info("plugin loaded: {} v{} (api {}, caps: {})", rt.info.id, rt.info.version, rt.info.api,
                         caps.empty() ? std::string("none") : caps);
        }

        auto shared = std::make_shared<Runtime>();
        shared->info = std::move(rt.info);
        shared->lua = std::move(rt.lua);
        shared->wasm = std::move(rt.wasm);
        shared->js = std::move(rt.js);
        fresh.emplace(shared->info.id, std::move(shared));
    }
}

} // namespace

int Host::load_all(Store& store) {
    std::map<std::string, std::shared_ptr<Runtime>> fresh;

    // Storage closures: same gate for every runtime this scan produces.
    Store* sp = &store;
    BS::thread_pool<>* dbp = impl_->db;
    StateGet sget = [sp, dbp](const std::string& pid, const std::string& key) -> std::optional<std::string> {
        if (dbp != nullptr) {
            return dbp->submit_task([&] { return sp->state_get(pid, key); }).get();
        }
        return sp->state_get(pid, key); // single-threaded tests
    };
    StateSet sset = [sp, dbp](const std::string& pid, const std::string& key, const std::string& val) {
        if (dbp != nullptr) {
            dbp->submit_task([&] { sp->state_set(pid, key, val); }).get();
            return;
        }
        sp->state_set(pid, key, val);
    };

    scan_dir(conf_, impl_->live_profile, store, sget, sset, conf_.dir, false, fresh);
    if (!conf_.enrichment_dir.empty()) {
        scan_dir(conf_, impl_->live_profile, store, sget, sset, conf_.enrichment_dir, true, fresh);
    }

    const std::scoped_lock lock(impl_->mu);
    const int total = static_cast<int>(fresh.size());
    impl_->plugins = std::move(fresh);
    return total;
}

} // namespace plugin
