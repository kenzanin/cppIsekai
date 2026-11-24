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

void bind_text_codecs(lua_State* L, sol::table host, const HostCtx& /*ctx*/) {
    sol::state_view sv(L);


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
    text_tbl["clean"] = [](const std::string& s) { return plugin::host_text::clean(s); };
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
}

} // namespace plugin::lua
