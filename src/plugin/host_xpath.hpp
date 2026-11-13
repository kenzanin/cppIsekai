/** \brief host.html.xpath(expr, html) — XPath over lenient HTML (P8/P9).
 *
 *  Pipeline: lexbor parse+serialize (HTML5 normalization: implied tags closed,
 *  tree regularized) -> fixup pass (self-close HTML void elements, fold common
 *  named entities to UTF-8) -> pugixml DOM -> xpath. Returns the string value
 *  of every match in document order (attribute nodes -> value, everything
 *  else -> text); nullopt on unparsable doc or bad expression.
 *
 *  Ceilings (ponytail): only the common named entities are folded — rarer
 *  ones stay literal in results; script bodies containing raw `<` can confuse
 *  the XML layer. Extend the map / pre-replace if a site needs more.
 */
#pragma once

#include <lexbor/html/html.h>
#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace plugin::host_xpath {

/** \brief HTML5 void elements — the serializer leaves them unclosed. */
constexpr std::array<std::string_view, 14> kVoidTags = {
    "area", "base", "br", "col", "embed", "hr", "img", "input",
    "link", "meta", "param", "source", "track", "wbr"};

/** \brief Named entities beyond the XML five, folded to UTF-8. */
constexpr std::array<std::pair<std::string_view, std::string_view>, 10> kEntities = {{
    {"&nbsp;", "\xC2\xA0"},   {"&copy;", "\xC2\xA9"},   {"&reg;", "\xC2\xAE"},
    {"&deg;", "\xC2\xB0"},    {"&mdash;", "\xE2\x80\x94"}, {"&ndash;", "\xE2\x80\x93"},
    {"&hellip;", "\xE2\x80\xA6"}, {"&lsquo;", "\xE2\x80\x98"},
    {"&rsquo;", "\xE2\x80\x99"},  {"&middot;", "\xC2\xB7"},
}};

/** \brief Self-close void tags: `<br>` -> `<br/>` (attribute-aware, cheap scan). */
inline std::string close_voids(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 32);
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '<') {
            out += s[i++];
            continue;
        }
        size_t n = i + 1;
        while (n < s.size() && static_cast<bool>(std::isalpha(static_cast<unsigned char>(s[n])))) {
            ++n;
        }
        const std::string_view name(s.data() + i + 1, n - i - 1);
        const bool voided = n > i + 1 && std::find(kVoidTags.begin(), kVoidTags.end(), name) != kVoidTags.end();
        if (!voided) {
            out += s[i++];
            continue;
        }
        const size_t gt = s.find('>', n);
        if (gt == std::string::npos) {
            out.append(s, i, std::string::npos);
            break;
        }
        out.append(s, i, gt - i); // "<tag attrs"
        if (gt > i && s[gt - 1] != '/') {
            out += '/';
        }
        out += '>';
        i = gt + 1;
    }
    return out;
}

/** \brief Fold the common named entities; XML-five are left for pugixml. */
inline void fold_entities(std::string& s) {
    for (const auto& [ent, utf8] : kEntities) {
        for (size_t pos = s.find(ent); pos != std::string::npos; pos = s.find(ent, pos + utf8.size())) {
            s.replace(pos, ent.size(), utf8);
        }
    }
}

/** \brief Evaluate `query` (XPath) over `html`; string values in document order. */
inline std::optional<std::vector<std::string>> xpath(const std::string& html, const std::string& query) {
    // -- 1. lexbor parse + serialize --
    lxb_html_document_t* doc = lxb_html_document_create();
    if (doc == nullptr) {
        return std::nullopt;
    }
    const lxb_status_t st =
        lxb_html_document_parse(doc, reinterpret_cast<const lxb_char_t*>(html.data()), html.size());
    std::string ser;
    if (st == LXB_STATUS_OK) {
        const lxb_html_serialize_cb_f cb = [](const lxb_char_t* data, size_t len, void* ctx) -> lxb_status_t {
            static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), len);
            return LXB_STATUS_OK;
        };
        lxb_html_serialize_tree_cb(lxb_dom_interface_node(doc), cb, &ser);
    }
    lxb_html_document_destroy(doc);
    if (st != LXB_STATUS_OK) {
        return std::nullopt;
    }

    // -- 2. XML-compliance fixup --
    ser = close_voids(ser);
    fold_entities(ser);

    // -- 3. pugixml evaluate --
    pugi::xml_document pdoc;
    if (!pdoc.load_string(ser.c_str(), pugi::parse_default)) {
        return std::nullopt;
    }
    pugi::xpath_node_set sel;
    try {
        sel = pdoc.select_nodes(query.c_str());
    } catch (const pugi::xpath_exception&) {
        return std::nullopt;
    }
    std::vector<std::string> out;
    out.reserve(sel.size());
    for (const pugi::xpath_node& r : sel) {
        if (r.attribute() != nullptr) {
            out.emplace_back(r.attribute().value());
        } else if (r.node().type() == pugi::node_pcdata || r.node().type() == pugi::node_cdata) {
            out.emplace_back(r.node().value());
        } else {
            out.emplace_back(r.node().text().get()); // elements: direct text
        }
    }
    return out;
}

} // namespace plugin::host_xpath
