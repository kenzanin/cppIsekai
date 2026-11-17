---
type: Finding
title: "Detail page dates: plugins emit both epoch seconds and ISO strings"
description: release_at reaches templates as either UNIX epoch seconds or an ISO date string; fmtDate now accepts both and the unknown status pill is hidden.
tags: [templates, release_at, dates, fmtDate, detail-page, 4ucomics, comix, en-thunderscan]
generated: { by: agent/mcp, at: "2026-11-17T21:50:38Z" }
status: stable
---

## Symptom
4ucomics detail page (and any plugin emitting ISO) showed **no chapter dates at all** and no series date, while row counters and read state were fine.

## Cause
`templates/detail_script.mustache` `fmtDate(ts)` was `new Date(ts * 1000)` -- epoch-seconds only (the mangafire shape). ISO strings such as `2026-09-28` become `NaN` and render as empty.

Reality across plugins:
- epoch seconds (`number`): mangafire (`tonumber(c.createdAt)`), host enrichment timestamps.
- ISO date string: comix (`relative_to_iso`), 4ucomics (`c.date:sub(1,10)`), en-thunderscan (`host.text.date_to_iso`).

`architecture/normalization-contract` says the host takes UNIX time only, but nothing in the host consumes `release_at` (no C++ reference) -- it goes plugin JSON -> template verbatim. So the contract is enforced nowhere; the template is the real boundary.

## Fix (c6bf916)
`fmtDate` accepts a number (epoch seconds), a digits-only string (epoch), or any other string (passed straight to `Date`). `fmtEnrichDate` inherits it. Golden `tests/fixtures/golden/detail.mustache.html` regenerated (diff is exactly these lines).

## Same commit, cosmetic
- Detail status pill hides `status === 'unknown'` (4ucomics' API returns `statuses: []` for every title, so "unknown" was a permanent grey badge with no information).
- 4ucomics detail `release_at` was hardcoded `0`; it now uses the earliest chapter date (only real date the API exposes) -> `2026-09-09` for the sampled title.

Verified live: detail page renders `4ucomics  2026-09-09` and rows `ch 011 2026-09-28`, `ch 010 2026-09-09`; suite 19/19.

# Related Concepts
- [Normalisation contract: MangaDex canonical + chapter release_at](../architecture/normalization-contract.md): Recorded divergence: plugins emit ISO date strings as well as epoch seconds; the template boundary accepts both.
- [4ucomics plugin: private JSON API v1 behind Cloudflare, thin manga payload](../architecture/4ucomics-plugin.md): 4ucomics is the ISO-date emitter whose dates were blank on the detail page.
