# Design: pages-local-first

## D1. `chapter_pages` rows are the read source for an opened chapter

**Decision.** `GET /api/pages/<plugin>/<slug>/<ch>` resolves the target release from the `chapters`
rows, then answers from `chapter_pages_urls(plugin, slug, chapter_id)` whenever that release has rows.
The plugin is called only for a release with no stored rows (never opened, or wiped by unsave).

**Why.** The rows are written on every successful parse, are permanent, and are already the store the
reader's offline fallback and the CBZ gate key on. `api_cache` is the disposable envelope store
(`library-promote-on-save` D6 kept `pages` there for signed-URL reasons); for page lists the durable
copy exists, so the disposable one is not the reader.

**Consequence.** Supersedes `library-promote-on-save` D6 for `pages` only: detail/chapters keep their
`api_cache` behavior for titles outside the library. A locally served list can lag the site until
⟳ Update or the silent revalidation of D3.

## D2. Disk completeness is not a serve gate

**Decision.** Serving a stored page list does not require the chapter's images to be complete
(`chapter_cached` is not consulted for the response). The disk is checked only to decide whether a
background revalidation is worth scheduling (D3).

**Why.** Page cache files are keyed by layout path (`chNNN/pNNN`), never by URL, so a cached page is
served without the URL. Gating the response on 100% disk coverage would keep the network call for
partially read chapters — exactly the case the change exists to remove.

**Consequence.** The reader always gets a list. Pages already on disk render; a missing page is the
only thing that touches the network.

## D3. Silent revalidation, throttled per chapter, fire-and-forget

**Decision.** After a locally served response, if the chapter is not fully on disk the route submits one
`plugin_pool.detach_task` that re-parses that release's page list and calls
`chapter_pages_replace` on success. A process-lifetime in-memory set (`plugin|slug|chapter_id`) admits
the task once; a second open in the same process does not schedule another. Cold-path parses never
schedule anything (they just produced fresh URLs).

**Why.** Some sites sign image URLs (`comico` `d=` decays in minutes, `mangatown` `token`/`ttl`), so a
permanently archived URL list can go stale. Serving locally and repairing in the background keeps the
read path offline-first without adding a request to the user's critical path. A new worker thread or
queue would duplicate what the shared plugin pool already provides; the throttle is one set under one
mutex. The task is submitted only while the plugin is enabled and goes through `call_cached` (which is
now a bare gate, since `pages` is no longer opt-in), so a disabled plugin stays network-quiet.

**Consequence.** Accepted: the open that schedules the revalidation may use a decayed URL for a page
that is not on disk yet, so that image can 404 once. The next open uses the refreshed list. Upgrade
path if that ever matters: return a fetch status from `Cache::Fetcher` and invalidate on 4xx (option
B in the discussion), or await the refresh before responding.

ponytail: throttle is once per chapter per process -- a site that rotates URLs inside one session is
not covered; move the set to a `refreshed_at` map with a TTL if that shows up.

## D4. `reader_window` reads rows; prefetch keeps filling missing images

**Decision.** `reader_window` builds its chapter list from `store.chapters_list` and its page lists
from `chapter_pages_urls`, with the existing `call_cached` path kept only for a cold (row-less) title.
The prefetch worker is unchanged: it still walks the window and calls `img_ensure`, which fetches only
pages absent from disk.

**Why.** `reader_window` is the prefetch input, and it was the last consumer of `call_cached("pages")`
besides the reader route. Leaving it on the envelope store would keep a metadata request per chapter
flip even for a fully opened title.

**Consequence.** Prefetch traffic is image-only, and only for pages not yet cached — the requested
behavior stays "network for what is missing, not for what we already know".

## D5. `pages` leaves `stale_optin`; the TTL special case goes with it

**Decision.** Remove `pages` from `stale_optin` and delete `cache_ttl_for` (the only reason it existed
was the 30-minute page TTL). `effective_ttl` uses the 24h constant; manifest overrides stay per
plugin+method.

**Why.** With D1 the route never reads `call_cached`'s page envelope, so writing a second copy is dead
weight and a second consistency surface. Existing `api_cache` page rows age out through
`cache_prune_expired` untouched; no migration, no wipe.

**Consequence.** The debug route `/plugin/<id>/pages` becomes a plain plugin call. That is the intent:
it is a debug seam, and the durable page list is written by `/api/pages`.
