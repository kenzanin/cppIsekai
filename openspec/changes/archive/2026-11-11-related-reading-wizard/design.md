# Design

## D1 — Why `chosen.related_add` and not Library entries
Recommendations live on MD/MU, not on the site we saved the manga from; adding them to the Library would imply we can read them, serve chapters, or track progress — none of which is true (no plugin owns those titles). They are *external links*, like the current Related section. Persisting them in `title_meta.chosen.related_add` keeps one write target, keeps reset semantics (`Reset detail` clears non-tag chosen — we include `related_add` in that clear set), and survives fetches.

## D2 — Append vs whitelist semantics
Explicit user ruling: "kalau di manga detail udah ada related reading, append yang dari wizard; double → tampil salah satu". So the Related section = union(scraper related, chosen.related_add), dedupe by normalized key `url || id || name` (lowercase, trim). First occurrence wins, order: scraper → MD → MU.
"Add all" is **replace** semantics on `related_add` (idempotent click), "Remove all" deletes the key. Per-item picking deliberately omitted — the user asked for batch because 10+10 items is a wall of pills.

## D3 — Cap 10 on recommendations only
Both enrichment plugins cap recommendations at 5 today; the user asked for 10 each. Covers (5) and scraper mangadex `related` are out of scope — mangadex scraper `related` returns all scored candidates uncapped already.

## D4 — Whitelist extension only
`normalize_chosen` gets one new key `related_add` (array of objects `{name, url, id?}`; `name`+`url` required strings, `id` optional string). Exact-key rejection is preserved; unknown keys still 400. `tags_hidden` untouched. Validation rejects entries missing `name` or `url` rather than silently dropping — keeps the client honest.

## D5 — Merge point
The Related section is rendered client-side from two sources: the existing `/api/related/<plugin>/<slug>` fetch and the new `enrichment.chosen.related_add` already present in `/api/detail` response. No new endpoint; the dedupe helper in `detail_script.mustache` extends to three inputs with the same key function. Wizard list reuses `mergedList`-style MD-first dedupe but over `{name,url,id}` objects instead of strings.
