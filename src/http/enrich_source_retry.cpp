#include "enrich_internal.hpp"

namespace http::api {

// GET /api/enrichment/<id>/<detail|covers>?plugin=&slug=&force=0|1
/** \brief Titles to retry with when the library name finds nothing upstream:
 *  the scraper detail envelope's own title/name, then every title a previous
 *  enrichment match recorded (with its alt titles). Order is best-first and
 *  deduped against \p first; bounded so one bad name costs a few calls, not a
 *  crawl. (library-ownership follow-up: the 5-cover report was a stale stored
 *  candidate whose refetch could never match the library's spelling.) */
std::vector<std::string> retry_titles(Store& store, const std::string& plugin, const std::string& slug,
                                             const std::string& first) {
    std::vector<std::string> out;
    const auto push = [&](const std::string& t) {
        if (!t.empty() && t != first && std::ranges::find(out, t) == out.end() && out.size() < 4) {
            out.push_back(t);
        }
    };
    if (const auto row = store.title_meta_get(plugin, slug); row.has_value()) {
        glz::generic fetched;
        if (!glz::read_json(fetched, row->fetched) && fetched.is_array()) {
            for (const auto& entry : fetched.get_array()) {
                if (!entry.is_object()) {
                    continue;
                }
                const auto& d = entry.get_object().find("data");
                if (d == entry.get_object().end() || !d->second.is_object()) {
                    continue;
                }
                const auto& data = d->second.get_object();
                if (const auto* t = data.find("title"); t != data.end()) {
                    if (const auto* s = std::get_if<std::string>(&t->second.data)) {
                        push(*s);
                    }
                }
                if (const auto* t = data.find("name"); t != data.end()) {
                    if (const auto* s = std::get_if<std::string>(&t->second.data)) {
                        push(*s);
                    }
                }
                if (const auto* a = data.find("alt_titles"); a != data.end() && a->second.is_array()) {
                    for (const auto& el : a->second.get_array()) {
                        if (const auto* s = std::get_if<std::string>(&el.data)) {
                            push(*s);
                        } else if (el.is_object()) {
                            if (const auto* t = el.get_object().find("title"); t != el.get_object().end()) {
                                if (const auto* s = std::get_if<std::string>(&t->second.data)) {
                                    push(*s);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // Scraper's own detail envelope last: it is what the source site calls the
    // work, which is usually the spelling the aggregators index too.
    crow::json::wvalue p;
    p["id"] = slug;
    if (const auto row = store.api_cache_get(plugin, stale_key(plugin, "detail", p.dump())); row.has_value()) {
        if (const auto obj = detail::envelope_object(row->payload)) {
            push(detail::envelope_str(*obj, "title"));
            push(detail::envelope_str(*obj, "name"));
        }
    }
    return out;
}

} // namespace http::api
