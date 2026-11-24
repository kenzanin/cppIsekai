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

void register_admin_config(Ctx& c) {
    auto& app = c.app;
    auto& plugins = c.plugins;
    const auto& ini_path = c.ini_path;
    const auto& overrides_ref = c.overrides;

    // Dev endpoint: rescan the plugin dir, re-register, no restart.

    // Config read (settings page): registry projection with layer provenance.
    // Re-resolves per request (ini may change on disk); no writes.
    CROW_ROUTE(app, "/config").methods(crow::HTTPMethod::GET)([&] {
        // Profile dropdown feed: [plugin] fetch_profiles = a,b,c in the ini
        // (not a registry knob, no flag/env -- UI-only list).
        std::vector<std::string> fetch_profiles;
        {
            CSimpleIniA ini;
            if (ini.LoadFile(ini_path.c_str()) >= 0) {
                if (const char* v = ini.GetValue("plugin", "fetch_profiles", nullptr)) {
                    std::string tok;
                    for (const char c : std::string(v)) {
                        if (c == ',') {
                            if (!tok.empty()) {
                                fetch_profiles.push_back(tok);
                                tok.clear();
                            }
                        } else if (c != ' ' && c != '\t') {
                            {
                                tok += c;
                            }
                        }
                    }
                    if (!tok.empty()) {
                        fetch_profiles.push_back(tok);
                    }
                }
            }
        }
        if (fetch_profiles.empty()) {
            fetch_profiles = {
                "chrome99",  "chrome100", "chrome101", "chrome104",  "chrome107",  "chrome110",
                "chrome116", "edge99",    "edge101",   "safari15_3", "safari15_5", "ff91esr",
                "ff95",      "ff98",      "ff100",     "ff102",      "ff109",      "ff117",
            };
        }
        crow::json::wvalue list(crow::json::type::List);
        int p = 0;
        for (const auto& x : fetch_profiles) {
            list[p++] = x;
        }

        crow::json::wvalue knobs(crow::json::type::List);
        int i = 0;
        for (const auto& d : config::describe(ini_path, overrides_ref)) {
            crow::json::wvalue k;
            k["section"] = d.section;
            k["key"] = d.key;
            k["fullKey"] = d.full_key;
            k["doc"] = d.doc;
            k["type"] = d.type == config::Type::Str ? "str" : (d.type == config::Type::Int ? "int" : "int64");
            k["flag"] = d.flag;
            k["env"] = d.env;
            k["default"] = d.def;
            k["value"] = d.value;
            k["source"] = d.source;
            k["readonly"] = d.readonly;
            knobs[i++] = std::move(k);
        }
        crow::json::wvalue out;
        out["ok"] = true;
        out["knobs"] = std::move(knobs);
        out["fetch_profiles"] = std::move(list);
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Config save (D5): validation-first merge, comments/ordering survive.
    // Body {"section.key": string|number} in resolved full-key form.
    CROW_ROUTE(app, "/config").methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
        auto fail = [](int code, const std::string& msg) {
            crow::json::wvalue e;
            e["ok"] = false;
            e["error"] = msg;
            crow::response r{std::move(e)};
            r.code = code;
            r.set_header("Content-Type", "application/json");
            return r;
        };

        std::map<std::string, std::string> updates;
        std::string parse_err;
        if (!config::parse_updates(req.body, updates, parse_err)) {
            return fail(400, parse_err);
        }
        // Registry-declared read-only knobs (e.g. [assets] path) cannot be
        // changed through HTTP either -- the UI is not the only client.
        for (const auto& d : config::describe(ini_path, overrides_ref)) {
            if (d.readonly) {
                updates.erase(d.full_key);
            }
        }

        std::string err;
        const int written = config::save(updates, ini_path, &err);
        if (written < 0) {
            return fail(written == -2 ? 500 : 400, err);
        }

        // Live default (task 1.2): the running Host picks up the new
        // plugin.fetch_profile without a restart.
        if (const auto it = updates.find("plugin.fetch_profile"); it != updates.end()) {
            plugins.set_default_profile(it->second);
        }

        crow::json::wvalue out;
        out["ok"] = true;
        out["written"] = written;
        crow::response r{std::move(out)};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // api-first: the [backup] feature is otherwise config-only (auto tick).
}

} // namespace http::api
