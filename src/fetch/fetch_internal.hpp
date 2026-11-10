/** \file fetch_internal.hpp
 *  \brief Internal seam: the curl-family fn tables shared by the dlopen loader
 *  and the transfer body. */
#pragma once
#include <string>
#include <curl/curl.h>
namespace fetch {
struct CurlApi {
    CURL* (*easy_init)();
    void (*easy_cleanup)(CURL*);
    CURLcode (*easy_setopt)(CURL*, CURLoption, ...);
    CURLcode (*easy_perform)(CURL*);
    CURLcode (*easy_getinfo)(CURL*, CURLINFO, ...);
    curl_slist* (*slist_append)(curl_slist*, const char*);
    const char* (*easy_strerror)(CURLcode);
    CURLcode (*easy_impersonate)(CURL*, const char*, unsigned);
};
const CurlApi& chrome_api();
const CurlApi* firefox_api();
const CurlApi* system_api();
} // namespace fetch
