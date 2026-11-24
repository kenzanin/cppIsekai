#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <utf8proc.h>


namespace store_detail {
namespace {
// --- library-dedup 2.1 tokeniser (design D5) -----------------------
// utf8proc pipeline: NFKD + casefold + strip combining marks; per-cp
// katakana->hiragana fold plus a tiny no-decomposition letter table.
// (ponytail upgrade of the hand-rolled Latin tables: full-Unicode
// folding without ICU. Tokens are runtime-only: dup groups recompute
// from titles on the next pass, no stored-token migration.)
bool is_separator(uint32_t cp) {
    if (cp < 0x21) {
        return true; // C0 controls + space
    }
    if (cp >= 0x2000 && cp <= 0x206F) {
        return true; // general punctuation
    }
    if (cp >= 0x2E00 && cp <= 0x2E7F) {
        return true;
    }
    if (cp >= 0x3000 && cp <= 0x303F) {
        return true; // CJK symbols & punctuation
    }
    if (cp == 0xFF01) {
        return true;
    }
    return cp == 0xFF0C || cp == 0xFF0E || cp == 0xFF1A || cp == 0xFF1B || cp == 0xFF08 || cp == 0xFF09 ||
           cp == 0xFF5B || cp == 0xFF5D;
}
// Letters utf8proc NFKD leaves intact (no decomposition) -> ASCII base,
// mirroring the old fold table's coverage.
uint32_t fold_residual(uint32_t cp) {
    switch (cp) {
        case 0x00F8: return 'o'; // ø
        case 0x00E6: return 'a'; // æ
        case 0x0111: return 'd'; // đ
        case 0x0131: return 'i'; // ı
        case 0x0142: return 'l'; // ł
        case 0x014A: return 'n'; // ŋ
        case 0x0153: return 'o'; // œ
        case 0x00FE: return 't'; // þ
        default: return cp;
    }
}
void encode_utf8(uint32_t cp, std::string& out) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}
} // namespace

using TokenMultiset = std::map<std::string, int>;
TokenMultiset tokenize(const std::string& s) {
    TokenMultiset out;
    std::string cur;
    auto flush = [&] {
        if (cur.empty()) {
            return;
        }
        out[std::move(cur)] += 1;
        cur.clear();
    };
    const utf8proc_ssize_t max_cp = static_cast<utf8proc_ssize_t>(s.size()) * 4 + 16;
    std::vector<utf8proc_int32_t> buffer(max_cp);
    const utf8proc_ssize_t ncp = utf8proc_decompose(
        reinterpret_cast<const utf8proc_uint8_t*>(s.data()),
        static_cast<utf8proc_ssize_t>(s.size()),
        buffer.data(), max_cp,
        static_cast<utf8proc_option_t>(UTF8PROC_COMPAT | UTF8PROC_DECOMPOSE | UTF8PROC_CASEFOLD | UTF8PROC_STRIPMARK));
    if (ncp < 0) {
        return out; // undecomposable input: no tokens
    }
    for (utf8proc_ssize_t i = 0; i < ncp; ++i) {
        const uint32_t cp = static_cast<uint32_t>(buffer[i]);
        if (is_separator(cp)) {
            flush();
            continue;
        }
        uint32_t folded = fold_residual(cp);
        if (folded >= 0x30A1 && folded <= 0x30F6) {
            folded -= 0x60; // katakana -> hiragana
        }
        if ((folded >= 'a' && folded <= 'z') || (folded >= '0' && folded <= '9')) {
            cur.push_back(static_cast<char>(folded));
        } else if (folded >= 0x1100) {
            encode_utf8(folded, cur); // CJK/kana/etc: verbatim
        } else {
            flush();
        }
    }
    flush();
    return out;
}
// Dice coefficient over two multisets: 2*Σmin(a,b) / (Σa + Σb)
double dice(const TokenMultiset& a, const TokenMultiset& b) {
    if (a.empty() || b.empty()) {
        return 0.0;
    }
    long sum = 0;
    long inter = 0;
    for (const auto& [k, v] : a) {
        sum += v;
    }
    for (const auto& [k, v] : b) {
        sum += v;
        auto it = a.find(k);
        if (it != a.end()) {
            inter += std::min(static_cast<long>(v), static_cast<long>(it->second));
        }
    }
    return (sum == 0) ? 0.0 : (2.0 * static_cast<double>(inter)) / static_cast<double>(sum);
    return (sum == 0) ? 0.0 : (2.0 * static_cast<double>(inter)) / static_cast<double>(sum);
}

} // namespace store_detail
