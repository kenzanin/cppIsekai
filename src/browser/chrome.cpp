#include "browser/chrome.hpp"

#include "cdp/CDP.hpp"

#include "spdlog/spdlog.h"

#include <chrono>
#include <glaze/glaze.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

namespace browser {

namespace {

std::string s_command;                     // configure(); empty = auto-discover
std::string s_args;                        // space-split -> additionalFlags
bool s_headless = true;                    // [chrome] headless, "0" = headed (xvfb)
std::unique_ptr<cdp::quick::QuickBrowser> s_browser;
// The one working page; owned by s_browser, recreated on relaunch.
cdp::quick::QuickPage* s_page = nullptr;
bool s_dead = false;                       // give-up flag: fail fast post-incident
std::chrono::steady_clock::time_point s_last_recover{};
constexpr auto kRecoverCooldown = std::chrono::seconds(3);
constexpr int kNavTimeoutMs = 45000;
// Evaluate promises get 60 s inside the vendored eval (awaitPromise path);
// every other command uses the lib default. Revisit when a harvest outgrows it.

/** \brief True when the browser process + page still answer. */
bool alive() {
    return s_browser != nullptr && s_page != nullptr;
}

void teardown() {
    s_page = nullptr;
    if (s_browser) {
        s_browser.reset(); // killOnDestruct stops the child; temp profile cleaned
    }
}

/** \brief (Re)spawn Chrome and open the working page. Caller holds mutex. */
bool launch(std::string* err) {
    cdp::ChromeLaunchOptions opts;
    opts.headless = s_headless;
    opts.disableGpu = true;
    opts.tempProfilePrefix = "cppisekai_chrome_";
    if (!s_command.empty()) {
        opts.customChromePath = s_command;
        opts.preferredChannel = cdp::ChromeChannel::Custom; // custom path honored only under Custom
    }
    // [chrome] args: space-split; chromium flags carry no embedded spaces.
    std::istringstream iss(s_args);
    for (std::string flag; iss >> flag;) {
        opts.additionalFlags.push_back(flag);
    }
    // 60 s evaluate cap lives in the vendored QuickPage::eval (awaitPromise);
    // ponytail: raise there if a harvest ever outgrows it.
    auto br = cdp::quick::launch(opts);
    if (!br) {
        if (err != nullptr) {
            *err = "chrome launch failed" + (br.error.empty() ? "" : ": " + br.error);
        }
        return false;
    }
    s_browser = std::move(br.browser);
    auto page = s_browser->newPage("about:blank");
    if (!page) {
        teardown();
        if (err != nullptr) {
            *err = "chrome page open failed" + (page.error.empty() ? "" : ": " + page.error);
        }
        return false;
    }
    s_page = page.page;
    // comix's router bounces HeadlessChrome UAs off title pages (2026-11-17 tomy2x build);
    // present as normal Chrome for every navigation.
    std::string ua = s_page->evalString("navigator.userAgent");
    const std::string kHc = "HeadlessChrome";
    if (auto p = ua.find(kHc); p != std::string::npos) {
        ua.replace(p, kHc.size(), "Chrome");
        s_page->client().Emulation.setUserAgentOverride(ua, "", "");
        spdlog::info("browser: ua override -> {}", ua);
    }
    // Permanent console funnel: page exceptions land in our log at debug.
    page.page->client().Runtime.enable();
    page.page->client().connection().onEvent("Runtime.exceptionThrown", [](const cdp::CDPEvent& ev) {
        std::string desc = "?";
        if (auto* d = ev.params.find("exceptionDetails")) {
            if (auto* x = d->find("exception")) {
                if (auto* t = x->find("description")) desc = t->asString();
            }
            if (desc == "?") {
                if (auto* t = d->find("text")) desc = t->asString();
            }
        }
        spdlog::debug("[browser] page exception: {}", desc.substr(0, 300));
    });
    return true;
}

/** \brief One relaunch attempt per incident (cooldown kept from D13). */
 bool relaunch(std::string* err) {
    if (s_dead) {
        return false;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - s_last_recover < kRecoverCooldown) {
        return false; // mid-incident: no relaunch storm
    }
    s_last_recover = now;
    teardown();
    if (!launch(err)) {
        s_dead = true;
        spdlog::error("browser: chrome relaunch failed, giving up: {}", err ? *err : "");
        return false;
    }
    spdlog::warn("browser: chrome relaunched");
    return true;
}

/** \brief args[key] as string (glaze generic); empty when absent. */
std::string arg_str(const glz::generic& args, const char* key) {
    if (!args.is_object()) {
        return {};
    }
    auto& obj = args.get_object();
    const auto it = obj.find(key);
    if (it == obj.end() || !it->second.is_string()) {
        return {};
    }
    return it->second.get_string();
}

} // namespace

std::mutex& mutex() {
    static std::mutex m;
    return m;
}

void configure(std::string command, std::string args, std::string headless) {
    std::lock_guard lock(mutex());
    s_command = std::move(command);
    s_args = std::move(args);
    s_headless = headless != "0"; // empty/1 => headless, "0" => headed (xvfb)
    s_dead = false;
}

/** \brief (Re)spawn Chrome and open the working page. Caller holds mutex. */
bool boot_locked(std::string* err);

bool boot(std::string* err) {
    std::lock_guard lock(mutex());
    return boot_locked(err);
}

/** \brief boot() without the lock; call() already holds it. */
bool boot_locked(std::string* err) {
    if (alive()) {
        return true; // idempotent
    }
    s_dead = false;
    if (!launch(err)) {
        s_dead = true;
        return false;
    }
        spdlog::info("browser: ready ({})", s_command.empty() ? "auto-discovered" : s_command);
    return true;
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
                if (!s_page->navigate(url, kNavTimeoutMs)) {
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
                    auto v = s_page->eval("!!(" + expr + ")");
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
                return s_page->evalString(script); // value-serialized; objects => JSON
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

void shutdown() {
    std::lock_guard lock(mutex());
    teardown();
    s_dead = false;
}

} // namespace browser
