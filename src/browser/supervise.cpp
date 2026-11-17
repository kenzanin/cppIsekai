#include "browser/supervise.hpp"

#include "browser/mcp.hpp"
#include "fetch/fetch.hpp"

#include "spdlog/spdlog.h"

#include <reproc++/reproc.hpp>

#include <chrono>
#include <cctype>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace browser {

namespace {

reproc::process s_child;
bool s_child_alive = false; // s_child holds a started child
bool s_owned = false; // we spawned it => shutdown stops it (D12)
std::string s_command;
std::chrono::steady_clock::time_point s_last_recover{};
bool s_dead = false;

constexpr auto kRecoverCooldown = std::chrono::seconds(3);
constexpr int kBootWaitMs = 5000;   // D10: bounded readiness poll
constexpr int kProbeMs = 1500;
constexpr int kPollMs = 200;

/** \brief Transport-level liveness: any completed HTTP exchange counts
 *  (status is irrelevant -- we want "is something listening"). */
bool probe_ok() {
    fetch::Request r;
    r.url = endpoint() + "/mcp";
    r.timeout_ms = kProbeMs;
    r.accept_header = "application/json, text/event-stream";
    const fetch::Result res = fetch::sync(r, ""); // loopback: proxy-exempt by gate (D-loopback)
    return res.error.empty();
}

/** \brief Port for the child args, parsed off the configured endpoint. */
int endpoint_port() {
    const std::string& u = endpoint();
    const auto pos = u.rfind(':');
    if (pos == std::string::npos) {
        return 8899; // kDefault mirrors the config default
    }
    int port = 0;
    for (size_t i = pos + 1; i < u.size() && std::isdigit(static_cast<unsigned char>(u[i])); ++i) {
        port = port * 10 + (u[i] - '0');
    }
    return port > 0 ? port : 8899;
}

/// Extra argv words appended after the fixed `mcp --port --host` tail
/// (whitespace-split; empty => default spawn). Set via `[lightpanda] extra_args`.
std::string s_extra;

void stop_child() {
    if (!s_child_alive) {
        return; // never started / already reaped
    }
    (void)s_child.stop({{reproc::stop::terminate, reproc::milliseconds(2000)},
                        {reproc::stop::kill, reproc::milliseconds(1000)},
                        {}});
    s_child_alive = false;
}

/** \brief Spawn app-owned child; poll to readiness; kill + err on timeout. */
bool spawn_and_wait(std::string* err) {
    const std::string cmd = s_command.empty() ? "lightpanda" : s_command;
    const std::string port = std::to_string(endpoint_port());
    std::vector<std::string> args{cmd, "mcp", "--port", port, "--host", "127.0.0.1"};
    // ponytail: whitespace split, no quotes — flags with embedded spaces can
    // wait until a real user appears.
    std::istringstream extra(s_extra);
    std::string w;
    while (extra >> w) {
        args.push_back(w);
    }

    reproc::options o;
    o.redirect.discard = true; // quiet child; daemon logs carry the state
    // Fresh process object per spawn: reusing one that already ran a child to
    // completion comes back EINVAL from start (observed on the D13 path).
    s_child = reproc::process {};
    if (const std::error_code ec = s_child.start(args, o)) {
        if (err != nullptr) {
            *err = "spawn '" + cmd + "' failed: " + ec.message();
        }
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kBootWaitMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (probe_ok()) {
            s_child_alive = true;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
    }
    stop_child();
    if (err != nullptr) {
        *err = "spawned lightpanda not ready on :" + port + " within " + std::to_string(kBootWaitMs) + "ms";
    }
    return false;
}

} // namespace

bool supervise_boot(const std::string& command, const std::string& extra_args, std::string* err) {
    std::lock_guard lock(mutex());
    s_command = command;
    s_extra = extra_args;
    if (probe_ok()) {
        s_owned = false; // D12: adopt; never kill an external instance
        spdlog::info("browser: reusing external lightpanda on {}", endpoint());
        return true;
    }
    if (!spawn_and_wait(err)) {
        return false; // D10: boot aborts, err carries the reason
    }
    s_owned = true;
    spdlog::info("browser: spawned app-owned lightpanda ({} mcp :{})", s_command, endpoint_port());
    return true;
}

bool recover() {
    // caller holds mutex (the mcp call path)
    if (s_dead) {
        return false; // D13: process-lifetime give-up, fail fast
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - s_last_recover < kRecoverCooldown) {
        return false; // mid-incident: no respawn storm
    }
    s_last_recover = now;
    if (s_owned) {
        stop_child();
    }
    std::string spawn_err;
    if (!spawn_and_wait(&spawn_err)) {
        mark_dead();
        spdlog::error("browser: backend dead, giving up (D13): {}", spawn_err);
        return false;
    }
    s_owned = true; // post-respawn child is ours even if the first was adopted
    spdlog::warn("browser: lightpanda respawned (D13)");
    return true;
}

void mark_dead() {
    // caller holds mutex (call path / recover)
    s_dead = true;
}

void shutdown() {
    std::lock_guard lock(mutex());
    if (s_owned) {
        stop_child(); // adopted externals: untouched, D12
        s_owned = false;
    }
    s_dead = false;
}

} // namespace browser
