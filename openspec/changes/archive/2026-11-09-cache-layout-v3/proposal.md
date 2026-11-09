# cache-layout-v3

## Why

The v2 cache stores pages under opaque hashed paths for most callers, serves originals
in whatever format the site ships, and has no cover normalization. This change gives the
image cache a predictable, browsable layout per plugin and transcodes incoming images to
one globally configured format (webp/avif/jxl via libvips), so the disk tree mirrors the
library structure and saves bandwidth/storage on re-serves.

## What Changes

- Image cache layout v3 under `data/cache/img/<plugin>/`:
  - `cover/<url-hash>.<ext>` — all covers of a plugin, flat.
  - `manga/<slug>/ch<n>-<id8>/pNNN.<ext>` — pages in reading order, per chapter,
    `ch<n>-<id8>` guards against duplicate/decimal chapter numbers.
  - Hash-only fallback path (v2 behavior) stays for callers without context.
- Global transcode: new ini knob `[cache] image_format` = `webp|avif|jxl|none`
  (default `webp`); incoming images are converted with libvips (all three loaders
  confirmed present in system vips 8.18.7). Format changes mid-flight are safe:
  lookups are by key, the actual extension on disk decides content-type, old-format
  files keep serving until LRU-evicted.
- Cover normalization: covers larger than a configured max dimension are resized
  keeping aspect ratio; plugins may pass a per-cover aspect ratio hint so the
  placeholder box can reserve the right shape.
- CBZ export follows the cache format as-is (no re-encode) — documented trade-off.

## Capabilities

### New Capabilities
- `cache-layout-v3` — image cache layout, transcode, and cover normalization.

### Modified Capabilities
- none (v2 layout behavior remains as the hash fallback; `/cache/img` contract
  extended with optional context params, not broken).
