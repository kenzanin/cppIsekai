#pragma once

/** \brief System-tray presence for the daemon (vendored Soundux/traypp).
 *
 * Spawns one detached UI thread owning the gtk/appindicator loop. Menu:
 * status label, Open Reader (xdg-open), Quit (raise SIGTERM -> the same
 * graceful path crow's handler already runs: serve() returns, scheduler and
 * browser shut down in main). No DISPLAY / no appindicator => warn + no tray,
 * the daemon runs exactly as before. ponytail: no stop/join -- the process
 * teardown reaps it; a hot restart of the tray needs a quit flag + join.
 */

#ifndef CPP_ISEKAI_TRAY_HPP
#define CPP_ISEKAI_TRAY_HPP

#include <spdlog/spdlog.h>

#include <csignal>
#include <thread>
#include <unistd.h> // fork/exec/raise

#if defined(__linux__) && !defined(CPP_ISEKAI_NO_TRAY)
#define CPP_ISEKAI_TRAY 1
// not <tray.hpp>: -Isrc precedes the vendored include dir, that name is this file
#include <components/button.hpp>
#include <components/label.hpp>
#include <components/separator.hpp>
#include <core/linux/tray.hpp>
#endif

namespace tray {

inline void start(unsigned port) {
#if defined(CPP_ISEKAI_TRAY)
    std::thread([port] {
        try {
            Tray::Tray t{"cppIsekai", "assets/tray.png"};
            t.addEntry(Tray::Label("cppIsekai :" + std::to_string(port) + " - running"));
            t.addEntry(Tray::Separator());
            t.addEntry(Tray::Button("Open Reader", [port] {
                const std::string url = "http://127.0.0.1:" + std::to_string(port);
                if (pid_t pid = fork(); pid == 0) {
                    setsid();
                    execlp("xdg-open", "xdg-open", url.c_str(), static_cast<char*>(nullptr));
                    _exit(127); // exec failed
                }
            }));
            t.addEntry(Tray::Button("Quit", [] { raise(SIGTERM); }));
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
