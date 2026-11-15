#pragma once

/** \file sha1.hpp
 *  \brief sha1_hex -- cache keys, vendored hash-library (same dep as host_crypto).
 */

#include <string>
#include <sha1.h>

/** \brief Lowercase-hex SHA-1 digest of \p msg. */
inline std::string sha1_hex(const std::string& msg) {
    SHA1 h;
    h.add(msg.data(), msg.size());
    return h.getHash();
}
