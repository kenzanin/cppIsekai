#pragma once

/** \brief System-tray presence for the daemon (vendored Soundux/traypp).
 *
 * Spawns one detached UI thread owning the gtk/appindicator loop. Every string
 * and toggle (icon, title, status row, the three menu entries) comes from the
 * config registry's [tray] section; nothing user-visible is hardcoded here.
 * Menu: status label, Open Reader (xdg-open), Quit (raise SIGTERM -> the same
 * graceful path crow's handler already runs: serve() returns, scheduler and
 * browser shut down in main). No DISPLAY / no appindicator => warn + no tray,
 * the daemon runs exactly as before. ponytail: no stop/join -- the process
 * teardown reaps it; a hot restart of the tray needs a quit flag + join.
 */

#ifndef CPP_ISEKAI_TRAY_HPP
#define CPP_ISEKAI_TRAY_HPP

#include <spdlog/spdlog.h>

#include "assets/assets.hpp"
#include "config/config.hpp"

#include <atomic>
#include <csignal>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <unistd.h> // fork/exec/raise/getcwd

#if defined(__linux__) && !defined(CPP_ISEKAI_NO_TRAY)
#define CPP_ISEKAI_TRAY 1
// not <tray.hpp>: -Isrc precedes the vendored include dir, that name is this file
#include <components/button.hpp>
#include <components/label.hpp>
#include <components/separator.hpp>
#include <core/linux/tray.hpp>
#endif

namespace tray {

inline std::atomic<bool> restart_requested{false}; // set by the tray, consumed in main

namespace detail {

// Detach a shell line from the gtk callback thread: double-fork so the
// daemon exit cannot take the spawned process with it.
inline void spawn(const std::string& cmd) {
    if (pid_t pid = fork(); pid == 0) {
        setsid();
        if (pid_t pid2 = fork(); pid2 == 0) {
            for (int fd = 3; fd < 64; ++fd) close(fd); // drop inherited listen fd etc,
            // else run-cppIsekai's kill-holders sees this sh as an :8090 holder
            execlp("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        _exit(0);
    }
}

} // namespace detail

inline void start(const config::Config& cfg) {
#if defined(CPP_ISEKAI_TRAY)
    // "1" (or unset via the registry default) shows the tray; anything else
    // keeps it off without touching the rest of the daemon.
    if (cfg.tray_enabled != "1" && cfg.tray_enabled != "true") {
        spdlog::info("tray: disabled by config [tray] enabled={}", cfg.tray_enabled);
        return;
    }
    // XFCE's systray host resolves IconName through the icon theme, so a relative
    // path ("assets/tray.png") looks like a missing theme name and the item comes
    // up blank; an absolute path is loaded straight from file. Wayland hosts took
    // the relative form, hence "works on Wayland, invisible on X11".
    // [assets] memory mode exposes the icon through a temp file (GTK needs a
    // real path); disk mode keeps the configured absolute path.
    std::error_code ec;
    std::string icon = assets::icon_path(cfg.tray_icon).string();
    std::thread([port = cfg.port, icon, title = cfg.tray_title, label = cfg.tray_label,
                 open_label = cfg.tray_open_label, restart_label = cfg.tray_restart_label,
                 stop_label = cfg.tray_stop_label] {
        try {
            // {port} in the status row is the only place the port is formatted.
            const std::string port_s = std::to_string(port);
            std::string status = label;
            for (std::size_t at = status.find("{port}"); at != std::string::npos;
                 at = status.find("{port}", at + port_s.size())) {
                status.replace(at, 6, port_s);
            }
            Tray::Tray t{title, icon};
            t.addEntry(Tray::Label(status));
            t.addEntry(Tray::Separator());
            t.addEntry(Tray::Button(open_label, [port] {
                const std::string url = "http://127.0.0.1:" + std::to_string(port);
                if (pid_t pid = fork(); pid == 0) {
                    setsid();
                    execlp("xdg-open", "xdg-open", url.c_str(), static_cast<char*>(nullptr));
                    _exit(127); // exec failed
                }
            }));
            t.addEntry(Tray::Button(restart_label, [] {
                // flag + graceful TERM; main spawns the respawn after teardown
                // (fork from the gtk thread raced the exit and flaked)
                restart_requested.store(true);
                raise(SIGTERM);
            }));
            t.addEntry(Tray::Button(stop_label, [] { raise(SIGTERM); }));
            t.run(); // blocking gtk loop; ends when the appindicator dies
            spdlog::info("tray: closed");
        } catch (const std::exception& e) {
            spdlog::warn("tray disabled: {}", e.what()); // headless/no indicator
        }
    }).detach();
    spdlog::info("tray: started");
#endif
}

} // namespace tray

#endif // CPP_ISEKAI_TRAY_HPP
