#pragma once

/** \file chrome.hpp
 *  \brief Browser backend: app-local headless Chrome driven over CDP by the
 *  vendored cdp-cpp (third_party/cdp-cpp). Replaces the lightpanda MCP HTTP
 *  client -- same browser:: surface, one engine, no external daemon.
 *
 *  Lifecycle: boot() spawns headless Chrome (temp profile, kill-on-destruct)
 *  and opens one working page; call() dispatches plugin tool calls against
 *  it under the one browser mutex; a crashed/dead browser gets exactly one
 *  in-call relaunch attempt per incident (successor of D13, cooldown kept);
 *  shutdown() stops the child we own.
 */

#include <mutex>
#include <string>

namespace browser {

/** \brief The one browser mutex. Serializes every call and every lifecycle
 *  op, so a harvest is never interleaved. */
std::mutex& mutex();

/** \brief Boot configuration. Empty \p command = auto-discover the Chrome
 *  binary (stable channel). Call before boot(). */
void configure(std::string command);

/** \brief Spawn headless Chrome + open the working page. Idempotent: a
 *  healthy backend is kept. Fatal => false + err (boot aborts the daemon). */
bool boot(std::string* err);

/** \brief Plugin tool call. Tools: "goto" {url} (waits for load) and
 *  "evaluate" {script} (value-serialized result; objects come back as JSON).
 *  Empty string + \p err set on failure. Thread-safe. */
std::string call(const std::string& tool, const std::string& args_json, std::string* err = nullptr);

/** \brief Stop the app-owned Chrome. Safe pre/post boot. */
void shutdown();

} // namespace browser
