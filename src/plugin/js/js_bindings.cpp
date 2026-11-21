/** \file js_bindings.cpp
 *  \brief The host.* registration table: property names + order live here;
 *  the handler bodies live in bindings/<topic>.cpp.
 */

#include "bindings/host_bindings.hpp"
#include "js_internal.hpp"
#include "js_rt.hpp"

namespace plugin::js {

void bind_host_globals(JSContext* c) {
    using namespace bindings;
    JSValue g = JS_GetGlobalObject(c);
    const auto bind = [&](JSValue obj, const char* name, JSValue fn) {
        JS_SetPropertyStr(c, obj, name, fn); // consumes the reference
    };

    // One namespaced surface: everything the host provides lives under
    // `host.` (parity with the lua/wasm tables; no bare globals).
    JSValue host = JS_NewObject(c);

    bind(host, "now", JS_NewCFunction(c, js_now, "now", 0));

    JSValue log = JS_NewObject(c);
    bind(log, "info", JS_NewCFunctionMagic(c, js_log, "info", 1, JS_CFUNC_generic_magic, 0));
    bind(log, "error", JS_NewCFunctionMagic(c, js_log, "error", 1, JS_CFUNC_generic_magic, 1));
    bind(host, "log", log);

    JSValue http = JS_NewObject(c);
    bind(http, "fetch", JS_NewCFunction(c, js_http_fetch, "fetch", 2));
    bind(http, "fetch_body", JS_NewCFunction(c, js_http_fetch_body, "fetch_body", 2));
    bind(http, "get_json", JS_NewCFunction(c, js_http_get_json, "get_json", 2));
    bind(http, "post_json", JS_NewCFunction(c, js_http_post_json, "post_json", 3));
    bind(host, "http", http);

    JSValue fs = JS_NewObject(c);
    bind(fs, "read", JS_NewCFunction(c, js_fs_read, "read", 1));
    bind(host, "fs", fs);

    // require(): CommonJS-style, confined to the plugin dir; modules cached
    // on globalThis.__modules so repeat requires are free.
    bind(g, "require", JS_NewCFunction(c, js_require, "require", 1));

    JSValue storage = JS_NewObject(c);
    bind(storage, "get", JS_NewCFunction(c, js_storage_get, "get", 1));
    bind(storage, "set", JS_NewCFunction(c, js_storage_set, "set", 2));
    bind(host, "storage", storage);

    // Host-side heavy primitives (design D4 one core, thin marshals): RE2
    // (linear-time, no ReDoS) and lexbor DOM selectors. Bad patterns throw
    // (loud); no-match / malformed HTML return [].
    JSValue re2 = JS_NewObject(c);
    bind(re2, "find", JS_NewCFunction(c, js_re2_find, "find", 2));
    bind(re2, "match", JS_NewCFunction(c, js_re2_match, "match", 2));
    bind(re2, "find_all", JS_NewCFunction(c, js_re2_find_all, "find_all", 2));
    bind(re2, "replace", JS_NewCFunction(c, js_re2_replace, "replace", 3));
    bind(re2, "quote", JS_NewCFunction(c, js_re2_quote, "quote", 1));
    bind(re2, "find_index", JS_NewCFunction(c, js_re2_find_index, "find_index", 2));
    bind(re2, "capture", JS_NewCFunction(c, js_re2_capture, "capture", 2));
    bind(host, "re2", re2);

    JSValue html = JS_NewObject(c);
    bind(html, "select", JS_NewCFunction(c, js_html_select, "select", 2));
    bind(html, "attr", JS_NewCFunction(c, js_html_attr, "attr", 3));
    bind(html, "xpath", JS_NewCFunction(c, js_html_xpath, "xpath", 2));
    bind(host, "html", html);

    // Query-component codec, same core lua's host.url_* delegate to.
    bind(host, "url_encode", JS_NewCFunction(c, js_url_encode, "url_encode", 1));
    bind(host, "url_decode", JS_NewCFunction(c, js_url_decode, "url_decode", 1));
    bind(host, "url_abs", JS_NewCFunction(c, js_url_abs, "url_abs", 2));

    // Plugin-local config (design D2) and host-side crypto (D3), lua parity.
    JSValue config = JS_NewObject(c);
    bind(config, "get", JS_NewCFunction(c, js_config_get, "get", 1));
    bind(host, "config", config);

    // host.crypto (P7) bindings below; cores are file-scope fns.
    JSValue crypto = JS_NewObject(c);
    bind(crypto, "vrf_sign", JS_NewCFunction(c, js_vrf_sign, "vrf_sign", 3));
    bind(crypto, "sha256_hex", JS_NewCFunction(c, js_crypto_sha256_hex, "sha256_hex", 1));
    bind(crypto, "md5_hex", JS_NewCFunction(c, js_crypto_md5_hex, "md5_hex", 1));
    bind(crypto, "hmac_sha256_hex", JS_NewCFunction(c, js_crypto_hmac_sha256_hex, "hmac_sha256_hex", 2));
    bind(crypto, "aes_gcm_decrypt", JS_NewCFunction(c, js_crypto_aes_gcm_decrypt, "aes_gcm_decrypt", 4));
    bind(host, "crypto", crypto);

    // browser-host D5: host.browser.call(tool, args_json) -> string|null.
    JSValue browser_tbl = JS_NewObject(c);
    bind(browser_tbl, "call", JS_NewCFunction(c, js_browser_call, "call", 2));
    bind(host, "browser", browser_tbl);

    JSValue plugin_tbl = JS_NewObject(c);
    bind(plugin_tbl, "param", JS_NewCFunction(c, js_plugin_param, "param", 1));
    bind(host, "plugin", plugin_tbl);

    JSValue text_tbl = JS_NewObject(c);
    bind(text_tbl, "normalize_status", JS_NewCFunction(c, js_text_normalize_status, "normalize_status", 1));
    bind(text_tbl, "chapter_num", JS_NewCFunction(c, js_text_chapter_num, "chapter_num", 1));
    bind(text_tbl, "date_to_iso", JS_NewCFunction(c, js_text_date_to_iso, "date_to_iso", 1));
    bind(text_tbl, "trim", JS_NewCFunction(c, js_text_trim, "trim", 1));
    bind(text_tbl, "unescape", JS_NewCFunction(c, js_text_unescape, "unescape", 1));
    bind(text_tbl, "strip_html", JS_NewCFunction(c, js_text_strip_html, "strip_html", 1));
    bind(text_tbl, "clean", JS_NewCFunction(c, js_text_clean, "clean", 1));
    bind(text_tbl, "date_epoch", JS_NewCFunction(c, js_text_date_epoch, "date_epoch", 1));
    bind(text_tbl, "json_blob", JS_NewCFunction(c, js_text_json_blob, "json_blob", 2));
    bind(host, "text", text_tbl);

    JSValue codec_tbl = JS_NewObject(c);
    bind(codec_tbl, "base64_encode", JS_NewCFunction(c, js_codec_base64_encode, "base64_encode", 1));
    bind(codec_tbl, "base64_decode", JS_NewCFunction(c, js_codec_base64_decode, "base64_decode", 1));
    bind(codec_tbl, "hex_encode", JS_NewCFunction(c, js_codec_hex_encode, "hex_encode", 1));
    bind(codec_tbl, "hex_decode", JS_NewCFunction(c, js_codec_hex_decode, "hex_decode", 1));
    bind(host, "codecs", codec_tbl);

    bind(g, "host", host); // consumes the host reference tree

    JS_FreeValue(c, g);
}

} // namespace plugin::js
