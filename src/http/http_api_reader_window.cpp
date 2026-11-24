#include "api_ctx.hpp"

#include <algorithm>
#include <optional>

namespace http::api {


// The reader window composition (image-cache): chapter list + pages through
// the cached ABI path (stale-serve included), skip flags from plugin_state,
// math in reader::window_parts. Empty result on any plugin failure -- the
// reader merely loses its prefetch. The worker (task 3.3) mirrors this shape.
// Declared in api_ctx.hpp; the state TU reuses it for the skip-rerun.
std::vector<reader::WindowPart> reader_window(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                                              plugin::Host& plugins, const std::string& plugin_id,
                                              const std::string& manga_id, const reader::Position& pos) {
    crow::json::wvalue query;
    query["id"] = manga_id;
    const std::string chapters_query = query.dump();

    const std::string envelope = on_pool(plugin_pool, [&] {
        return call_cached(store, plugin_id, "chapters", chapters_query,
                           [&] { return plugins.call(plugin_id, "chapters", chapters_query); }, &plugins);
    });

    std::vector<reader::Chapter> chapters;
    if (const auto data = envelope_data(envelope)) {
        for (const auto& item : *data) {
            if (!item.is_object()) {
                continue;
            }
            const auto& entry = item.get_object();
            const auto* const id = entry.find("id");
            const auto* id_s = id == entry.end() ? nullptr : std::get_if<std::string>(&id->second.data);
            if (id_s == nullptr) {
                continue;
            }
            double number = 0; // fractional chapter numbers are a thing
            if (const auto* const num = entry.find("number"); num != entry.end()) {
                if (const auto* d = std::get_if<double>(&num->second.data)) {
                    number = *d;
                }
            }
            chapters.push_back(reader::Chapter{.id = *id_s, .number = number});
        }
    }
    if (chapters.empty()) {
        spdlog::warn("reader: no chapters for plugin='{}' manga='{}'", plugin_id, manga_id);
        return {};
    }

    reader::Skipped skip;
    on_pool(db, [&] {
        // chapter-store 3.4: skip lives in the chapters.flags bitfield now
        // (bit1); the legacy host/skip keys ride in via chapters_upsert (D2).
        for (const auto& [ch_id, flags] : store.chapter_flags_list(plugin_id, manga_id)) {
            skip[ch_id] = (flags & std::to_underlying(ChapterFlag::kSkip)) != 0;
        }
        return 0; // on_pool is value-based
    });

    const auto pages_of = [&](const std::string& chapter_id) {
        crow::json::wvalue q;
        q["id"] = chapter_id;
        const std::string pages_query = q.dump();
        const std::string env = on_pool(plugin_pool, [&] {
            return call_cached(store, plugin_id, "pages", pages_query,
                               [&] { return plugins.call(plugin_id, "pages", pages_query); }, &plugins);
        });
        std::vector<std::string> urls;
        if (const auto data = envelope_data(env)) {
            for (const auto& el : *data) {
                if (const auto* s = std::get_if<std::string>(&el.data)) {
                    urls.emplace_back(*s);
                }
            }
        }
        return urls;
    };

    const std::vector<reader::WindowPart> window = reader::window_parts(chapters, pages_of, pos, skip);
    std::size_t n = 0;
    for (const auto& part : window) {
        n += part.urls.size();
    }
    spdlog::info("reader window {} p{} -> {} page(s) in {} part(s)", pos.chapter_id, pos.page, n, window.size());
    return window;
}

} // namespace http::api
