/** \file bindings/crypto.cpp
 *  \brief host.crypto.* -- hash/hmac/aes cores plus vrf_sign.
 */

#include "host_bindings.hpp"

#include "plugin/host_crypto.hpp"
#include "plugin/vrf.hpp" // plugin::vrf_sign -- the host.crypto body

#include <glaze/glaze.hpp>

#include <spdlog/spdlog.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace plugin::js::bindings {

// host.crypto.vrf_sign: the shared core (plugin/vrf.hpp) behind lua's
// host.crypto (design D3). Signature string, or null with the reason logged
// tagged by plugin id. Params carry string values only -- anything else takes
// the error path (design: no silent String() second contract); stages are
// {iv 0-255, key b64, tbl b64} as in tests/test_vrf.cpp.
JSValue js_vrf_sign(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 3 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "crypto.vrf_sign(api_path, params, stages)");
    }
    const Instance& inst = self(ctx);
    const std::string api_path = arg_str(ctx, argv[0]);
    const auto fail = [&](const std::string& why) {
        spdlog::warn("[plugin:{}] vrf_sign {} failed: {}", inst.id, api_path, why);
        return JS_NULL;
    };
    if (api_path.empty()) {
        return fail("vrf_sign: api_path is empty");
    }

    std::map<std::string, std::string> prm;
    if (JS_IsObject(argv[1])) { // absent/non-object: empty, lua optional-table parity
        glz::generic root;
        if (auto ec = glz::read_json(root, json_stringify(ctx, argv[1]))) {
            return fail("vrf_sign: params must be an object");
        }
        const auto* obj = std::get_if<glz::generic::object_t>(&root.data);
        if (obj == nullptr) {
            return fail("vrf_sign: params must be an object");
        }
        for (const auto& [k, v] : *obj) {
            if (const auto* s = std::get_if<std::string>(&v.data)) {
                prm.emplace(k, *s);
            } else {
                return fail("vrf_sign: params[\"" + k + "\"] must be a string");
            }
        }
    }

    std::vector<VrfStage> vstages;
    {
        glz::generic root;
        if (auto ec = glz::read_json(root, json_stringify(ctx, argv[2]))) {
            return fail("vrf_sign: stages must be a non-empty array");
        }
        const auto* arr = std::get_if<glz::generic::array_t>(&root.data);
        if ((arr == nullptr) || arr->empty()) {
            return fail("vrf_sign: stages must be a non-empty array");
        }
        size_t idx = 0;
        for (const auto& el : *arr) {
            ++idx;
            const auto* so = std::get_if<glz::generic::object_t>(&el.data);
            if (so == nullptr) {
                return fail("vrf_sign: stage " + std::to_string(idx) + ": missing");
            }
            const auto* const iv = so->find("iv");
            const auto* const key = so->find("key");
            const auto* const tbl = so->find("tbl");
            if (iv == so->end() || key == so->end() || tbl == so->end()) {
                return fail("vrf_sign: stage " + std::to_string(idx) + ": needs iv, key, tbl");
            }
            const auto* ivd = std::get_if<double>(&iv->second.data);
            if ((ivd == nullptr) || *ivd < 0 || *ivd > 255 || *ivd != static_cast<double>(static_cast<int>(*ivd))) {
                return fail("vrf_sign: stage " + std::to_string(idx) + ": iv must be integer 0-255");
            }
            const auto* k = std::get_if<std::string>(&key->second.data);
            const auto* t = std::get_if<std::string>(&tbl->second.data);
            if ((k == nullptr) || (t == nullptr)) {
                return fail("vrf_sign: stage " + std::to_string(idx) + ": needs iv, key, tbl");
            }
            vstages.push_back({.iv = static_cast<uint8_t>(*ivd), .key = *k, .tbl = *t});
        }
    }

    std::string err;
    const auto out = vrf_sign(api_path, prm, vstages, err);
    if (!out) {
        return fail(err);
    }
    return JS_NewStringLen(ctx, out->data(), out->size());
}

// host.crypto (P7): hex-in/hex-out; null on tag/hex/key failure.
JSValue js_crypto_sha256_hex(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "crypto.sha256_hex(string)");
    }
    const std::string out = plugin::host_crypto::sha256_hex(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_crypto_md5_hex(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "crypto.md5_hex(string)");
    }
    const std::string out = plugin::host_crypto::md5_hex(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_crypto_hmac_sha256_hex(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_ThrowTypeError(ctx, "crypto.hmac_sha256_hex(data, key)");
    }
    const std::string out = plugin::host_crypto::hmac_sha256_hex(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_crypto_aes_gcm_decrypt(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 4 || !JS_IsString(argv[0]) || !JS_IsString(argv[1]) || !JS_IsString(argv[2]) ||
        !JS_IsString(argv[3])) {
        return JS_ThrowTypeError(ctx, "crypto.aes_gcm_decrypt(iv, ct, tag, key)");
    }
    const auto pt = plugin::host_crypto::aes_gcm_decrypt(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]),
                                                         arg_str(ctx, argv[2]), arg_str(ctx, argv[3]));
    if (!pt) {
        return JS_NULL;
    }
    return JS_NewStringLen(ctx, pt->data(), pt->size());
}

} // namespace plugin::js::bindings
