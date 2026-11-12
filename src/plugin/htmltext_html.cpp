#include "htmltext.hpp"
#include "htmltext_internal.hpp"

#include <lexbor/css/css.h>
#include <lexbor/css/selectors/selectors.h>
#include <lexbor/dom/dom.h>
#include <lexbor/html/html.h>
#include <lexbor/selectors/selectors.h>

#include <cstring>
#include <memory>
#include <vector>

namespace plugin {
namespace {
/// Owns one parsed HTML document; null-equivalent on parse failure (lenient).
class HtmlDoc {
public:
    explicit HtmlDoc(const std::string& html) {
        doc_ = lxb_html_document_create();
        if (doc_ == nullptr) {
            return;
        }
        const lxb_status_t st =
            lxb_html_document_parse(doc_, reinterpret_cast<const lxb_char_t*>(html.data()), html.size());
        if (st != LXB_STATUS_OK) {
            lxb_html_document_destroy(doc_);
            doc_ = nullptr;
        }
    }
    ~HtmlDoc() {
        if (doc_ != nullptr) {
            lxb_html_document_destroy(doc_);
        }
    }
    HtmlDoc(const HtmlDoc&) = delete;
    HtmlDoc& operator=(const HtmlDoc&) = delete;

    lxb_html_document_t* get() const { return doc_; }

private:
    lxb_html_document_t* doc_ = nullptr;
};

/// RAII guards for the lexbor objects (fn-pointer deleters with bool flags do
/// not fit unique_ptr's D cleanly; tiny structs are clearer).
struct SelectorsGuard {
    lxb_selectors_t* p = nullptr;
    ~SelectorsGuard() {
        if (p != nullptr) {
            (void)lxb_selectors_destroy(p, true);
        }
    }
};
struct CollectionGuard {
    lxb_dom_collection_t* p = nullptr;
    ~CollectionGuard() {
        if (p != nullptr) {
            (void)lxb_dom_collection_destroy(p, true);
        }
    }
};
struct ParserGuard {
    lxb_css_parser_t* p = nullptr;
    ~ParserGuard() {
        if (p != nullptr) {
            (void)lxb_css_parser_destroy(p, true);
        }
    }
};

/// Runs `fn` on every element matching the CSS selector, in document order.
/// Selector/CSS failure yields zero visits (lenient), never an error.
template <typename Fn> void select_each(const std::string& html, const std::string& css, Fn fn) {
    HtmlDoc doc(html);
    if (doc.get() == nullptr) {
        return;
    }
    lxb_dom_document_t* ddoc = lxb_dom_interface_document(doc.get());

    ParserGuard parser_guard;
    parser_guard.p = lxb_css_parser_create();
    if (parser_guard.p == nullptr || lxb_css_parser_init(parser_guard.p, nullptr) != LXB_STATUS_OK) {
        return;
    }

    SelectorsGuard sels_guard;
    sels_guard.p = lxb_selectors_create();
    if (sels_guard.p == nullptr || lxb_selectors_init(sels_guard.p) != LXB_STATUS_OK) {
        return;
    }

    CollectionGuard col_guard;
    col_guard.p = lxb_dom_collection_make(ddoc, 16);
    if (col_guard.p == nullptr) {
        return;
    }

    lxb_css_selector_list_t* list =
        lxb_css_selectors_parse(parser_guard.p, reinterpret_cast<const lxb_char_t*>(css.data()), css.size());
    if (list == nullptr) {
        return; // malformed selector: zero matches (lenient)
    }

    std::vector<lxb_dom_node_t*> nodes;
    const auto cb = [](lxb_dom_node_t* node, lxb_css_selector_specificity_t /*spec*/, void* ctx) -> lxb_status_t {
        static_cast<std::vector<lxb_dom_node_t*>*>(ctx)->push_back(node);
        return LXB_STATUS_OK;
    };
    if (lxb_selectors_find(sels_guard.p, lxb_dom_interface_node(doc.get()), list, cb, &nodes) != LXB_STATUS_OK) {
        return;
    }

    for (lxb_dom_node_t* node : nodes) {
        fn(lxb_dom_interface_element(node), ddoc);
    }
}

} // namespace

std::vector<std::string> html_select(const std::string& html, const std::string& css) {
    std::vector<std::string> out;
    select_each(html, css, [&](lxb_dom_element_t* el, lxb_dom_document_t* ddoc) {
        lxb_dom_node_t* n = lxb_dom_interface_node(el);
        size_t tl = 0;
        lxb_char_t* text = lxb_dom_node_text_content(n, &tl);
        if (text == nullptr) {
            out.emplace_back();
            return;
        }
        out.push_back(trim_text(std::string(reinterpret_cast<const char*>(text), tl)));
        lxb_dom_document_destroy_text(ddoc, text);
    });
    return out;
}

std::vector<std::string> html_attr(const std::string& html, const std::string& css, const std::string& attr) {
    std::vector<std::string> out;
    select_each(html, css, [&](lxb_dom_element_t* el, lxb_dom_document_t* ddoc) {
        // Elements without the requested attribute are skipped (spec).
        for (lxb_dom_attr_t* a = lxb_dom_element_first_attribute(el); a != nullptr;
             a = lxb_dom_element_next_attribute(a)) {
            size_t nl = 0;
            const lxb_char_t* name = lxb_dom_attr_qualified_name(a, &nl);
            if (nl != attr.size() || std::memcmp(name, attr.data(), nl) != 0) {
                continue;
            }
            size_t vl = 0;
            const lxb_char_t* val = lxb_dom_attr_value(a, &vl);
            if (val != nullptr) {
                out.push_back(trim_text(std::string(reinterpret_cast<const char*>(val), vl)));
            }
            return; // first matching attribute wins; done with this element
        }
    });
    return out;
}

} // namespace plugin
