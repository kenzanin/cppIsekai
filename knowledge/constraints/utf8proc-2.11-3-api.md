---
type: constraint
title: utf8proc 2.11.3 C API surface
description: "utf8proc_NFKD() is a FUNCTION (normalize string), not an option constant. Option constants: UTF8PROC_COMPAT, UTF8PROC_DECOMPOSE, UTF8PROC_CASEFOLD, UTF8PROC_STRIPMARK (enum utf8proc_option_t, scoped-needs-cast). NFKD decomposition via utf8proc_decompose(str, len, buffer, bufsize, UTF8PROC_COMPAT|UTF8PROC_DECOMPOSE|UTF8PROC_CASEFOLD|UTF8PROC_STRIPMARK) returning int32 codepoint array. Output buffer from utf8proc_map freed with std::free, not utf8proc_free (does not exist). utf8proc_int32_t typedef for codepoints."
tags: [utf8proc, constraint, c-api, tokenizer]
generated: { by: agent/mcp, at: "2026-11-13T00:44:17Z" }
status: stable
---


