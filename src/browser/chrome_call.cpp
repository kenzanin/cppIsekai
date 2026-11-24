#include "browser/chrome.hpp"

#include "cdp/CDP.hpp"

#include "spdlog/spdlog.h"

#include <chrono>
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
