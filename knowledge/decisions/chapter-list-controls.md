---
type: decision
title: "chapter-list-controls: read-state routes and detail rows"
description: "Read progress per chapter lives under plugin_state key host/read/<chapter-id> (max pages read; 0 = cleared). Routes: POST /chapter/<id>/read {page}, POST /chapter/state {ids:[...]} -> {<id>: {skip, read}} (absent = unset). POST /reader/position also bumps host/read/<ch> via max(stored, page). Detail page rows carry eye (read) + double-check (done/skip) toggles and a 20/page client pager. Commit 1b6bd9b."
generated: { by: agent/mcp, at: "2026-11-10T07:40:16Z" }
status: stable
---


