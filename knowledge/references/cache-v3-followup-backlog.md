---
type: reference
title: "cache-v3 follow-up backlog: cbz lexicographic sort, common/ cover namespace, VIPS link requirement"
description: "Gap dan catatan environment dari final review cache-layout-v3 (2026-11-09): (1) lexicographic sort di cbz builder -- ch999 > ch1000 secara string, manga >999 chapter bisa kebalik urutan di CBZ export, belum di-fix; (2) CoverCtx.plugin kosong jatuh ke namespace 'common/' di img_store/cover_serve, deliberately permissive, bisa diperketat nanti; (3) semua target CMake yang compile cache.cpp wajib link PkgConfig::VIPS (transcode.hpp include vips) -- target baru tanpa link akan gagal dengan glib.h not found."
tags: [backlog, cache-layout-v3, cbz-export, sort, review-findings, polish]
generated: { by: agent/mcp, at: "2026-11-09T11:45:23Z" }
status: stable
---

# image-cache deferred polish backlog

Dari sesi cache-layout-v3 (2026-11-09):

## Gap: lexicographic sort di cbz builder (bug nyata, belum di-fix)
`src/http/export_cbz.cpp` pakai `std::map<std::string>` -- sort string murni. Min-3 zero-pad (`p003`, `ch012`) aman sampai 999, tapi `ch999` > `ch1000` secara lexicographic: manga >999 chapter atau halaman >999 dalam satu chapter bisa kebalik urutan di CBZ hasil export. Fix path: numeric-aware compare pada segmen `ch`/`p` sebelum emplace ke map. Seeded question dari user: "doremon yang udah ribuan chapter gimana?" -- naming-nya aman, sort cbz-nya yang belum.

## Gap: plugin kosong di CoverCtx jatuh ke `common/`
`img_store`/`cover_serve` memakai `"common"` sebagai namespace plugin ketika `CoverCtx.plugin` kosong (cover di-fetch tanpa param `plugin`). Ini deliberately permissive (degradasi, bukan error); dipertimbangkan diperketat jadi reject atau routing explisit nanti.

## Environment note: semua target yang compile cache.cpp wajib link PkgConfig::VIPS
cache.cpp sekarang include transcode.hpp (vips). Target test yang compile cache.cpp (test_cache, test_http_img, test_stale, test_worker) sudah di-link; target BARU yang menambah cache.cpp harus ikut, else `glib.h: No such file` fatal error.

# Related Concepts
- [cache-layout-v3: per-plugin layout, global transcode, host cover ratio](../architecture/cache-layout-v3.md): Follow-up gaps discovered during cache-layout-v3 final review: cbz lexicographic sort, common/ cover namespace, VIPS link requirement
