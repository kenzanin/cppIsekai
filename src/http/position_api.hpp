#pragma once

/** \file position_api.hpp
 *  \brief Pure core of POST /reader/position (reader-page 2.2): layering the
 *  request over the stored row. Crow-free, so the partial-update semantics are
 *  unit tested directly (test_position).
 */

#include "store/store.hpp"

#include <optional>

/** \brief Absent (empty) mode/direction on the request keep the stored value;
 *  identity, chapter and page always come from the request. The chapter
 *  number is resolved by the route from the window pass, not here. */
inline ReaderPosition merge_position(const std::optional<ReaderPosition>& stored, ReaderPosition next) {
    if (stored) {
        if (next.mode.empty())
            next.mode = stored->mode;
        if (next.direction.empty())
            next.direction = stored->direction;
        if (next.zoom.empty())
            next.zoom = stored->zoom;
    }
    return next;
}
