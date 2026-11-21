#include "wasm_internal.hpp"

#include "imports/host_imports.hpp"

#include <array>
#include <string>
#include <string_view>

namespace plugin::wasm {
namespace {

struct Import {
    const char* name;
    const char* sig; // m3 link notation, m3_core.h ConvertTypeCharToTypeId
    M3RawCall fn;
};

// Import names, signatures and order are the plugin ABI: bodies live one
// topic TU per domain under imports/, this table is the only registration site.
constexpr std::array kImports{
    Import{"now", "I", imports::host_now},
    Import{"log_info", "v(ii)", imports::host_log_info},
    Import{"fs_read", "i(iiii)", imports::host_fs_read},
    Import{"storage_get", "i(iiii)", imports::host_storage_get},
    Import{"storage_set", "v(iiii)", imports::host_storage_set},
    Import{"http_fetch", "i(iiiiii)", imports::host_http_fetch},
    Import{"http_fetch_body", "i(iiiiii)", imports::host_http_fetch_body},
    Import{"http_get_json", "i(iiiiii)", imports::host_http_get_json},
    Import{"http_post_json", "i(iiiiiiii)", imports::host_http_post_json},
    Import{"plugin_param", "i(iiii)", imports::host_plugin_param},
    Import{"re2_match", "i(iiiiii)", imports::host_re2_match},
    Import{"re2_find", "i(iiiiii)", imports::host_re2_find},
    Import{"re2_find_all", "i(iiiiiiiiii)", imports::host_re2_find_all},
    Import{"re2_replace", "i(iiiiiiiiii)", imports::host_re2_replace},
    Import{"re2_quote", "i(iiii)", imports::host_re2_quote},
    Import{"re2_find_index", "i(iiiiii)", imports::host_re2_find_index},
    Import{"html_select", "i(iiiii)", imports::host_html_select},
    Import{"html_attr", "i(iiiiiii)", imports::host_html_attr},
    Import{"html_xpath", "i(iiiiii)", imports::host_html_xpath},
    Import{"url_encode", "i(iiii)", imports::host_url_encode},
    Import{"url_decode", "i(iiii)", imports::host_url_decode},
    Import{"text_normalize_status", "i(iiii)", imports::host_text_normalize_status},
    Import{"text_chapter_num", "I(ii)", imports::host_text_chapter_num},
    Import{"text_date_to_iso", "i(iiii)", imports::host_text_date_to_iso},
    Import{"text_trim", "i(iiii)", imports::host_text_trim},
    Import{"text_unescape", "i(iiii)", imports::host_text_unescape},
    Import{"text_strip_html", "i(iiii)", imports::host_text_strip_html},
    Import{"codec_base64_encode", "i(iiii)", imports::host_codec_base64_encode},
    Import{"codec_base64_decode", "i(iiii)", imports::host_codec_base64_decode},
    Import{"codec_hex_encode", "i(iiii)", imports::host_codec_hex_encode},
    Import{"codec_hex_decode", "i(iiii)", imports::host_codec_hex_decode},
    Import{"crypto_sha256_hex", "i(iiii)", imports::host_crypto_sha256_hex},
    Import{"crypto_md5_hex", "i(iiii)", imports::host_crypto_md5_hex},
    Import{"crypto_hmac_sha256_hex", "i(iiiiii)", imports::host_crypto_hmac_sha256_hex},
    Import{"crypto_aes_gcm_decrypt", "i(iiiiiiiiii)", imports::host_crypto_aes_gcm_decrypt},
    Import{"browser_call", "i(iiiiii)", imports::host_browser_call},
};

} // namespace

bool link_imports(IM3Module module, Instance& inst, std::string& err) {
    for (const Import& imp : kImports) {
        const M3Result e = m3_LinkRawFunctionEx(module, "env", imp.name, imp.sig, imp.fn, &inst);
        if (e != nullptr && std::string_view{e} != std::string_view{m3Err_functionLookupFailed}) {
            err = std::string("link ") + imp.name + ": " + e;
            return false;
        }
    }
    return true;
}

} // namespace plugin::wasm
