---
type: decision
title: dummy seed plugins removed
description: "Dummy seed plugins (Asura/Flame/Reaper/Void, 4x12 titles) dihapus sepenuhnya (2026-11-10, commits e0ec8f6/3a550c5): block seed di Store ctor dihapus; tests/test_store.cpp pakai fixture raw SQL sendiri (Asura/Flame 2x12, wipe-then-insert, file test_store.db dihapus di dtor); data/library.db dibersihkan (48 titles + 4 plugin rows, backup di /tmp/library.db.bak-*; MangaDex satu-satunya plugin tersisa); spec plugins-page scenario count-click dibuat generic. ctest 14/14 hijau, specs 16/16 valid, daemon :8090 healthy. Plugin nyata ke depan lewat plugins_dev/ + load_all, bukan seed."
generated: { by: agent/mcp, at: "2026-11-10T09:32:30Z" }
status: stable
---


