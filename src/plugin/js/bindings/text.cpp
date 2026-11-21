/** \file bindings/text.cpp
 *  \brief host.text.* / host.codecs.* / host.url_* -- string-in, string-out
 *  marshals over the shared cores.
 */

#include "host_bindings.hpp"

#include "plugin/host_codecs.hpp"
#include "plugin/host_text.hpp"
#include "plugin/url_codec.hpp" // plugin::url_{encode,decode} -- host.url_* body

#include <string>

namespace plugin::js::bindings {

// host.text: status normalization, chapter-number extraction, date→ISO
// (P2, host_text.hpp — one core, three thin marshals; design D4).
JSValue js_text_normalize_status(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "text.normalize_status(string)");
    }
    const std::string out = plugin::host_text::normalize_status(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_text_chapter_num(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "text.chapter_num(string)");
    }
    const auto v = plugin::host_text::chapter_num(arg_str(ctx, argv[0]));
    if (!v) {
        return JS_NULL; // parity: lua nil on no number
    }
    return JS_NewFloat64(ctx, *v);
}

JSValue js_text_date_to_iso(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "text.date_to_iso(string)");
    }
    const auto v = plugin::host_text::date_to_iso(arg_str(ctx, argv[0]));
    if (!v) {
        return JS_NULL; // parity: lua nil on unparseable
    }
    return JS_NewStringLen(ctx, v->data(), v->size());
}

JSValue js_text_trim(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "text.trim(string)");
    }
    const std::string out = plugin::host_text::trim(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_text_unescape(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "text.unescape(string)");
    }
    const std::string out = plugin::host_text::unescape(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_text_strip_html(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "text.strip_html(string)");
    }
    const std::string out = plugin::host_text::strip_html(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_text_clean(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "text.clean(string)");
    }
    const std::string out = plugin::host_text::clean(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

// plugin-host-promotion D5/D7 parity.
JSValue js_text_date_epoch(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "text.date_epoch(string)");
    }
    const auto v = plugin::host_text::date_epoch(arg_str(ctx, argv[0]));
    if (!v) {
        return JS_NULL; // parity: lua nil on unparseable
    }
    return JS_NewInt64(ctx, *v);
}

JSValue js_text_json_blob(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_ThrowTypeError(ctx, "text.json_blob(text, marker)");
    }
    const auto v = plugin::host_text::json_blob(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
    if (!v) {
        return JS_NULL;
    }
    return JS_NewStringLen(ctx, v->data(), v->size());
}

// host.codecs (P6): string-in/string-out, decode null on invalid.
JSValue js_codec_base64_encode(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "codecs.base64_encode(string)");
    }
    const std::string out = plugin::host_codec::base64_encode(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_codec_base64_decode(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "codecs.base64_decode(string)");
    }
    const auto v = plugin::host_codec::base64_decode(arg_str(ctx, argv[0]));
    if (!v) {
        return JS_NULL;
    }
    return JS_NewStringLen(ctx, v->data(), v->size());
}

JSValue js_codec_hex_encode(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "codecs.hex_encode(string)");
    }
    const std::string out = plugin::host_codec::hex_encode(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_codec_hex_decode(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "codecs.hex_decode(string)");
    }
    const auto v = plugin::host_codec::hex_decode(arg_str(ctx, argv[0]));
    if (!v) {
        return JS_NULL;
    }
    return JS_NewStringLen(ctx, v->data(), v->size());
}

// host.url_*: one shared codec core (plugin/url_codec.hpp), thin marshal.
// Strings only, exactly one arg; anything else is a TypeError (host-surface
// parity decision, design of js-wasm-host-parity).
JSValue js_url_encode(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "url_encode(string)");
    }
    const std::string out = plugin::url_encode(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_url_decode(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "url_decode(string)");
    }
    const std::string out = plugin::url_decode(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

// plugin-host-promotion D6: madaradex semantics, shared core url_codec.hpp.
JSValue js_url_abs(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_ThrowTypeError(ctx, "url_abs(base, ref)");
    }
    const std::string out = plugin::url_abs(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

} // namespace plugin::js::bindings
