#pragma once

/** \file bindings/host_bindings.hpp
 *  \brief host.* handler declarations, one topic TU per domain; js_bindings.cpp
 *  owns the registration table (property names + order) and nothing else.
 */

#include "../js_internal.hpp"

namespace plugin::js::bindings {

/** \brief The instance behind a realm (runtime opaque). */
inline Instance& self(JSContext* ctx) {
    return *static_cast<Instance*>(JS_GetRuntimeOpaque(JS_GetRuntime(ctx)));
}

JSValue js_now(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
JSValue js_require(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_fs_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_plugin_param(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

JSValue js_http_fetch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_fetch_body(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_get_json(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_post_json(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

JSValue js_text_normalize_status(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_text_chapter_num(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_text_date_to_iso(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_text_trim(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_text_unescape(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_text_strip_html(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_text_date_epoch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_text_json_blob(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

JSValue js_codec_base64_encode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_codec_base64_decode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_codec_hex_encode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_codec_hex_decode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

JSValue js_url_encode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_url_decode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_url_abs(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

JSValue js_re2_match(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_re2_find(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_re2_find_all(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_re2_replace(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_re2_quote(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_re2_find_index(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_re2_capture(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

JSValue js_html_select(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_html_attr(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_html_xpath(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

JSValue js_storage_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_storage_set(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_config_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

JSValue js_vrf_sign(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_sha256_hex(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_md5_hex(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_hmac_sha256_hex(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_aes_gcm_decrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

JSValue js_browser_call(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

} // namespace plugin::js::bindings
