#include "fetch/fetch.hpp"
#include "fetch/fetch_internal.hpp"

#include <algorithm>
#include <cctype>

#include <cstdio>

#include <spdlog/spdlog.h>

namespace fetch {

namespace {
std::string sh_quote(const std::string& s) {
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += '\'';
    return out;
}
} // namespace

/** \brief Headless-renderer fetch for CF-challenge sites (4ucomics): the real
 *  browser engine solves the challenge; --dump html feeds the normal lexbor
 *  pipeline unchanged. Stateless per request -- verified to solve without
 *  persisted cookies. GET only: POST/header/cookie scrapes keep curl engines
 *  (profile rewritten to chrome116 by the caller below). */
Result rendered_fetch(const Request& req) {
    const bool obscura = req.profile == "obscura";
    const std::string tool = obscura ? "obscura" : "lightpanda";
    Result r;
    // coreutils guard on top of the tool's own waits (nightlies hang).
    const std::string secs = std::to_string(req.timeout_ms / 1000 + 15);
    std::string cmd = "timeout " + secs + " ";
    if (obscura) {
        // --stealth: consistent fingerprint + TLS impersonation. storage-dir is
        // best-effort (nightly persists nothing) but harmless.
        cmd += "obscura --stealth --storage-dir data/obscura fetch " + sh_quote(req.url) +
               " --dump html --wait-until networkidle2 --timeout " + std::to_string(req.timeout_ms / 1000);
    } else {
        // Challenge JS budget: 8s solved it in Nov 2026 testing, but the page
        // now regularly needs 15-25s before DOMContentLoaded -- 20s keeps the
        // coreutils headroom (timeout_ms/1000+15) honest without hanging.
        // ponytail: per-call wait knob if a second renderer plugin lands.
        cmd += "lightpanda fetch " + sh_quote(req.url) + " --dump html --wait-ms 20000";
    }
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) {
        r.error = tool + ": popen failed";
        return r;
    }
    std::array<char, 65536> buf{};
    size_t n = 0;
    while ((n = fread(buf.data(), 1, buf.size(), p)) > 0) {
        r.body.append(buf.data(), n);
    }
    const int rc = pclose(p);
    if (rc != 0) {
        r.body.clear();
        r.error = tool + " exit " + std::to_string(rc);
        return r;
    }
    if (r.body.empty()) {
        r.error = tool + ": empty body";
        return r;
    }
    r.status = 200;
    return r;
}

} // namespace fetch
