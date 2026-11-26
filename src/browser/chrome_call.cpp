#include "browser/chrome.hpp"

#include "cdp/CDP.hpp"
#include "plugin/http_client.hpp" // cookie_jar_path: one owner for the plugin jar path

#include "spdlog/spdlog.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <glaze/glaze.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "browser/chrome_internal.hpp"

namespace browser {

std::string call(const std::string& tool, const std::string& args_json, std::string* err) {
    std::lock_guard lock(mutex());
    const auto fail = [&](const std::string& why) {
        spdlog::debug("[browser] {} failed: {}", tool, why);
        if (err != nullptr) {
            *err = why;
        }
        return std::string {};
    };

    glz::generic args;
    if (auto ec = glz::read_json(args, args_json); ec || !args.is_object()) {
        return fail("browser: args is not a json object");
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!alive() && !(attempt == 0 ? boot_locked(nullptr) : relaunch(err))) {
            return attempt == 0 ? fail("browser: not booted") : std::string {}; // err set by relaunch
        }
        try {
            if (tool == "goto") {
                const std::string url = arg_str(args, "url");
                if (url.empty()) {
                    return fail("goto: no url");
                }
                if (!page()->navigate(url, nav_timeout_ms())) {
                    return fail("goto failed");
                }
                return "ok";
            }
            if (tool == "wait") {
                const std::string expr = arg_str(args, "expression");
                if (expr.empty()) {
                    return fail("wait: no expression");
                }
                int tmo = 15000;
                if (args.is_object()) {
                    const auto it = args.get_object().find("timeout_ms");
                    if (it != args.get_object().end() && it->second.is_number()) {
                        tmo = static_cast<int>(it->second.get_number());
                    }
                }
                // CDP wait_for: poll !!expr until truthy or deadline. Eval errors
                // (mid-navigation) count as not-yet and keep polling.
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(tmo);
                for (;;) {
                    auto v = page()->eval("!!(" + expr + ")");
                    if (v.isBool() && v.asBool()) {
                        return "ok";
                    }
                    if (std::chrono::steady_clock::now() >= deadline) {
                        return fail("wait: condition not met in " + std::to_string(tmo) + "ms");
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                }
            }
            if (tool == "evaluate") {
                const std::string script = arg_str(args, "script");
                if (script.empty()) {
                    return fail("evaluate: no script");
                }
                return page()->evalString(script); // value-serialized; objects => JSON
            }
            if (tool == "seed-jar") {
                // Persist the live session's cookies (httpOnly cf_clearance
                // included) as the plugin's Netscape jar: cf challenge zones
                // mint in the browser, curl-side image fetches consume. One
                // call per plugin process right after a solved goto; stale
                // jars after daemon restarts heal on the next data fetch.
                const std::string plugin = arg_str(args, "plugin");
                const std::string domain = arg_str(args, "domain");
                const std::string all = page()->getAllCookies();
                if (all.empty() || plugin.empty() || domain.empty()) {
                    return fail("seed-jar: no cookies or bad args");
                }
                glz::generic parsed{};
                if (auto ec = glz::read_json(parsed, all); ec != glz::error_code{} || !parsed.is_array()) {
                    return fail("seed-jar: unparsable cookie blob");
                }
                std::string out;
                for (const auto& c : parsed.get_array()) {
                    if (!c.is_object()) {
                        continue;
                    }
                    const auto& o = c.get_object();
                    const auto str = [&o](const char* k) {
                        const auto it = o.find(k);
                        return it != o.end() && it->second.is_string()
                                   ? std::string{std::get<std::string>(it->second.data)}
                                   : std::string{};
                    };
                    const std::string dom = str("domain");
                    if (dom.find(domain) == std::string::npos) {
                        continue;
                    }
                    double exp = 0;
                    if (const auto it = o.find("expires"); it != o.end() && it->second.is_number()) {
                        exp = it->second.get_number();
                    }
                    bool secure = false;
                    if (const auto it = o.find("secure"); it != o.end() && it->second.is_boolean()) {
                        secure = it->second.get<bool>();
                    }
                    // Netscape: domain TAB flag path secure expiry name value
                    out += dom + "\tTRUE\t" + (str("path").empty() ? "/" : str("path")) + "\t" +
                           (secure ? "TRUE" : "FALSE") + "\t" +
                           std::to_string(exp > 0 ? static_cast<long long>(exp) : 2147483647LL) + "\t" +
                           str("name") + "\t" + str("value") + "\n";
                }
                if (out.empty()) {
                    return fail("seed-jar: no cookies for domain");
                }
                // cookie_jar_path owns the layout + creates the state dir.
                std::ofstream f(plugin::cookie_jar_path(plugin), std::ios::trunc);
                if (!f) {
                    return fail("seed-jar: cannot write jar");
                }
                f << "# Netscape HTTP Cookie File\n" << out;
                return "ok";
            }
            return fail("browser: unknown tool '" + tool + "'");
        } catch (const std::exception& e) {
            if (attempt == 0 && relaunch(nullptr)) {
                continue; // dead browser mid-call: one retry on the fresh one
            }
            return fail(std::string("browser: ") + e.what());
        }
    }
    return {}; // unreachable
}

} // namespace browser
