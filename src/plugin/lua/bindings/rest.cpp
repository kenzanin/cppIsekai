#include "plugin/lua/bindings.hpp"

#include "browser/chrome.hpp"
#include "plugin/host_codecs.hpp"
#include "plugin/host_crypto.hpp"
#include "plugin/host_text.hpp"
#include "plugin/host_xpath.hpp"
#include "plugin/htmltext.hpp"
#include "plugin/url_codec.hpp"

#include <spdlog/spdlog.h>

namespace plugin::lua {

/** \brief Remaining host tables (text, codecs, re2, crypto, browser, html).
 *  Bodies moved verbatim from lua_runtime.cpp; only the outer host-table name
 *  changed. Registration order within install_host_tools is preserved. */
void bind_rest(lua_State* L, sol::table host, const HostCtx& ctx) {
    sol::state_view sv(L);
    const std::string& id = ctx.id;

    // host.text: status normalization, chapter-number extraction, date→ISO
    // (P2, host_text.hpp — one core, three thin marshals; design D4).
    sol::table text_tbl = sv.create_table();
    text_tbl["normalize_status"] = [](const std::string& s) { return plugin::host_text::normalize_status(s); };
    text_tbl["chapter_num"] = [](const std::string& s) -> std::optional<double> {
        const auto v = plugin::host_text::chapter_num(s);
        return v ? std::optional<double>(*v) : std::nullopt;
    };
    text_tbl["date_to_iso"] = [](const std::string& s) -> std::optional<std::string> {
        return plugin::host_text::date_to_iso(s);
    };
    text_tbl["date_epoch"] = [](const std::string& s) -> std::optional<long long> {
        return plugin::host_text::date_epoch(s);
    };
    text_tbl["json_blob"] = [](const std::string& s, const std::string& marker, sol::this_state ts) -> sol::object {
        const auto v = plugin::host_text::json_blob(s, marker);
        return v ? sol::make_object(ts, *v) : sol::make_object(ts, sol::lua_nil);
    };
    text_tbl["trim"] = [](const std::string& s) { return plugin::host_text::trim(s); };
    text_tbl["unescape"] = [](const std::string& s) { return plugin::host_text::unescape(s); };
    text_tbl["strip_html"] = [](const std::string& s) { return plugin::host_text::strip_html(s); };
    host["text"] = text_tbl;

    // host.codecs: base64/hex encodings (P6, host_codecs.hpp one core, thin
    // marshals); decode nil on invalid, the house optional contract.
    sol::table codec_tbl = sv.create_table();
    codec_tbl["base64_encode"] = [](const std::string& s) { return plugin::host_codec::base64_encode(s); };
    codec_tbl["base64_decode"] = [](const std::string& s, sol::this_state ts) -> sol::object {
        const auto v = plugin::host_codec::base64_decode(s);
        return v ? sol::make_object(ts, *v) : sol::make_object(ts, sol::lua_nil);
    };
    codec_tbl["hex_encode"] = [](const std::string& s) { return plugin::host_codec::hex_encode(s); };
    codec_tbl["hex_decode"] = [](const std::string& s, sol::this_state ts) -> sol::object {
        const auto v = plugin::host_codec::hex_decode(s);
        return v ? sol::make_object(ts, *v) : sol::make_object(ts, sol::lua_nil);
    };
    host["codecs"] = codec_tbl;

    // Host-side heavy primitives (design D4 one core, thin marshals): RE2
    // (linear-time, no ReDoS) and lexbor DOM selectors. Bad patterns throw
    // (loud); no-match / malformed HTML return empty lists.
    sol::table re2 = sv.create_table();
    re2["find"] = [](sol::this_state ts, const std::string& p, const std::string& t) -> sol::object {
        std::vector<std::string> groups;
        try {
            groups = re2_find(p, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
        sol::table out(ts.lua_state(), sol::new_table());
        for (size_t i = 0; i < groups.size(); ++i) {
            out[i + 1] = groups[i]; // lua arrays are 1-based
        }
        return out;
    };
    re2["match"] = [](sol::this_state ts, const std::string& p, const std::string& t) -> sol::object {
        std::vector<std::string> groups;
        try {
            groups = re2_match(p, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
        sol::table out(ts.lua_state(), sol::new_table());
        for (size_t i = 0; i < groups.size(); ++i) {
            out[i + 1] = groups[i];
        }
        return out;
    };
    // P4: find_all, gmatch (closure), replace, quote, find_index
    re2["find_all"] = [](sol::this_state ts, const std::string& p, const std::string& t) -> sol::object {
        std::vector<std::vector<std::string>> all;
        try {
            all = re2_find_all(p, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
        sol::table out(ts.lua_state(), sol::new_table());
        for (size_t i = 0; i < all.size(); ++i) {
            sol::table row(ts.lua_state(), sol::new_table());
            for (size_t j = 0; j < all[i].size(); ++j) {
                row[j + 1] = all[i][j]; // 1-based per existing find/match convention
            }
            out[i + 1] = sol::make_object(ts.lua_state(), row);
        }
        return out;
    };
    re2["gmatch"] = [](sol::this_state ts, const std::string& p, const std::string& t) -> sol::object {
        // Closure iterator over matches (same 1-based groups array as find_all).
        std::vector<std::vector<std::string>> all;
        try {
            all = re2_find_all(p, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
        auto shared = std::make_shared<std::vector<std::vector<std::string>>>(std::move(all));
        auto* idx = new size_t(0); // heap: outlives the call
        lua_State* L = ts.lua_state();
        return sol::make_object(L, [shared, idx, L]() -> std::optional<sol::table> {
            if (*idx >= shared->size()) {
                return std::nullopt; // end
            }
            ++(*idx);
            // Build a 1-based sol::table from shared->[*idx - 1]
            sol::table row(L, sol::new_table());
            const auto& groups = (*shared)[*idx - 1];
            for (size_t j = 0; j < groups.size(); ++j) {
                row[j + 1] = groups[j];
            }
            return std::optional<sol::table>(std::move(row));
        });
    };
    re2["replace"] = [](const std::string& p, const std::string& r, const std::string& t) {
        try {
            return re2_replace(p, r, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
    };
    re2["quote"] = [](const std::string& s) { return re2_quote(s); };
    re2["find_index"] = [](const std::string& p, const std::string& t) {
        try {
            return static_cast<long long>(re2_find_index(p, t)); // 0 = not found
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
    };
    // capture: group 1 (or whole match for group-less patterns), nil on no
    // match -- the find1 shim, host-owned (plugin-host-promotion D2).
    re2["capture"] = [](const std::string& p, const std::string& t) -> std::optional<std::string> {
        try {
            return re2_capture(p, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
    };
    host["re2"] = re2;

    // crypto: host-executed, plugin-declared transform. Returns (result, err) pair
    // like http.fetch so plugins can distinguish bad input from a site 403.
    sol::table crypto = sv.create_table();
    crypto["vrf_sign"] = [id](const std::string& api_path, const sol::optional<sol::table>& params,
                              const sol::optional<sol::table>& stages,
                              sol::this_state ts) -> std::pair<sol::object, std::optional<std::string>> {
        const auto fail = [&](std::string why) {
            spdlog::warn("[plugin:{}] vrf_sign {} failed: {}", id, api_path, why);
            return std::make_pair(sol::make_object(ts, sol::lua_nil), std::optional(std::move(why)));
        };
        if (api_path.empty())
            return fail("vrf_sign: api_path is empty");

        std::map<std::string, std::string> prm;
        if (params) {
            for (auto& [k, v] : *params) {
                if (!k.is<std::string>())
                    return fail("vrf_sign: params keys must be strings");
                if (v.is<std::string>()) {
                    prm[k.as<std::string>()] = v.as<std::string>();
                } else if (v.is<bool>()) {
                    prm[k.as<std::string>()] = v.as<bool>() ? "true" : "false";
                } else if (v.is<int>() || v.is<double>() || v.is<float>()) {
                    prm[k.as<std::string>()] = std::to_string(v.as<double>());
                } else {
                    return fail("vrf_sign: params[\"" + k.as<std::string>() + "\"] must be scalar");
                }
            }
        }

        std::vector<VrfStage> vstages;
        if (!stages || stages->size() == 0)
            return fail("vrf_sign: stages must be a non-empty array");
        for (size_t i = 0; i < stages->size(); ++i) {
            const size_t idx = i + 1;
            const sol::optional<sol::table> st = (*stages)[idx];
            if (!st)
                return fail("vrf_sign: stage " + std::to_string(idx) + ": missing");
            const sol::optional<double> iv = (*st)["iv"];
            const sol::optional<std::string> key = (*st)["key"];
            const sol::optional<std::string> tbl = (*st)["tbl"];
            if (!iv || !key || !tbl)
                return fail("vrf_sign: stage " + std::to_string(idx) + ": needs iv, key, tbl");
            const double fid = *iv;
            const int ivv = static_cast<int>(fid);
            if (fid != static_cast<double>(ivv) || ivv < 0 || ivv > 255) {
                return fail("vrf_sign: stage " + std::to_string(idx) + ": iv must be integer 0-255");
            }
            vstages.push_back({static_cast<uint8_t>(ivv), *key, *tbl});
        }

        std::string err;
        auto out = vrf_sign(api_path, prm, vstages, err);
        if (!out) {
            spdlog::warn("[plugin:{}] vrf_sign {} failed: {}", id, api_path, err);
            return std::make_pair(sol::make_object(ts, sol::lua_nil), std::optional(std::move(err)));
        }
        return std::make_pair(sol::make_object(ts, *out), std::optional<std::string>{});
    };
    // host.crypto P7 (host_crypto.hpp): hex in/out; nil on tag/hex/key failure.
    crypto["sha256_hex"] = [](const std::string& s) { return plugin::host_crypto::sha256_hex(s); };
    crypto["md5_hex"] = [](const std::string& s) { return plugin::host_crypto::md5_hex(s); };
    crypto["hmac_sha256_hex"] = [](const std::string& data, const std::string& key) {
        return plugin::host_crypto::hmac_sha256_hex(data, key);
    };
    crypto["aes_gcm_decrypt"] = [](const std::string& iv, const std::string& ct, const std::string& tag,
                                   const std::string& key, sol::this_state ts) -> sol::object {
        const auto pt = plugin::host_crypto::aes_gcm_decrypt(iv, ct, tag, key);
        return pt ? sol::make_object(ts, *pt) : sol::make_object(ts, sol::lua_nil);
    };
    host["crypto"] = crypto;

    // browser (change browser-host D5): one passthrough, host.browser.call(tool, args)
    // -> (result, err) like http.fetch. args = Lua table (encoded here) or raw JSON
    // string; result is the tool text, err carries transport/RPC/isError diagnostics.
    sol::table browser_tab = sv.create_table();
    browser_tab["call"] = [id](const std::string& tool, const sol::object& args, sol::this_state ts)
                              -> std::pair<sol::object, std::optional<std::string>> {
        const auto fail = [&](std::string why) {
            spdlog::warn("[plugin:{}] browser.{} failed: {}", id, tool, why);
            return std::make_pair(sol::make_object(ts, sol::lua_nil), std::optional(std::move(why)));
        };
        std::string args_json;
        if (args.is<std::string>()) {
            args_json = args.as<std::string>();
        } else if (args.is<sol::table>()) {
            args_json = dump_json(sol_to_json(args));
        } else if (!args.valid() || args == sol::lua_nil) {
            args_json = "{}";
        } else {
            return fail("browser: args must be a table or json string");
        }
        std::string err;
        std::string out = ::browser::call(tool, args_json, &err);
        if (out.empty() && !err.empty()) {
            return fail(std::move(err));
        }
        return std::make_pair(sol::make_object(ts, std::move(out)), std::optional<std::string>{});
    };
    host["browser"] = browser_tab;

    sol::table html = sv.create_table();
    const auto to_table = [](sol::this_state ts, const std::vector<std::string>& vals) {
        sol::table out(ts.lua_state(), sol::new_table());
        for (size_t i = 0; i < vals.size(); ++i) {
            out[i + 1] = vals[i];
        }
        return out;
    };
    html["select"] = [to_table](sol::this_state ts, const std::string& h, const std::string& c) -> sol::object {
        return to_table(ts, html_select(h, c));
    };
    html["attr"] = [to_table](sol::this_state ts, const std::string& h, const std::string& c,
                              const std::string& a) -> sol::object { return to_table(ts, html_attr(h, c, a)); };
    // P8/P9: host.html.xpath(expr, html) -> string table; nil on bad expr.
    html["xpath"] = [](const std::string& expr, const std::string& doc, sol::this_state ts) -> sol::object {
        const auto vals = plugin::host_xpath::xpath(doc, expr);
        if (!vals) {
            return sol::make_object(ts, sol::lua_nil);
        }
        sol::table out(ts.lua_state(), sol::new_table());
        for (size_t i = 0; i < vals->size(); ++i) {
            out[i + 1] = (*vals)[i];
        }
        return sol::make_object(ts, out);
    };
    host["html"] = html;
}

} // namespace plugin::lua
