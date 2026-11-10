---
type: decision
title: "browser smoke tooling: blueprint-mcp free mode"
description: "Live smoke di cppIsekai dipakai via blueprint-mcp FREE mode: call enable dengan {client_id:\"free\", force_free:true} (tanpa 5555/client_id asli), lalu browser_tabs attach ke tab cppIsekai (daemon tidak selalu jalan di port yang sama; cek `ss -ltn` / pgrep). chrome-cdp butuh remote debugging manual (chrome://inspect/#remote-debugging) dan biasanya tidak aktif. js di browser_evaluate: hindari IIFE/arrow chain berurutan (parser error) — pakai var + statement terpisah. Console messages penuh noise ekstensi (TOTP/content script) + warning Tailwind CDN, abaikan. Related section detail-enrichment adalah text-only title links (sesuai spec), bukan cover grid. Commit 42b5047/97b4cef."
generated: { by: agent/mcp, at: "2026-11-10T08:38:21Z" }
status: stable
---


