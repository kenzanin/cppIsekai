---
type: decision
title: "chapter-list-controls: read-state routes and detail rows"
description: "Read progress per chapter lives under plugin_state key host/read/<chapter-id> (max pages read; 0 = cleared). Routes: POST /chapter/<id>/read {page}, POST /chapter/state {ids:[...]} -> {<id>: {skip, read}} (absent = unset). POST /reader/position also bumps host/read/<ch> via max(stored, page). Detail page rows carry eye/skip + read toggles and a 20/page client pager. Pager numbers render via x-for :key=\"ct|cb'+n\" — Alpine x-for keys must NOT reference $index (throws ReferenceError and the whole window silently renders nothing, leaving only ‹ ›). Also: top pager must stay on one line with the Chapters heading (flex flex-nowrap whitespace-nowrap, justify-between); bottom pager justify-end. Commit 1b6bd9b; fix 2026-11-11."
generated: { by: agent/mcp, at: "2026-11-11T15:04:42Z" }
status: stable
---


