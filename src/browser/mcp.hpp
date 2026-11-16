#pragma once

/** \file mcp.hpp
 *  \brief Browser-host RPC client (change browser-host): JSON-RPC `tools/call`
 *  against the lightpanda MCP daemon over plain HTTP (D1/D2). One shared
 *  session, one mutex (D3); lazy initialize with a single 404 re-init retry
 *  (D4); crash recovery is a registered host hook (D13). */

#include <functional>
#include <mutex>
#include <string>

namespace browser {

/** \brief D3/D10: the one browser mutex. Serializes every MCP call sequence
 *  and every supervisor child operation, so a harvest is never interleaved. */
std::mutex& mutex();

/** \brief Boot configuration. Empty \p url disables the subsystem (calls fail
 *  with a clear error). \p proxy rides fetch::sync unchanged -- the D6
 *  loopback exemption inside the gate keeps the local daemon direct. */
void configure(std::string url, std::string proxy);

/** \brief D13 hook: the supervisor registers ONE recovery attempt (respawn +
 *  session drop). Empty default = no recovery (unit tests, adopt-only mode).
 *  Return true when the backend was brought back. */
using Recover = std::function<bool()>;
void on_death(Recover fn);

/** \brief Configured endpoint root (supervision probe/args; mutex-held reads). */
const std::string& endpoint();

/** \brief D13 helper: drop the cached session id (post-respawn the old one is
 *  stale). Caller holds the mutex. */
void reset_session();

/** \brief `tools/call`. Returns the tool result text (`result.content[0].text`;
 *  lossless fallback: the raw response body when the envelope has no text
 *  content). Empty string + \p err set on any failure -- transport, RPC error
 *  object, or `result.isError` with its text as the reason. Thread-safe. */
std::string call(const std::string& tool, const std::string& args_json, std::string* err = nullptr);


} // namespace browser
