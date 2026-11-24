#include "api_ctx.hpp"

#include "img.hpp"

#include <SimpleIni.h>

#include <fstream>
#include <filesystem>
#include <map>
#include <sstream>
#include <system_error>
#include <vector>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace http::api {

void register_admin_system(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    const auto& ini_path = c.ini_path;
    const auto& overrides_ref = c.overrides;

    // Dev endpoint: rescan the plugin dir, re-register, no restart.

    // api-first: the [backup] feature is otherwise config-only (auto tick).
    // GET lists snapshots; POST triggers the same VACUUM -> VACUUM INTO -> prune
    // unit the scheduler uses, so the whole feature is curl-accessible.
    // Restore stays a deliberate filesystem operation (stop daemon, copy
    // snapshot over data/library.db, start) -- exposing it over HTTP would
    // overwrite a live DB.
    // NOTE: capture ini_path/overrides_ref (references to long-lived objects),
    // NOT the local Ctx c -- it dies when register_api returns.
    CROW_ROUTE(app, "/api/backup")([&] {
        const config::Config cfg = config::resolve(overrides_ref, ini_path);
        const std::string dir = (std::filesystem::path(cfg.db).parent_path() / "backups").generic_string();
        crow::json::wvalue files(crow::json::type::List);
        int i = 0;
        std::error_code ec;
        if (std::filesystem::exists(dir, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                if (!entry.is_regular_file(ec)) continue;
                const auto name = entry.path().filename().string();
                if (name.rfind("library-", 0) != 0 || entry.path().extension() != ".db") continue;
                crow::json::wvalue f;
                f["name"] = name;
                f["size"] = static_cast<std::int64_t>(entry.file_size(ec));
                files[i++] = std::move(f);
            }
        }
        crow::json::wvalue out;
        out["ok"] = true;
        out["dir"] = dir;
        out["keep"] = cfg.backup_keep;
        out["files"] = std::move(files);
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });
    CROW_ROUTE(app, "/api/backup").methods(crow::HTTPMethod::POST)([&] {
        const config::Config cfg = config::resolve(overrides_ref, ini_path);
        const std::string dir = (std::filesystem::path(cfg.db).parent_path() / "backups").generic_string();
        const std::string path =
            on_pool(db, [&] { return store.maintenance_vacuum_backup(dir, cfg.backup_keep); });
        crow::json::wvalue out;
        out["ok"] = !path.empty();
        out["path"] = path;
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // /health: tiny liveness probe for the settings-page restart poll.
    CROW_ROUTE(app, "/health")([] {
        crow::json::wvalue out;
        out["ok"] = true;
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Re-exec in place (D: respawn): fork a watchdog that waits for this
    // process to die, then execs /proc/self/exe with the original argv.
    // The response is flushed first; the actual re-exec happens ~300ms later.
    CROW_ROUTE(app, "/admin/restart").methods(crow::HTTPMethod::POST)([&app] {
        const pid_t parent = getpid();
        const pid_t pid = fork();
        if (pid == 0) {
            // The watchdog inherits every fd, including crow's listen socket;
            // hold it and the re-exec'd instance can never bind. Drop them all.
            for (int fd = 3; fd < 1024; ++fd) {
                close(fd);
            }
            // Watchdog child: wait for the parent to actually exit (ESRCH), not
            // EPERM from reparenting under init; then exec with the original argv.
            for (;;) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (kill(parent, 0) != 0 && errno == ESRCH) {
                    break;
                }
            }
            // Parent is gone but the kernel may still hold the listening
            // socket briefly; give the port a beat to free before re-exec.
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            char exe_buf[4096];
            const ssize_t n = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
            if (n <= 0) {
                _exit(1);
            }
            std::string exe(exe_buf, static_cast<std::size_t>(n));
            // Rebuilt binary since boot: kernel marks the old path " (deleted)".
            // Strip it and re-exec the current file on disk instead.
            const std::string deleted = " (deleted)";
            if (exe.size() > deleted.size() && exe.ends_with(deleted)) {
                exe.erase(exe.size() - deleted.size());
            }
            // Rebuild argv from /proc/self/cmdline (NUL-separated).
            std::ifstream cl("/proc/self/cmdline", std::ios::binary);
            std::string raw((std::istreambuf_iterator<char>(cl)), std::istreambuf_iterator<char>());
            std::vector<char*> args;
            size_t i = 0;
            while (i < raw.size()) {
                const size_t end = raw.find('\0', i);
                args.push_back(const_cast<char*>(raw.data()) + i);
                i = (end == std::string::npos ? raw.size() : end + 1);
            }
            args.push_back(nullptr);
            execv(exe.c_str(), args.data());
            _exit(127);
        }
        crow::json::wvalue out;
        out["ok"] = true;
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        // Ask crow to stop shortly after the response is flushed, so the
        // watchdog's parent-liveness poll trips and the exec happens.
        std::thread([&app] {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            app.stop();
        }).detach();
        return r;
    });
}

} // namespace http::api
