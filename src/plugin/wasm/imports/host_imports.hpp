#pragma once

/** \file imports/host_imports.hpp
 *  \brief env.* import handler declarations, one topic TU per domain;
 *  wasm_imports.cpp owns the link table (names + order) and nothing else.
 */

#include "../wasm_internal.hpp"

#include <cstdint>

namespace plugin::wasm::imports {

// Slot convention per m3_core.h (m3ApiReturnType/GetArg): the return slot sits
// at _sp[0], arguments after it, one 8-byte cell each; an i32 occupies the low
// half. Hand-rolled rather than via the m3Api macros so the unused
// runtime/mem params stay nameless (no -Wextra noise).
inline uint32_t arg_i32(uint64_t*& sp) {
    return static_cast<uint32_t>(*sp++);
}

const void* host_now(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_log_info(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_fs_read(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_storage_get(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_storage_set(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_plugin_param(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_http_fetch(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_http_fetch_body(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_http_get_json(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_http_post_json(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_re2_match(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_re2_find(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_re2_find_all(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_re2_replace(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_re2_quote(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_re2_find_index(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_html_select(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_html_attr(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_html_xpath(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_url_encode(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_url_decode(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_text_normalize_status(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_text_chapter_num(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_text_date_to_iso(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_text_trim(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_text_unescape(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_text_strip_html(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_codec_base64_encode(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_codec_base64_decode(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_codec_hex_encode(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_codec_hex_decode(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_crypto_sha256_hex(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_crypto_md5_hex(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_crypto_hmac_sha256_hex(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_crypto_aes_gcm_decrypt(IM3Runtime, IM3ImportContext, uint64_t*, void*);
const void* host_browser_call(IM3Runtime, IM3ImportContext, uint64_t*, void*);

} // namespace plugin::wasm::imports
