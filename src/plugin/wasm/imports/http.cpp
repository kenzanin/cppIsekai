/** \file imports/http.cpp
 *  \brief env.http_fetch / env.http_fetch_body -- raw reply text.
 */

#include "host_imports.hpp"

#include <cstdint>
#include <string>

namespace plugin::wasm::imports {

const void* host_http_fetch(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t url_off = arg_i32(args);
    const uint32_t url_len = arg_i32(args);
    const uint32_t opts_off = arg_i32(args);
    const uint32_t opts_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto url = guest_str(self, url_off, url_len);
    const auto opts = guest_str(self, opts_off, opts_len);
    if (url && opts && self.services.http_fetch) {
        if (auto reply = self.services.http_fetch(*url, *opts)) {
            if (guest_write(self, out, cap, *reply)) {
                rc = static_cast<int32_t>(reply->size());
            } else {
                rc = kTruncated;
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_http_fetch_body(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t url_off = arg_i32(args);
    const uint32_t url_len = arg_i32(args);
    const uint32_t opts_off = arg_i32(args);
    const uint32_t opts_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto url = guest_str(self, url_off, url_len)) {
        if (const auto opts = guest_str(self, opts_off, opts_len)) {
            if (self.services.http_fetch) {
                if (auto reply = self.services.http_fetch(*url, *opts)) {
                    // Parse status from the fetch reply JSON; write body on 200, else kNotFound.
                    glz::generic root;
                    if (glz::read_json(root, reply->data())) {
                        rc = kNotFound;
                    } else {
                        const auto* obj = std::get_if<glz::generic::object_t>(&root.data);
                        if (obj == nullptr) {
                            rc = kNotFound;
                        } else {
                            auto sit = obj->find("status");
                            const auto* bs = (sit == obj->end()) ? nullptr : std::get_if<double>(&sit->second.data);
                            const bool is_200 = (bs != nullptr && *bs == 200.0);
                            if (!is_200) {
                                rc = kNotFound;
                            } else {
                                auto bit = obj->find("body");
                                const auto* bv = (bit == obj->end()) ? nullptr : std::get_if<std::string>(&bit->second.data);
                                if (bv == nullptr) {
                                    rc = kNotFound;
                                } else if (guest_write(self, out, cap, *bv)) {
                                    rc = static_cast<int32_t>(bv->size());
                                } else {
                                    rc = kTruncated;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

} // namespace plugin::wasm::imports
