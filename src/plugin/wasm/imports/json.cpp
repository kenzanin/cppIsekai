/** \file imports/json.cpp
 *  \brief env.http_get_json / env.http_post_json -- fetch, require 200, require
 *  the body to parse as JSON (P5).
 */

#include "host_imports.hpp"

#include <optional>
#include <string>

namespace plugin::wasm::imports {
namespace {

// get_json/post_json core -- guests get the JSON text (wasm has no table type;
// same convention as the re2/html imports) or nullopt on any failure.
std::optional<std::string> http_json_body(Instance& self, const std::string& url, const std::string& opts) {
    if (!self.services.http_fetch) {
        return std::nullopt;
    }
    const auto reply = self.services.http_fetch(url, opts);
    if (!reply) {
        return std::nullopt;
    }
    glz::generic root;
    if (glz::read_json(root, reply->data())) {
        return std::nullopt;
    }
    const auto* obj = std::get_if<glz::generic::object_t>(&root.data);
    if (obj == nullptr) {
        return std::nullopt;
    }
    const auto sit = obj->find("status");
    const auto* sd = (sit != obj->end()) ? std::get_if<double>(&sit->second.data) : nullptr;
    if (sd == nullptr || *sd != 200.0) {
        return std::nullopt;
    }
    const auto bit = obj->find("body");
    const auto* bv = (bit != obj->end()) ? std::get_if<std::string>(&bit->second.data) : nullptr;
    if (bv == nullptr) {
        return std::nullopt;
    }
    glz::generic body_root;
    if (glz::read_json(body_root, bv->data())) {
        return std::nullopt; // get_json contract: parsed-or-nothing
    }
    return *bv;
}

} // namespace

const void* host_http_get_json(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t url_off = arg_i32(args);
    const uint32_t url_len = arg_i32(args);
    const uint32_t hdr_off = arg_i32(args);
    const uint32_t hdr_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto url = guest_str(self, url_off, url_len);
    const auto hdr = guest_str(self, hdr_off, hdr_len);
    if (url && hdr) {
        std::string opts = "{\"method\":\"GET\"}";
        if (!hdr->empty()) {
            opts.resize(opts.size() - 1);
            opts += ",\"headers\":" + *hdr + "}";
        }
        if (auto body = http_json_body(self, *url, opts)) {
            if (guest_write(self, out, cap, *body)) {
                rc = static_cast<int32_t>(body->size());
            } else {
                rc = kTruncated;
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_http_post_json(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t url_off = arg_i32(args);
    const uint32_t url_len = arg_i32(args);
    const uint32_t body_off = arg_i32(args);
    const uint32_t body_len = arg_i32(args);
    const uint32_t hdr_off = arg_i32(args);
    const uint32_t hdr_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto url = guest_str(self, url_off, url_len);
    const auto body = guest_str(self, body_off, body_len);
    const auto hdr = guest_str(self, hdr_off, hdr_len);
    if (url && body && hdr) {
        std::string opts = "{\"method\":\"POST\"";
        if (!body->empty()) {
            std::string esc;
            if (auto w = glz::write_json(*body)) {
                esc = std::move(*w); // quoted, escaped JSON string
            }
            if (!esc.empty()) {
                opts += ",\"body\":" + esc;
            }
        }
        if (!hdr->empty()) {
            opts += ",\"headers\":" + *hdr;
        }
        opts += '}';
        if (auto res = http_json_body(self, *url, opts)) {
            if (guest_write(self, out, cap, *res)) {
                rc = static_cast<int32_t>(res->size());
            } else {
                rc = kTruncated;
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

} // namespace plugin::wasm::imports
