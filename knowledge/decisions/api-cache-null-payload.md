---
type: decision
title: api_cache NULL payload served as 200-empty-body
description: "Bug (2026-11-10): detail page /view/manga/<plugin>/<slug> munjuk SyntaxError Unexpected end of JSON input. Akar: 3 row api_cache punya payload NULL (ditulis sebelum jalur jsonb() terbukti kerja); look-aside anggap row fresh -> return body kosong -> HTTP 200 0B -> alpine res.json() throw. Fix: src/store/store.cpp api_cache_get perlakukan payload NULL/empty (plen==0) sebagai MISS, bukan fresh hit; stale-serve ikut aman karena cabangnya pakai hasil get yang sama. Locked by test_store 'api_cache NULL/empty payload is a miss, not a fresh hit' (raw SQL insert NULL row, get returns nullopt). 3 row rusak di data/library.db dihapus; daemon restart :8090, tab detail direload normal. ctest 14/14 hijau. Commit 52f61cd (+ test)."
generated: { by: agent/mcp, at: "2026-11-10T09:44:07Z" }
status: stable
---


