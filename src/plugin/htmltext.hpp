/** \file htmltext.hpp
 *  \brief Host-side heavy text primitives shared by every plugin runtime.
 *
 *  Two families, both returning plain \c std::vector&lt;std::string&gt; so no
 *  runtime type leaks in (design D4 - one core, three thin marshals):
 *  - \ref re2_find / \ref re2_match - RE2 (linear-time, no ReDoS) capture-group
 *    arrays; element 0 is the whole match, then submatches. No match is an
 *    empty vector, never an error; an uncompilable pattern throws
 *    std::invalid_argument (loud, runtime code turns it into its own error).
 *  - \ref html_select / \ref html_attr - lexbor (lenient HTML parsing) element
 *    text / attribute arrays in document order; no match or malformed input
 *    yields an empty vector, never an error.
 */
#pragma once

#include <string>
#include <vector>

namespace plugin {

/** \brief Whole match + capture groups of the first position where `pattern`
 *         matches `text`; empty vector when unavailable.
 *  \throws std::invalid_argument when the pattern fails RE2 compilation.
 */
std::vector<std::string> re2_find(const std::string& pattern, const std::string& text);

/** \brief Match anchored at the start of `text` (RE2 FullMatch semantics):
 *         whole match + capture groups; empty vector when it does not match.
 *  \throws std::invalid_argument when the pattern fails RE2 compilation.
 */
std::vector<std::string> re2_match(const std::string& pattern, const std::string& text);

/** \brief Trimmed text content of every element matching the CSS selector, in
 *         document order. Empty vector on no match or malformed HTML. */
std::vector<std::string> html_select(const std::string& html, const std::string& css);

/** \brief Values of `attr` on every element matching the CSS selector that has
 *         it, in document order (elements without the attribute are skipped).
 *         Empty vector on no match or malformed HTML. */
std::vector<std::string> html_attr(const std::string& html, const std::string& css, const std::string& attr);

} // namespace plugin
