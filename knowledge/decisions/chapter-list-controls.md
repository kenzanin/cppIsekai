---
type: decision
title: "chapter-list-controls: read-state routes and detail rows"
description: "Read progress per chapter lives under plugin_state key host/read/<chapter-id> (max pages read; 0 = cleared). Routes: POST /chapter/<id>/read {page}, POST /chapter/state {ids:[...]} -> {<id>: {skip, read}} (absent = unset). POST /reader/position also bumps host/read/<ch> via max(stored, page). Detail page rows carry eye/skip + read toggles and a 20/page client pager rendering [< 1 2 3 ... p-1 p p+1 ... max-2 max-1 max >] (0 marks ellipsis). Alpine x-for pitfalls: keys must NOT reference $index (throws ReferenceError, whole window silently renders nothing), and x-for requires exactly ONE root element — the span+button pair silently rendered only the hidden ellipsis span, leaving just ‹ ›. Fix: single <button> root that renders '…' disabled when n===0. Top pager one line with Chapters heading (flex flex-nowrap whitespace-nowrap, justify-between); bottom pager justify-end. Commit 1b6bd9b; fix 2026-11-11 (x2)."
generated: { by: agent/mcp, at: "2026-11-11T15:11:16Z" }
status: stable
---


