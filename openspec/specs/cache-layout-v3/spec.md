# cache-layout-v3 Specification

## Purpose
Give the image cache a browsable per-plugin layout and transcode every stored image to
one globally configured format (webp/avif/jxl), with cover normalization and safe
mid-flight format switches.

## Requirements

### Requirement: Context-aware image cache layout
Images stored with context parameters SHALL be written under
`img/<plugin>/cover/<urlhash>.<ext>` (covers) or
`img/<plugin>/manga/<slug>/ch<n>-<id8>/pNNN.<ext>` (pages, sorted by `pNNN` for reading
order); `ch<n>-<id8>` SHALL disambiguate duplicate or decimal chapter numbers. Images
stored without context SHALL keep the hashed fallback path. Reads SHALL match by key
without extension so any stored format is servable.

#### Scenario: Page lands in its chapter dir
- **WHEN** the reader stores page 3 of chapter 12 (id `8f3a2c...`) of slug `solo-leveling`
  with plugin context
- **THEN** the file is written at `img/mangadex/manga/solo-leveling/ch12-8f3a2c/p003.webp`
- **AND** a later request for the same key serves it regardless of stored extension

#### Scenario: No context keeps hashed fallback
- **WHEN** `/cache/img?url=...` is called without `plugin`/`slug`/`ch`/`page`
- **THEN** the image is stored under the v2 hashed path and serves unchanged

### Requirement: Global image transcode
The daemon SHALL transcode every newly stored image to the format configured by
`[cache] image_format` (webp default; avif and jxl supported; none = passthrough),
via the config registry precedence (ini < env < flag). A mid-flight format change SHALL
NOT invalidate stored files: old-format files keep serving until LRU eviction. If vips
decoding fails, the original bytes SHALL be stored unchanged and a warning logged.

#### Scenario: Stored image is in the configured format
- **WHEN** `image_format=webp` and a jpeg page is fetched through `/cache/img`
- **THEN** the stored file has the `.webp` extension and the response content-type is
  `image/webp`

#### Scenario: Format switch keeps old files servable
- **WHEN** `image_format` changes from webp to avif while old webp files exist
- **THEN** requests for existing keys serve the old webp files
- **AND** new fetches are stored as avif

#### Scenario: Undecodable input falls back to passthrough
- **WHEN** vips cannot decode the fetched bytes
- **THEN** the original bytes are stored as-is and a warning is logged
- **AND** the response serves normally

### Requirement: Cover normalization with host-computed ratio
Covers stored with `cover=1` SHALL be resized to fit within the configured max
dimension (ini knob, default 800px) preserving aspect ratio. The image ratio SHALL be
computed by the host from the decoded dimensions at store time — clients SHALL NOT
supply it — and persisted in a JSON sidecar next to the cover; the serve response
SHALL expose it via the `X-Image-Ratio` header. A plugin MAY provide a ratio hint in
metadata, used only when decoding fails; otherwise the ratio is omitted.

#### Scenario: Oversized cover is resized
- **WHEN** a cover of 2000x3000 is stored with cover=1 and max 800
- **THEN** the stored file is at most 800px on its longest side, aspect preserved
- **AND** the sidecar records the original-fitting dims and the serve response carries
  the `X-Image-Ratio` header

#### Scenario: Small cover passes through unresized
- **WHEN** a cover is already within the max dimension
- **THEN** only transcode applies, dimensions unchanged

#### Scenario: Ratio always comes from the host
- **WHEN** a client passes a `ratio` query param to `/cache/img`
- **THEN** the param is ignored and the stored sidecar ratio (host-computed) is served
