/** \file bindings/http.cpp
 *  \brief host.http.* -- fetch / fetch_body / get_json / post_json.
 */

#include "host_bindings.hpp"

#include <glaze/glaze.hpp>

#include <string>

namespace plugin::js::bindings {

JSValue js_http_fetch(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "http.fetch(url, opts)");
    }
    Instance& inst = self(ctx);
    if (!inst.services.http_fetch) {
        return JS_ThrowTypeError(ctx, "http unavailable");
    }
    const std::string url = arg_str(ctx, argv[0]);
    std::string opts = "{}";
    if (argc > 1 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        opts = json_stringify(ctx, argv[1]);
    }
    const auto res = inst.services.http_fetch(url, opts);
    if (!res) {
        return JS_NULL; // parity: lua nil on failure
    }
    JSValue parsed = JS_ParseJSON(ctx, res->data(), res->size(), "<fetch>");
    if (JS_IsException(parsed)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        return JS_NULL;
    }
    return parsed; // {status, body, headers} -- the lua-shaped reply
}

JSValue js_http_fetch_body(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "http.fetch_body(url, opts)");
    }
    Instance& inst = self(ctx);
    if (!inst.services.http_fetch) {
        return JS_ThrowTypeError(ctx, "http unavailable");
    }
    const std::string url = arg_str(ctx, argv[0]);
    std::string opts = "{}";
    if (argc > 1 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        opts = json_stringify(ctx, argv[1]);
    }
    const auto res = inst.services.http_fetch(url, opts);
    if (!res) {
        return JS_NULL;
    }
    glz::generic root;
    if (auto ec = glz::read_json(root, res->data())) {
        return JS_NULL;
    }
    const auto* obj = std::get_if<glz::generic::object_t>(&root.data);
    if (obj == nullptr) {
        return JS_NULL;
    }
    const auto* status_it = obj->find("status");
    if (status_it == obj->end()) {
        return JS_NULL;
    }
    const auto* sv = std::get_if<double>(&status_it->second.data);
    if (sv == nullptr || *sv != 200.0) {
        return JS_NULL;
    }
    const auto* body_it = obj->find("body");
    if (body_it == obj->end()) {
        return JS_NULL;
    }
    const auto* bs = std::get_if<std::string>(&body_it->second.data);
    if (bs == nullptr) {
        return JS_NULL;
    }
    return JS_NewStringLen(ctx, bs->data(), bs->size());
}

// P5: get_json/post_json -- fetch + JS_ParseJSON of the body; null on any
// failure (transport, non-200, malformed JSON). opts envelope as in fetch.
JSValue js_http_json_call(JSContext* ctx, JSValueConst* argv, int argc, bool post) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, post ? "http.post_json(url, body, headers)" : "http.get_json(url, headers)");
    }
    Instance& inst = self(ctx);
    if (!inst.services.http_fetch) {
        return JS_NULL;
    }
    const std::string url = arg_str(ctx, argv[0]);
    std::string opts = post ? "{\"method\":\"POST\"}" : "{}";
    if (post) {
        if (argc > 1 && JS_IsString(argv[1]) && !arg_str(ctx, argv[1]).empty()) {
            JSValue s = JS_NewStringLen(ctx, arg_str(ctx, argv[1]).data(), arg_str(ctx, argv[1]).size());
            const std::string enc = json_stringify(ctx, s);
            JS_FreeValue(ctx, s);
            opts.resize(opts.size() - 1); // drop trailing }
            opts += ",\"body\":" + enc + "}";
        }
        if (argc > 2 && JS_IsObject(argv[2])) {
            opts.resize(opts.size() - 1);
            opts += ",\"headers\":" + json_stringify(ctx, argv[2]) + "}";
        }
    } else if (argc > 1 && JS_IsObject(argv[1])) {
        // plugin-host-promotion: a table carrying known opt keys is the full
        // fetch envelope (parity with lua get_json); anything else is the
        // legacy flat headers map.
        static const char* kKnown[] = {"timeout_ms", "profile", "method", "body",
                                       "accept",     "user_agent", "cookie", "headers"};
        bool has_known = false;
        for (const char* k : kKnown) {
            JSValue v = JS_GetPropertyStr(ctx, argv[1], k);
            const bool present = !JS_IsUndefined(v);
            JS_FreeValue(ctx, v);
            if (present) {
                has_known = true;
                break;
            }
        }
        if (has_known) {
            opts = json_stringify(ctx, argv[1]);
        } else {
            opts = "{\"headers\":" + json_stringify(ctx, argv[1]) + "}";
        }
    }
    const auto res = inst.services.http_fetch(url, opts);
    if (!res) {
        return JS_NULL;
    }
    glz::generic root;
    if (auto ec = glz::read_json(root, res->data())) {
        return JS_NULL;
    }
    const auto* obj = std::get_if<glz::generic::object_t>(&root.data);
    if (obj == nullptr) {
        return JS_NULL;
    }
    const auto* sit = obj->find("status");
    const auto* sd = (sit != obj->end()) ? std::get_if<double>(&sit->second.data) : nullptr;
    if (sd == nullptr || *sd != 200.0) {
        return JS_NULL;
    }
    const auto* bit = obj->find("body");
    const auto* bs = (bit != obj->end()) ? std::get_if<std::string>(&bit->second.data) : nullptr;
    if (bs == nullptr) {
        return JS_NULL;
    }
    JSValue parsed = JS_ParseJSON(ctx, bs->data(), bs->size(), "<json>");
    if (JS_IsException(parsed)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        return JS_NULL;
    }
    return parsed;
}

JSValue js_http_get_json(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    return js_http_json_call(ctx, argv, argc, /*post=*/false);
}

JSValue js_http_post_json(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    return js_http_json_call(ctx, argv, argc, /*post=*/true);
}

} // namespace plugin::js::bindings
