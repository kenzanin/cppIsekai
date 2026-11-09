# Design: cache-layout-v3

## Decisions

### D1 — Layout v3 paths (context-aware writes, hash fallback reads)
- With context params: `img/<plugin>/cover/<sha1-16(url)>.<ext>` or
  `img/<plugin>/manga/<slug>/ch<n>-<id8>/pNNN.<ext>` where `<id8>` = last 8 hex of
  sha1(chapter_id or page url) — the existing `slug.hpp` collision-suffix pattern.
- Without context: current hashed path `img/<sha1-16(url)>.<ext>` (v2 fallback), so
  search-page covers and old links keep working.
- Reads always look up by key WITHOUT extension: caller asks for `ch12-8f3a2c/p003`,
  cache dir scan finds `p003.webp|avif|jxl|original`. Exact-key map from the boot scan
  already exists (v2 rel_path map) — v3 entries just use longer keys.

### D2 — Transcode at store time, one knob
- New registry entry in `src/config/` : `[cache] image_format` (webp default,
  none = passthrough), `CPPISEKAI_IMAGE_FORMAT` env, `--image-format` flag — full
  registry precedence, no config logic outside src/config.
- Conversion happens once at store: decode with vips, encode to target. Store fn
  takes bytes in, writes chosen-ext bytes out; the LRU byte count uses the stored size.
- Old-format files are never rewritten; they serve until eviction. No purge migration.

### D3 — Cover normalization + ratio hint
- `GET /cache/img?url=..&plugin=..&cover=1` — cover=1 routes to `cover/<hash>`;
  a client-supplied `ratio` param is REJECTED (host is source of truth).
- At store time (covers only): vips decodes → host reads real w·h → resize to fit
  `cover_max_px` (default 800) preserving aspect (ratio unchanged by resize) →
  persist dims in a one-line JSON sidecar `cover/<hash>.json`
  (`{"w":..,"h":..,"ratio":..}`) so the serve path never decodes.
- Plugin ratio hint stays OPTIONAL via metadata (not a query param); used only when
  decoding fails. Priority: decoded dims > plugin hint > omit (no default fabricated).
- Serve response exposes `X-Image-Ratio` from the sidecar for placeholder layout.
  Sidecar is outside LRU byte accounting (tiny, bounded by cover count).

### D4 — Chapter dir naming
- `ch<n>-<id8>`: `<n>` = zero-padded chapter number the plugin reports (integer part
  for ordering), `<id8>` disambiguates decimals/duplicates. CBZ builder walks the
  manga slug dir; page order = sorted `pNNN` — unchanged contract.

### D5 — CBZ follows cache format
- No re-encode on export. Readers without webp/avif support are out of scope; noted
  in the capability spec as a documented trade-off.

### D6 — Caller contract
- `/cache/img` gains OPTIONAL `plugin`, `slug`, `ch`, `page`, `cover` params (no
  `ratio` — host-computed, see D3). Detail/chapters templates and worker pass
  context; everything else unchanged.
- reader worker `ensure(rel_path=...)` callers pass the new rel_path shape.

## Risks
- jxl encode of huge buffers is slow: acceptable (local daemon), no timeout added.
- vips load failure on weird input: fall back to storing original bytes (passthrough
  on error), log warn — never fail the fetch because conversion failed.
