---
type: Decision
title: "Suwayomi WebUI adoption: contract first, no Go port"
description: Do not port cppIsekai to Go to adopt Suwayomi-WebUI; start from the API contract. Language is not the blocker — WebUI is hardwired to Suwayomi-Server GraphQL schema.
tags: [architecture, ui, suwayomi, api]
generated: { by: agent/mcp, at: "2026-11-21T01:32:09Z" }
status: stable
---

# Decision (2026-11-21)

User accepted: start from the contract, not the language. No Go port.

## Evidence
- Suwayomi-WebUI is a Vite/React/TypeScript SPA (MUI v5, Zustand, Apollo GraphQL), default client of Suwayomi-Server (Kotlin/JVM) — not a generic frontend.
- Its data layer spans 18 GraphQL domains: backup, category, chapter, common, download, extension, image, koreader, manga, metadata, server, settings, source, sync, tracker, updater, user, web-view; plus REST media and WS subscriptions.
- cppIsekai has 57 REST routes with its own JSON shapes, Mustache pages, and Lua/JS/WASM plugins. No GraphQL.
- goIsekai API is its own; Suwayomi-Server is Kotlin. A Go port buys zero compatibility and rewrites a working store/plugin/browser/reader stack.

## Two real paths
- A. C++ GraphQL compatibility layer implementing a Suwayomi schema subset — requires backend concepts we lack (categories, download queue, extension APKs, trackers, sync, updater).
- B. Fork the WebUI and repoint its GraphQL client to cppIsekai REST — backend untouched, but a large TS fork to maintain.

## Chosen order
1. Contract first: define the wire shape domain by domain, starting with manga + chapter.
2. Validate with a small POC bridge for one domain before any wholesale swap.
3. Default recommendation stays: borrow Suwayomi UI patterns into the existing Mustache UI rather than a full swap — highest ROI, no feature regression.

The 2026-11-21 API-first audit (57 curl-accessible routes plus JSON log and backup endpoints) is the prerequisite for path B.
