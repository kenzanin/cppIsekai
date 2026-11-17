#include "browser/mcp.hpp"

#include "fetch/fetch.hpp"

#include "glaze/glaze.hpp"
#include "spdlog/spdlog.h"

#include <sstream>
#include <string>

namespace browser {

namespace {

std::string g_url;  // configure(); empty = subsystem off
std::string g_proxy;
std::string g_session; // Mcp-Session-Id, D4
unsigned long g_id = 0;
Recover g_recover;
constexpr const char* kAccept = "application/json, text/event-stream"; // D2: plain JSON comes back
constexpr long kTimeoutMs = 15000; // hard RPC cap; per-tool timeouts ride in args (design Risks)

/** \brief One POST to /mcp with the current session (if any). */
fetch::Result post(const std::string& body, long timeout_ms) {
    fetch::Request r;
    r.url = g_url + "/mcp";
    r.method = "POST";
    r.timeout_ms = timeout_ms;
    r.accept_header = kAccept;
    r.headers.emplace_back("Content-Type", "application/json");
    if (!g_session.empty()) {
        r.headers.emplace_back("mcp-session-id", g_session);
    }
    r.body = body;
    return fetch::sync(r, g_proxy);
}

/** \brief D4: fresh initialize; captures mcp-session-id, then the
 *  notifications/initialized courtesy (202, best-effort). */
bool init_session(std::string* err) {
    g_session.clear();
    const fetch::Result res = post(
        R"({"jsonrpc":"2.0","id":0,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},)"
        R"("clientInfo":{"name":"cppisekai","version":"0.1"}}})", kTimeoutMs);
    if (!res.error.empty() || res.status != 200) {
        if (err != nullptr) {
            *err = "initialize failed: " + (res.error.empty() ? "http " + std::to_string(res.status) : res.error);
        }
        return false;
    }
    const auto sid = res.headers.find("mcp-session-id");
    if (sid == res.headers.end() || sid->second.empty()) {
        if (err != nullptr) {
            *err = "initialize: no mcp-session-id header";
        }
        return false;
    }
    g_session = sid->second;
    (void)post(R"({"jsonrpc":"2.0","method":"notifications/initialized"})", kTimeoutMs);
    return true;
}

/** \brief Envelope -> result text. RPC error object or isError => failure. */
std::string envelope_result(const std::string& body, std::string* err) {
    // Slow tool calls come back as an SSE frame (Accept allows it): reduce to
    // the JSON carried by the last "data:" line before parsing.
    std::string json = body;
    if (body.find("event:") != std::string::npos) {
        std::string line;
        std::istringstream in(body);
        while (std::getline(in, line)) {
            if (line.rfind("data:", 0) == 0) {
                std::string payload = line.substr(5);
                if (!payload.empty() && payload.front() == ' ') {
                    payload.erase(0, 1);
                }
                if (!payload.empty()) {
                    json = payload;
                }
            }
        }
    }
    glz::generic v;
    if (auto ec = glz::read_json(v, json); ec || !v.is_object()) {
        if (err != nullptr) {
            *err = "bad jsonrpc envelope";
        }
        return {};
    }
    auto& obj = v.get_object();
    if (const auto e = obj.find("error"); e != obj.end() && e->second.is_object()) {
        std::string msg = "rpc error";
        auto& eobj = e->second.get_object();
        if (auto m = eobj.find("message"); m != eobj.end() && m->second.is_string()) {
            msg = m->second.get_string();
        }
        if (err != nullptr) {
            *err = msg;
        }
        return {};
    }
    const auto r = obj.find("result");
    if (r == obj.end() || !r->second.is_object()) {
        if (err != nullptr) {
            *err = "jsonrpc envelope without result";
        }
        return {};
    }
    auto& robj = r->second.get_object();
    const auto content = robj.find("content");
    if (content != robj.end() && content->second.is_array() && !content->second.get_array().empty()) {
        auto& first = content->second.get_array().front();
        if (first.is_object()) {
            auto& fobj = first.get_object();
            if (const auto t = fobj.find("text"); t != fobj.end() && t->second.is_string()) {
                const auto ie = robj.find("isError");
                if (ie != robj.end() && ie->second.is_boolean() && ie->second.get<bool>()) {
                    if (err != nullptr) { // tool-level failure: the text is the diagnostic
                        *err = t->second.get_string();
                    }
                    return {};
                }
                return t->second.get_string();
            }
        }
    }
    return body; // lossless fallback (task 1.2): unusual shape, hand back raw
}

} // namespace

std::mutex& mutex() {
    static std::mutex m;
    return m;
}

const std::string& endpoint() { return g_url; } // boot-time / mutex-held reads

void reset_session() { g_session.clear(); } // D13; caller holds the mutex

void configure(std::string url, std::string proxy) {
    std::lock_guard lock(mutex());
    g_url = std::move(url);
    g_proxy = std::move(proxy);
    g_session.clear(); // re-init on next call; also absorbs operator restarts (D12 note)
}

void on_death(Recover fn) {
    std::lock_guard lock(mutex());
    g_recover = std::move(fn);
}

std::string call(const std::string& tool, const std::string& args_json, std::string* err) {
    std::lock_guard lock(mutex());
    const auto fail = [&](const std::string& why) {
        spdlog::debug("[browser] {} failed: {}", tool, why);
        if (err != nullptr) {
            *err = why;
        }
        return std::string {};
    };
    if (g_url.empty()) {
        return fail("browser: no lightpanda_url configured");
    }

    glz::generic args;
    if (auto ec = glz::read_json(args, args_json); ec || !args.is_object()) {
        return fail("browser: args is not a json object");
    }
    // per-call timeout rides in args (design Risks): "timeout_ms", else 15 s cap
    long timeout_ms = kTimeoutMs;
    if (args.is_object()) {
        auto& obj = args.get_object();
        if (const auto t = obj.find("timeout_ms"); t != obj.end() && t->second.is_number()) {
            const long v = static_cast<long>(t->second.get<double>());
            if (v > 0) timeout_ms = v;
        }
    }
    spdlog::debug("[browser] {} timeout_ms={} session='{}'", tool, timeout_ms, g_session);
    glz::generic env;
    env["jsonrpc"] = "2.0";
    env["id"] = ++g_id;
    env["method"] = "tools/call";
    env["params"]["name"] = tool;
    env["params"]["arguments"] = std::move(args);
    const std::string body = glz::write_json(env).value_or(std::string {});

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (g_session.empty() && !init_session(err)) {
            // D13 covers the cold path too: dead backend at first contact gets
            // exactly one respawn attempt; the retry re-inits against it.
            if (attempt == 0 && g_recover && g_recover()) {
                continue; // session still empty; next iteration re-inits
            }
            return {}; // err already set by init_session
        }
        const fetch::Result res = post(body, timeout_ms);
        if (!res.error.empty()) {
            // D13: transport death gets exactly one recover attempt (respawn +
            // session drop); a second consecutive failure is final for the call.
            if (attempt == 0 && g_recover && g_recover()) {
                reset_session(); // old id is stale once the child is replaced
                continue;
            }
            return fail("transport: " + res.error);
        }
        if (res.status == 404) { // D4: stale session -> one fresh init + retry
            if (!init_session(err)) {
                return {};
            }
            continue;
        }
        if (res.status != 200) {
            return fail("http " + std::to_string(res.status));
        }
        return envelope_result(res.body, err);
    }
    return {}; // unreachable: both attempts returned or continued into failure
}

} // namespace browser
