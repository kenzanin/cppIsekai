#include "enrich_internal.hpp"

namespace http::api {


// Serve the per-source entry of the stored `fetched` array shaped as the
// enrichment envelope; nullopt when this source has nothing stored.
 std::optional<crow::json::wvalue> stored_source(const std::string& fetched_json, const std::string& id,
                                                       const std::string& kind, std::int64_t& out_fetched_at) {
    glz::generic fetched;
    if (glz::read_json(fetched, fetched_json) || !fetched.is_array()) {
        return std::nullopt;
    }
    for (const auto& entry : fetched.get_array()) {
        if (!entry.is_object()) {
            continue;
        }
        const auto& obj = entry.get_object();
        const auto* const src = obj.find("source");
        if (src == obj.end()) {
            continue;
        }
        const auto* s = std::get_if<std::string>(&src->second.data);
        if (s == nullptr || *s != id) {
            continue;
        }
        // entries are keyed (source, kind); a missing kind is a legacy detail entry
        const auto* const k = obj.find("kind");
        const auto* ks = k != obj.end() ? std::get_if<std::string>(&k->second.data) : nullptr;
        if (((ks != nullptr) ? *ks : std::string("detail")) != kind) {
            continue;
        }
        crow::json::wvalue out;
        out["ok"] = true;
        out["cached"] = true;
        out["source"] = id;
        const auto* const at = obj.find("fetched_at");
        double at_d = 0;
        if (at != obj.end()) {
            if (const auto* d = std::get_if<double>(&at->second.data)) {
                at_d = *d;
            }
        }
        out_fetched_at = static_cast<std::int64_t>(at_d);
        out["fetched_at"] = at_d;
        const auto* const data = obj.find("data");
        out["data"] = data != obj.end() ? crow::json::load(glz::write_json(data->second).value_or(std::string{}))
                                        : crow::json::wvalue();
        return out;
    }
    return std::nullopt;
}



// Validate + normalize the POST chosen member into a compact JSON string.
// nullopt on any shape violation (caller returns 400). Exact key whitelist;
// artist/author accept String or List<String> and normalize to array.
 std::optional<std::string> normalize_chosen(const crow::json::rvalue& ch) {
    if (ch.t() != crow::json::type::Object) {
        return std::nullopt;
    }
    static const char* allowed[] = {
        "title", "synopsis", "artist", "author", "cover", "tags_add", "tags_hidden", "related_add",
    };
    for (const auto& member : ch) {
        const std::string k = std::string(member.key());
        bool ok = false;
        for (const char* a : allowed) {
            if (k == a) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            return std::nullopt;
        }
    }
    crow::json::wvalue out;
    for (const char* key : {"title", "synopsis", "cover"}) {
        if (ch.has(key) && ch[key].t() == crow::json::type::String) {
            out[key] = ch[key];
        }
    }
    for (const char* key : {"artist", "author"}) {
        if (!ch.has(key)) {
            continue;
        }
        if (ch[key].t() == crow::json::type::String) {
            crow::json::wvalue one(crow::json::type::List);
            one[0] = ch[key];
            out[key] = std::move(one);
        } else if (ch[key].t() == crow::json::type::List) {
            crow::json::wvalue arr(crow::json::type::List);
            int n = 0;
            for (const auto& item : ch[key]) {
                if (item.t() != crow::json::type::String) {
                    return std::nullopt;
                }
                arr[n++] = item;
            }
            out[key] = std::move(arr);
        } else {
            {
                return std::nullopt;
            }
        }
    }
    if (ch.has("related_add")) {
        const auto& arr = ch["related_add"];
        if (arr.t() != crow::json::type::List) {
            return std::nullopt;
        }
        crow::json::wvalue out_arr(crow::json::type::List);
        int n = 0;
        for (const auto& item : arr) {
            if (item.t() != crow::json::type::Object) {
                return std::nullopt;
            }
            // extra keys rejected; name/url required non-empty, id optional non-empty string
            for (const auto& member : item) {
                const std::string k = std::string(member.key());
                if (k != "name" && k != "url" && k != "id") {
                    return std::nullopt;
                }
            }
            if (!item.has("name") || !item.has("url")) {
                return std::nullopt;
            }
            crow::json::wvalue row;
            for (const char* key : {"name", "url", "id"}) {
                if (!item.has(key)) {
                    continue;
                }
                if (item[key].t() != crow::json::type::String) {
                    return std::nullopt;
                }
                const std::string v = item[key].s();
                if (v.empty() || v.size() > 512) {
                    return std::nullopt;
                }
                row[key] = v;
            }
            out_arr[n++] = std::move(row);
        }
        out["related_add"] = std::move(out_arr);
    }
    for (const char* key : {"tags_add", "tags_hidden"}) {
        if (!ch.has(key)) {
            continue;
        }
        const auto& arr = ch[key];
        if (arr.t() != crow::json::type::List) {
            return std::nullopt;
        }
        crow::json::wvalue out_arr(crow::json::type::List);
        int n = 0;
        for (const auto& item : arr) {
            if (item.t() != crow::json::type::String) {
                return std::nullopt;
            }
            out_arr[n++] = item;
        }
        out[key] = std::move(out_arr);
    }
    return std::string(out.dump());
}

// Chosen display title from title_meta.chosen; nullopt when unset/empty.

} // namespace http::api
