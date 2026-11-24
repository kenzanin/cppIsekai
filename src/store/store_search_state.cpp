#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <ctime>
#include <string>
#include <string_view>
#include <vector>

using namespace sqlite_orm;


std::vector<std::string> Store::state_keys_value(const std::string& plugin_id, const std::string& key_prefix,
                                                 const std::string& value) {
    const store_detail::OpTrace _op{"state_keys_value"};
    std::string pat;
    pat.reserve(key_prefix.size() + 1);
    for (char ch : key_prefix) {
        if (ch == '%' || ch == '_' || ch == '\\') {
            pat += '\\';
        }
        pat += ch;
    }
    pat += '%';
    auto rows = impl_->storage.get_all<store_detail::StateEntity>(
        where(c(&store_detail::StateEntity::plugin_id) == plugin_id &&
              like(&store_detail::StateEntity::key, pat).escape(std::string("\\")) &&
              c(&store_detail::StateEntity::value) == value));
    std::vector<std::string> out;
    out.reserve(rows.size());
    for (const auto& r : rows) {
        out.push_back(r.key);
    }
    return out;
}

void Store::position_set(const ReaderPosition& pos) {
    const store_detail::OpTrace _op{"position_set"};
    // REPLACE on the (plugin, slug) PK: the route layers optional fields over
    // the stored row, this just persists the result. updated_at stamped here.
    if (!saved_exists(pos.plugin, pos.slug)) {
        auto& e = ephem_create(pos.plugin, pos.slug);
        e.position = store_detail::ReaderPositionEntity{.plugin = pos.plugin,
                                                        .slug = pos.slug,
                                                        .chapter_id = pos.chapter_id,
                                                        .chapter_number = pos.chapter_number,
                                                        .page = pos.page,
                                                        .mode = pos.mode,
                                                        .direction = pos.direction,
                                                        .zoom = pos.zoom,
                                                        .updated_at = std::time(nullptr)};
        return;
    }

    impl_->storage.replace(store_detail::ReaderPositionEntity{
        .plugin = pos.plugin,
        .slug = pos.slug,
        .chapter_id = pos.chapter_id,
        .chapter_number = pos.chapter_number,
        .page = pos.page,
        .mode = pos.mode,
        .direction = pos.direction,
        .zoom = pos.zoom,
        .updated_at = static_cast<std::int64_t>(std::time(nullptr)),
    });
}

std::optional<ReaderPosition> Store::position_get(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"position_get"};
    if (!saved_exists(plugin, slug)) {
        if (auto* e = ephem_find(plugin, slug); e != nullptr && e->position.has_value()) {
            const auto& r = *e->position;
            return ReaderPosition{.plugin = r.plugin,
                                  .slug = r.slug,
                                  .chapter_id = r.chapter_id,
                                  .chapter_number = r.chapter_number,
                                  .page = r.page,
                                  .mode = r.mode,
                                  .direction = r.direction,
                                  .zoom = r.zoom,
                                  .updated_at = r.updated_at};
        }
        return std::nullopt;
    }
    auto rows = impl_->storage.get_all<store_detail::ReaderPositionEntity>(
        where(c(&store_detail::ReaderPositionEntity::plugin) == plugin &&
              c(&store_detail::ReaderPositionEntity::slug) == slug));
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& r = rows[0];
    return ReaderPosition{
        .plugin = r.plugin,
        .slug = r.slug,
        .chapter_id = r.chapter_id,
        .chapter_number = r.chapter_number,
        .page = r.page,
        .mode = r.mode,
        .direction = r.direction,
        .zoom = r.zoom,
        .updated_at = r.updated_at,
    };
}

std::optional<ReaderPosition> Store::position_last() {
    const store_detail::OpTrace _op{"position_last"};
    const auto to_pos = [](const store_detail::ReaderPositionEntity& r) {
        return ReaderPosition{.plugin = r.plugin,
                              .slug = r.slug,
                              .chapter_id = r.chapter_id,
                              .chapter_number = r.chapter_number,
                              .page = r.page,
                              .mode = r.mode,
                              .direction = r.direction,
                              .zoom = r.zoom,
                              .updated_at = r.updated_at};
    };
    std::optional<ReaderPosition> best;
    // ephemeral pairs race the SQL rows for the anchor: newest updated_at wins
    for (const auto& [key, e] : impl_->ephem) {
        if (!e.position.has_value()) {
            continue;
        }
        if (!best || e.position->updated_at > best->updated_at) {
            best = to_pos(*e.position);
        }
    }
    auto rows = impl_->storage.get_all<store_detail::ReaderPositionEntity>(
        order_by(&store_detail::ReaderPositionEntity::updated_at).desc(), limit(1));
    if (!rows.empty() && (!best || rows.front().updated_at > best->updated_at)) {
        best = to_pos(rows.front());
    }
    return best;
}

