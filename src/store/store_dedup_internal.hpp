#pragma once

/** \file store_dedup_internal.hpp
 *  \brief Tokenizer shared by the dedup TUs: defined in store_dedup_tokens.cpp. */

#include <map>
#include <string>

namespace store_detail {

using TokenMultiset = std::map<std::string, int>;

TokenMultiset tokenize(const std::string& text);
double dice(const TokenMultiset& a, const TokenMultiset& b);

} // namespace store_detail
