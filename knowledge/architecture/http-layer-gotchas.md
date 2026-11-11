---
type: Fact
title: "src/http compile gotchas: global-scope helpers in api_ctx.hpp; crow::response is move-only"
description: "Two repeat compile traps in src/http: helpers defined below the namespace close of api_ctx.hpp must qualify http::/http::api:: names, and crow::response locals must stay non-const to be returned (copy ctor deleted)."
tags: [crow, api_ctx, compile, namespaces, http]
generated: { by: agent/mcp, at: "2026-11-11T16:59:41Z" }
status: stable
---

Both hit 2026-11-11 while wiring library-grid-stats; cost a failed build each.

## 1. api_ctx.hpp global-scope helpers
`sync_saved_stats` (and the `detail` envelope readers block) live AFTER `} // namespace api } // namespace http` in `src/http/api_ctx.hpp` -- global scope. Unqualified `on_pool` / `kStateHost` / `kReadPrefix` there do NOT compile (`error: not declared in scope; did you mean http::on_pool?`). Qualify explicitly: `http::on_pool(...)`, `http::api::kStateHost`, `http::api::kReadPrefix`. New shared helpers that need pool/state access: either define inside `namespace http::api` or qualify every reference.

## 2. crow::response move-only
`crow::response` declares a move ctor => the copy ctor is implicitly deleted. The pattern `const crow::response r = f(); ...; return r;` fails: a const local cannot use the implicit-move-on-return path and copy is deleted. Keep response locals non-const (`crow::response r = ...`) when returned; annotate route lambdas `-> crow::response` as usual.
