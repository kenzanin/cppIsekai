#include "plugin_internal.hpp"
#include "store/store.hpp"

#include <filesystem>
#include <functional>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace plugin {


std::string err_json(const std::string& msg) {
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

} // namespace plugin
