/** \file bindings/storage.cpp
 *  \brief Per-plugin persistence: host.storage.get/set and host.config.get.
 */

#include "host_bindings.hpp"

#include "plugin/fs.hpp" // config.json read

#include <string>

namespace plugin::js::bindings {

JSValue js_storage_get(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "storage.get(key)");
    }
    Instance& inst = self(ctx);
    if (!inst.services.state_get) {
        return JS_NULL;
    }
    const auto v = inst.services.state_get(inst.id, arg_str(ctx, argv[0]));
    if (!v) {
        return JS_NULL;
    }
    return JS_NewStringLen(ctx, v->data(), v->size());
}

JSValue js_storage_set(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "storage.set(key, value)");
    }
    Instance& inst = self(ctx);
    if (inst.services.state_set) {
        inst.services.state_set(inst.id, arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
    }
    return JS_UNDEFINED;
}

// host.config.get: the plugin's own config.json (design D2, lua parity).
// Parsed per call -- the file is tiny and configs are read once per tick;
// absent file/key/parse -> null (lua nil), never an error.
JSValue js_config_get(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "config.get(key)");
    }
    const auto data = plugin::fs_read(self(ctx).dir, "config.json");
    if (!data) {
        return JS_NULL;
    }
    JSValue root = JS_ParseJSON(ctx, data->data(), data->size(), "<config>");
    if (JS_IsException(root)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        return JS_NULL;
    }
    const std::string key = arg_str(ctx, argv[0]);
    JSValue val = JS_GetPropertyStr(ctx, root, key.c_str());
    JS_FreeValue(ctx, root);
    if (JS_IsUndefined(val)) { // absent key: lua nil parity
        JS_FreeValue(ctx, val);
        return JS_NULL;
    }
    return val;
}

} // namespace plugin::js::bindings
