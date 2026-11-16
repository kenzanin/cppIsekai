/** \brief In-process supervision of the lightpanda MCP child (browser-host
 *  D10/D12/D13).
 *
 *  Boot: probe the configured endpoint; a healthy listener is adopted
 *  as-is (never killed on shutdown). Otherwise one app-owned child is
 *  spawned from `lightpanda_command` and polled to readiness (~5 s);
 *  a child that never comes up aborts the boot with a clear error.
 *
 *  Crash path (D13): `recover` is the mcp-call death hook -- at most one
 *  respawn per incident with a short cooldown so a dead backend can
 *  never become a respawn storm; `mark_dead` pins the give-up state for
 *  the process lifetime (a fresh boot recovers).
 *
 *  All child operations run under browser::mutex() -- the same lock that
 *  serializes mcp calls, so a respawn can never interleave with a call.
 */
#pragma once

#include <string>

namespace browser {

/// Boot-time probe/adopt/spawn. Registers nothing; wire the D13 hook via
/// browser::on_death(&browser::recover) from main. Fatal => false + err.
bool supervise_boot(const std::string& command, std::string* err);

/// D13 death hook: one respawn (owned child restarted; adopted instance
/// replaced by an owned one on the same port). Must hold browser::mutex().
bool recover();

/// D13 give-up: after this, recover is a no-op until process restart.
void mark_dead();

/// Shutdown: stop only app-owned children; adopted instances are untouched.
void shutdown();

} // namespace browser
