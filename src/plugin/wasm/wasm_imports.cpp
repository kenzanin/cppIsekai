#include "wasm_internal.hpp"
#include "wasm_rt.hpp"

#include "plugin/fs.hpp"
#include "plugin/host_text.hpp"
#include "plugin/htmltext.hpp"
#include "plugin/url_codec.hpp" // plugin::url_{encode,decode} -- host.url_* body

#include <spdlog/spdlog.h>

#include <bit>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace plugin::wasm {
namespace {
// ---------------------------------------------------------------------------
// env.* imports. Slot convention per m3_core.h (m3ApiReturnType/GetArg): the
// return slot sits at _sp[0], arguments after it, one 8-byte cell each; an
// i32 occupies the low half. Hand-rolled rather than via the m3Api macros so
// the unused runtime/mem params stay nameless (no -Wextra noise).
// ---------------------------------------------------------------------------

uint32_t arg_i32(uint64_t*& sp) {
    return static_cast<uint32_t>(*sp++);
}

const void* host_now(IM3Runtime, IM3ImportContext, uint64_t* sp, void*) {
    *reinterpret_cast<int64_t*>(sp) = static_cast<int64_t>(std::time(nullptr));
    return m3Err_none;
}

const void* host_log_info(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    const uint32_t off = arg_i32(sp);
    const uint32_t len = arg_i32(sp);
    const auto msg = guest_str(self, off, len);
    if (!msg) {
        return m3Err_trapOutOfBoundsMemoryAccess;
    }
    // Same route as lua log.info: default logger, whose sinks feed the log page.
    spdlog::info("[plugin:{}] {}", self.id, *msg);
    return m3Err_none;
}

const void* host_fs_read(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1; // over the i32 return slot
    const uint32_t rel_off = arg_i32(args);
    const uint32_t rel_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto rel = guest_str(self, rel_off, rel_len)) {
        if (auto content = plugin::fs_read(self.dir, *rel)) {
            if (guest_write(self, out, cap, *content)) {
                rc = static_cast<int32_t>(content->size());
            } else {
                rc = kTruncated;
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_storage_get(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t key_off = arg_i32(args);
    const uint32_t key_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto key = guest_str(self, key_off, key_len)) {
        std::optional<std::string> v;
        if (self.services.state_get) {
            v = self.services.state_get(self.id, *key);
        }
        if (v && guest_write(self, out, cap, *v)) {
            rc = static_cast<int32_t>(v->size());
        } else if (v) {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_storage_set(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    const uint32_t key_off = arg_i32(sp); // void import: no return slot
    const uint32_t key_len = arg_i32(sp);
    const uint32_t val_off = arg_i32(sp);
    const uint32_t val_len = arg_i32(sp);

    const auto key = guest_str(self, key_off, key_len);
    const auto val = guest_str(self, val_off, val_len);
    if (!key || !val) {
        return m3Err_trapOutOfBoundsMemoryAccess;
    }
    if (self.services.state_set) {
        self.services.state_set(self.id, *key, *val);
    }
    return m3Err_none;
}

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

const void* host_plugin_param(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t key_off = arg_i32(args);
    const uint32_t key_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto key = guest_str(self, key_off, key_len)) {
        const auto it = self.services.params.find(*key);
        if (it != self.services.params.end() && guest_write(self, out, cap, it->second)) {
            rc = static_cast<int32_t>(it->second.size());
        } else if (it != self.services.params.end()) {
            rc = kTruncated;
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

struct Import {
    const char* name;
    const char* sig; // m3 link notation, m3_core.h ConvertTypeCharToTypeId
    M3RawCall fn;
};

// host.re2/html: one core (plugin::htmltext), results as JSON arrays written
// into guest memory via the same guest_write convention as fs_read/storage.
const void* host_re2_call(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*, bool full) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t pat_off = arg_i32(args);
    const uint32_t pat_len = arg_i32(args);
    const uint32_t txt_off = arg_i32(args);
    const uint32_t txt_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto pat = guest_str(self, pat_off, pat_len);
    const auto txt = guest_str(self, txt_off, txt_len);
    if (pat && txt) {
        try {
            const auto groups = full ? plugin::re2_match(*pat, *txt) : plugin::re2_find(*pat, *txt);
            // JSON array of groups; empty list on no-match (never an error).
            std::string json = "[";
            for (size_t i = 0; i < groups.size(); ++i) {
                if (i != 0) {
                    json += ',';
                }
                json += '"';
                for (const char c : groups[i]) {
                    if (c == '"' || c == '\\') {
                        json += '\\';
                    }
                    json += c;
                }
                json += '"';
            }
            json += ']';
            if (guest_write(self, out, cap, json)) {
                rc = static_cast<int32_t>(json.size());
            } else {
                rc = kTruncated;
            }
        } catch (const std::invalid_argument&) {
            rc = kBadPattern; // loud: bad pattern is the plugin's bug
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_re2_match(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return host_re2_call(rt, ctx, sp, mem, /*full=*/true);
}

const void* host_re2_find(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return host_re2_call(rt, ctx, sp, mem, /*full=*/false);
}

const void* host_html_call(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*, bool want_attr) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t html_off = arg_i32(args);
    const uint32_t html_len = arg_i32(args);
    const uint32_t sel_off = arg_i32(args);
    const uint32_t sel_len = arg_i32(args);
    uint32_t attr_off = 0, attr_len = 0;
    if (want_attr) {
        attr_off = arg_i32(args);
        attr_len = arg_i32(args);
    }
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto html = guest_str(self, html_off, html_len);
    const auto sel = guest_str(self, sel_off, sel_len);
    const auto attr = want_attr ? guest_str(self, attr_off, attr_len) : std::optional<std::string>{};
    if (html && sel && (!want_attr || attr)) {
        // JSON string array; malformed HTML / no match => [] (lenient).
        const auto vals = want_attr ? plugin::html_attr(*html, *sel, *attr) : plugin::html_select(*html, *sel);
        std::string json = "[";
        for (size_t i = 0; i < vals.size(); ++i) {
            if (i != 0) {
                json += ',';
            }
            json += '"';
            for (const char c : vals[i]) {
                if (c == '"' || c == '\\') {
                    json += '\\';
                }
                json += c;
            }
            json += '"';
        }
        json += ']';
        if (guest_write(self, out, cap, json)) {
            rc = static_cast<int32_t>(json.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_html_select(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return host_html_call(rt, ctx, sp, mem, /*want_attr=*/false);
}

const void* host_html_attr(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return host_html_call(rt, ctx, sp, mem, /*want_attr=*/true);
}

// host.url_*: the shared query-component codec (plugin/url_codec.hpp), raw
// slot layout (in_off,in_len,out,cap) -> length; kNotFound on a bad guest
// window, kTruncated when the result exceeds cap.
const void* host_url_encode(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::url_encode(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_url_decode(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::url_decode(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

// host.text: status normalization, chapter-number extraction, date→ISO
// (P2, host_text.hpp — one core, three thin marshals; design D4).
const void* host_text_normalize_status(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_text::normalize_status(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_text_chapter_num(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);

    // Return slot: i64 (f64 bitcast). Write 0.0f when no number / volume-only.
    int64_t f64_bits = 0;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const auto v = plugin::host_text::chapter_num(*in);
        if (v) {
            const double d = *v;
            const auto bits = std::bit_cast<int64_t>(d);
            f64_bits = bits;
        }
        // No number / volume-only → 0.0f (bits=0) — parity with Go's 0.0.
    }
    *reinterpret_cast<int64_t*>(sp) = f64_bits;
    return m3Err_none;
}

const void* host_text_date_to_iso(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        if (const auto res = plugin::host_text::date_to_iso(*in)) {
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

const void* host_text_trim(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_text::trim(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_text_unescape(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_text::unescape(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_text_strip_html(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_text::strip_html(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

constexpr std::array kImports{
    Import{"now", "I", host_now},
    Import{"log_info", "v(ii)", host_log_info},
    Import{"fs_read", "i(iiii)", host_fs_read},
    Import{"storage_get", "i(iiii)", host_storage_get},
    Import{"storage_set", "v(iiii)", host_storage_set},
    Import{"http_fetch", "i(iiiiii)", host_http_fetch},
    Import{"http_fetch_body", "i(iiiiii)", host_http_fetch_body},
    Import{"plugin_param", "i(iiii)", host_plugin_param},
    Import{"re2_match", "i(iiiiii)", host_re2_match},
    Import{"re2_find", "i(iiiiii)", host_re2_find},
    Import{"html_select", "i(iiiii)", host_html_select},
    Import{"html_attr", "i(iiiiiii)", host_html_attr},
    Import{"url_encode", "i(iiii)", host_url_encode},
    Import{"url_decode", "i(iiii)", host_url_decode},
    Import{"text_normalize_status", "i(iiii)", host_text_normalize_status},
    Import{"text_chapter_num", "I(ii)", host_text_chapter_num},
    Import{"text_date_to_iso", "i(iiii)", host_text_date_to_iso},
    Import{"text_trim", "i(iiii)", host_text_trim},
    Import{"text_unescape", "i(iiii)", host_text_unescape},
    Import{"text_strip_html", "i(iiii)", host_text_strip_html},
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
